/* $Id$ */

#include "psc_ds/types.h"
#include "psc_util/palloc.h"

struct psc_journal {
	int		pj_entsz;	/* sizeof log entry */
	int		pj_nents;	/* #ent slots in journal */
	daddr_t		pj_daddr;	/* disk offset of starting ent */
	int		pj_nextwrite;	/* slot to write next ent */
	int		pj_gen;		/* current wrap generation */
};

struct psc_journal_walker {
	int		pjw_pos;	/* current position */
	int		pjw_stop;	/* targetted end position */
	int		pjw_seen;	/* whether to terminate at stop_pos */
};

struct psc_journal_enthdr {
	u64		pje_magic;	/* validity check */
	u32		pje_gen;	/* log generation for wrapping */
	u32		pje_type;	/* app-specific log entry type */
};

#define PJE_MAGIC	0x45678912aabbccdd

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
	struct psc_journal_enthdr *pje;
	daddr_t addr;

	if (data & (pscPageSize - 1))
		pfatal("data is not page-aligned");

	pje = data;
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	addr = pj->pj_daddr + pj->pj_nextwrite * pj->pj_entsz;
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
