/* $Id$ */

#include "psc_ds/types.h"
#include "psc_util/palloc.h"

#define PJ_LOCK(pj)	spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)	freelock(&(pj)->pj_lock)

#define PJE_MAGIC	0x45678912aabbccdd
#define PJE_XID_NONE	0		/* invalid transaction ID */

/*
 * pjournal_init - initialize the in-memory representation of a journal.
 * @pj: the journal.
 * @start: disk address of beginning of journal.
 * @nents: number of entries journal can store before wrapping.
 * @entsz: size of a journal entry.
 */
void
pjournal_init(struct pjournal *pj, daddr_t start, int nents, int entsz)
{
	memset(pj, 0, sizeof(*pj));
	pj->pj_daddr = start;
	pj->pj_nents = nents;
	pj->pj_entsz = entsz;
	LOCK_INIT(&pj->pj_lock);
	atomic_init(&pj->pj_nextxid, 0);
}

/*
 * pjournal_nextxid - obtain an unused journal transaction ID.
 * @pj: the journal.
 * Returns: new, unused transaction ID.
 */
int
pjournal_nextxid(struct pjournal *pj)
{
	int xid;

	do {
		xid = atomic_inc_return(&pj->pj_nextxid);
	} while (xid == PJE_XID_NONE);
	return (xid);
}

/*
 * pjournal_xend - write a "transaction began" record in a journal.
 * @pj: the journal.
 * @xid: ID of initiated transaction.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_xstart(struct pjournal *pj, int xid)
{
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	pje = palloc(pj->pj_entsz);
	pje->pje_xid = xid;
	_pjournal_logwrite(pj, slot, PJET_XSTART, pje);
	free(pje);
	return (0);
}

/*
 * pjournal_xend - write a "transaction finished" record in a journal.
 * @pj: the journal.
 * @xid: ID of finished transaction.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_xend(struct pjournal *pj, int xid)
{
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	pje = palloc(pj->pj_entsz);
	pje->pje_xid = xid;
	_pjournal_logwrite(pj, slot, PJET_XEND, pje);
	free(pje);
	return (0);
}

/*
 * pjournal_clearlog - invalidate a journal entry.
 * @pj: the journal.
 * @slot: position location in journal to invalidate.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_clearlog(struct psc_journal *pj, int slot)
{
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	pje = palloc(pj->pj_entsz);
	_pjournal_logwrite(pj, slot, PJET_VOID, pje);
	free(pje);
	return (0);
}

/*
 * pjournal_alloclog - allocate a log for I/O to a journal.
 * @pj: the journal.
 * Returns: data buffer pointer valid for journal I/O.
 */
void *
pjournal_alloclog(struct psc_journal *pj)
{
	return (palloc(pj->pj_entsz));
}

/*
 * _pjournal_logwrite - store a new entry in a journal.
 * @pj: the journal.
 * @slot: position location in journal to write.
 * @type: the application-specific log entry type.
 * @xid: transaction ID of entry.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
__static int
_pjournal_logwrite(struct psc_journal *pj, int slot, int type, int xid,
    const void *data)
{
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	if (slot >= pj->pj_nents)
		return (-1);

	if (data & (pscPageSize - 1))
		pfatal("data is not page-aligned");

	pje = data;
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	pje->pje_xid = xid;
	addr = pj->pj_daddr + slot * pj->pj_entsz;
	ppio_write(addr, pje, pj->entsz);
	return (0);
}

/*
 * pjournal_logwritex - store a new entry in a journal transaction.
 * @pj: the journal.
 * @type: the application-specific log entry type.
 * @xid: transaction ID.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_logwritex(struct psc_journal *pj, int type, int xid,
    const void *data)
{
	if (type == PJET_VOID ||
	    type == PJET_XSTART ||
	    type == PJET_XEND)
		pfatal("invalid journal entry type");

	rc = _pjournal_logwrite(pj, pj->pj_nextwrite, type, xid, data);
	if (rc)
		return (rc);

	PJ_LOCK(pj);
	if (++pj->pj_nextwrite >= pj->pj_nents) {
		pj->pj_nextwrite = 0;
		pj->pj_xid = 0;
		pj->pj_genid++;
	}
	PJ_ULOCK(pj);
	return (rc);
}

/*
 * pjournal_logwrite - store a new entry in a journal.
 * @pj: the journal.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_logwrite(struct psc_journal *pj, int type, const void *data)
{
	return (_pjournal_logwrite(pj, pj->pj_nextwrite, type, data,
	    PJE_XID_NONE));
}

/*
 * pjournal_logread - get a specified entry from a journal.
 * @pj: the journal.
 * @slot: the position in the journal of the entry to obtain.
 * @pje: an entry to be filled in for the journal entry.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_logread(struct psc_journal *pj, int slot, void *data)
{
	daddr_t addr;

	if (data & (pscPageSize - 1))
		pfatal("data is not page-aligned");

	if (slot >= pj->pj_nents)
		return (-1);

	addr = pj->pj_daddr + slot * pj->pj_entsz;
	ppio_read(addr, data, pj->entsz);
	return (0);
}

/*
 * pjournal_walk - traverse each entry in a journal.
 * @pj: the journal.
 * @pjw: a walker for the journal.
 * @pje: an entry to be filled in for the next journal entry.
 * Returns: 0 on success, -1 on error, -2 on end of journal.
 */
int
pjournal_walk(struct psc_journal *pj, struct psc_journal_walker *pjw,
    struct psc_journal_entry *pje)
{
	daddr_t addr;

	if (pjw->pjw_pos == pjw->pjw_stop) {
		if (pjw->pjw_seen)
			return (-2);
		pjw->pjw_seen = 1;
	}
	pjournal_logread(pj, pjw->pjw_pos, pje);
	if (++pjw->pjw_pos >= pj->pj_nents)
		pjw->pjw_pos = 0;
	return (0);
}
