/* $Id$ */

#include "psc_ds/types.h"

struct psc_journal {
	int		pj_entsz;	/* sizeof log entry */
	int		pj_nent;	/* #ent slots in journal */
	daddr_t		pj_daddr;	/* disk offset of starting ent */
	int		pj_nextwrite;	/* slot to write next ent */
	int		pj_gen;		/* current wrap generation */
};

struct psc_journal_walker {
	int		pjw_pos;
};

struct psc_journal_enthdr {
	u64		pje_magic;	/* validity check */
	u32		pje_gen;	/* log generation for wrapping */
	u32		pje_type;	/* app-specific log entry type */
};

#define PJE_MAGIC	0x45678912aabbccdd

void *
pjournal_alloclog(struct psc_journal *pj)
{
	return (palloc(pj->pj_entsz));
}

int
pjournal_logwrite(struct psc_journal *pj, int type, const void *data)
{
	struct psc_journal_enthdr *pje;

	pje = data;
	pje->pje_magic = PJE_MAGIC;
	pje->pje_type = type;
	ppio_write(pj->nextlog, pje, pj->entsz);
	return (0);
}

int
pjournal_walk_start(struct psc_journal *pj, struct psc_journal_walker *pjw,
    struct psc_journal_entry *pje)
{
	memset(pjw, 0, sizeof(*pjw));
	return (pjournal_walk(pj, pjw, pje));
}

int
pjournal_walk(struct psc_journal *pj, struct psc_journal_walker *pjw,
    struct psc_journal_entry *pje)
{
}



#define SLASH_INUM_ALLOC_SZ	1024	/* allocate 1024 inums at a time */

#define SLASH_PJET_VOID		0
#define SLASH_PJET_INUM		1

struct slash_jent_inum {
	struct psc_journal_enthdr	sji_hdr;
	slash_inum_t			sji_inum;
};

slash_inum_t
slash_get_inum(struct slash_sb_mem *sbm)
{
	struct slash_jent_inum *sji;

	if (++sbm->sbm_inum % INUM_ALLOC_SZ == 0) {
		sji = pjournal_alloclog(sbm->sbm_pj);
		sji->sgi_inum = sbm->sbm_inum;
		pjournal_writelog(sbm->sbm_pj, SLASH_PJET_INUM, sji);
		free(sji);
	}
	return (sbm->sbm_inum);
}
