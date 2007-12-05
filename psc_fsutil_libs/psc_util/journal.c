/* $Id$ */

#include "psc_ds/types.h"
#include "psc_util/palloc.h"

struct psc_journal {
	psc_spinlock_t	pj_lock;	/* contention lock */
	int		pj_entsz;	/* sizeof log entry */
	int		pj_nents;	/* #ent slots in journal */
	daddr_t		pj_daddr;	/* disk offset of starting ent */
	int		pj_nextwrite;	/* next entry slot to write to */
	int		pj_genid;	/* current wrap generation */
	int		pj_nextxid;	/* next transaction ID */
};

struct psc_journal_walker {
	int		pjw_pos;	/* current position */
	int		pjw_stop;	/* targetted end position */
	int		pjw_seen;	/* whether to terminate at stop_pos */
};

struct psc_journal_enthdr {
	u64		pje_magic;	/* validity check */
	u32		pje_genid;	/* log generation for wrapping */
	u32		pje_type;	/* app-specific log entry type */
	u32		pje_xid;	/* transaction ID */
	/* record contents follow */
};

struct psc_journal_x {
	struct plisthead		 pjx_head;
	int				 pjx_nents;
};

struct psc_journal_xent {
	struct plistentry		 pjxe_entry;
	struct psc_journal_enthdr	*pjxe_pje;
};

#define PJ_LOCK(pj)	spinlock(&(pj)->pj_lock)
#define PJ_ULOCK(pj)	freelock(&(pj)->pj_lock)

#define PJE_MAGIC	0x45678912aabbccdd

/* Entry types. */
#define PJET_VOID	0
#define PJET_XSTART	(-1)
#define PJET_XEND	(-2)

void
pjournal_init(struct pjournal *pj, daddr_t start, int nents, int entsz)
{
	memset(pj, 0, sizeof(*pj));
	pj->pj_daddr = start;
	pj->pj_nents = nents;
	pj->pj_entsz = entsz;
	LOCK_INIT(&pj->pj_lock);
}

int
pjournal_xstart(struct pjournal *pj)
{
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	pje = palloc(pj->pj_entsz);
	_pjournal_logwrite(pj, slot, PJET_XSTART, pje);
	free(pje);
	return (0);
}

int
pjournal_xend(struct pjournal *pj, struct psc_journal_x *pjx)
{
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	pje = palloc(pj->pj_entsz);
	_pjournal_logwrite(pj, slot, PJET_XEND, pje);
	free(pje);
	return (0);
}

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
 * pjournal_logwrite - store a new entry in a journal.
 * @pj: the journal.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
int
pjournal_logwrite(struct psc_journal *pj, int type, const void *data)
{
	if (type == PJET_VOID ||
	    type == PJET_XSTART ||
	    type == PJET_XEND)
		pfatal("invalid journal entry type");

	rc = _pjournal_logwrite(pj, pj->pj_nextwrite, type, data);
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
 * _pjournal_logwrite - store a new entry in a journal.
 * @pj: the journal.
 * @type: the application-specific log entry type.
 * @data: the journal entry contents to store.
 * Returns: 0 on success, -1 on error.
 */
__static int
_pjournal_logwrite(struct psc_journal *pj, int slot, int type,
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
	addr = pj->pj_daddr + slot * pj->pj_entsz;
	ppio_write(addr, pje, pj->entsz);
	return (0);
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

/*
 * pjournal_walkx - traverse each transaction in a journal.
 * @pj: the journal.
 * @pjw: a walker for the journal.
 * @pje: an entry to be filled in for the next journal entry.
 * Returns: 0 on success, -1 on error, -2 on end of journal.
 */
int
pjournal_walkx(struct psc_journal *pj, struct psc_journal_walker *pjw,
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
