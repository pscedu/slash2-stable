/* $Id$ */

struct psc_journal {
	psc_spinlock_t	pj_lock;	/* contention lock */
	int		pj_entsz;	/* sizeof log entry */
	int		pj_nents;	/* #ent slots in journal */
	daddr_t		pj_daddr;	/* disk offset of starting ent */
	int		pj_nextwrite;	/* next entry slot to write to */
	int		pj_genid;	/* current wrap generation */
	atomic_t	pj_nextxid;	/* next transaction ID */
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

/* Journal entry types. */
#define PJET_VOID	0		/* null journal record */
#define PJET_XSTART	(-1)		/* transaction began */
#define PJET_XEND	(-2)		/* transaction ended */

void	 pjournal_init(struct pjournal *, daddr_t, int, int);
int	 pjournal_nextxid(struct pjournal *);
int	 pjournal_xstart(struct pjournal *, int);
int	 pjournal_xend(struct pjournal *, int);
int	 pjournal_clearlog(struct psc_journal *, int);
void	*pjournal_alloclog(struct psc_journal *);
int	 pjournal_logwritex(struct psc_journal *, int, int, const void *);
int	 pjournal_logwrite(struct psc_journal *, int, const void *);
int	 pjournal_logread(struct psc_journal *, int, void *);
int	 pjournal_walk(struct psc_journal *, struct psc_journal_walker *,
	    struct psc_journal_entry *);
