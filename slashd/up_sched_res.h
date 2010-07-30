/* $Id$ */

#ifndef _UP_SCHED_RES_H_
#define _UP_SCHED_RES_H_

struct up_sched_work_item {
	struct fidc_membh		*uswi_fcmh;
	psc_atomic32_t			 uswi_refcnt;
	int				 uswi_gen;
	int				 uswi_flags;
	pthread_mutex_t			 uswi_mutex;
	struct psc_multiwaitcond	 uswi_mwcond;
	struct psclist_head		 uswi_lentry;
	SPLAY_ENTRY(up_sched_work_item)	 uswi_tentry;
};

/* work item flags */
#define USWIF_BUSY		(1 << 0)	/* work item is being modified */
#define USWIF_DIE		(1 << 1)	/* work item is going away */

#define USWI_INOH(wk)		fcmh_2_inoh((wk)->uswi_fcmh)
#define USWI_INO(wk)		(&USWI_INOH(wk)->inoh_ino)
#define USWI_INOX(wk)		USWI_INOH(wk)->inoh_extras
#define USWI_NREPLS(wk)		USWI_INO(wk)->ino_nrepls
#define USWI_FG(wk)		(&(wk)->uswi_fcmh->fcmh_fg)
#define USWI_FID(wk)		USWI_FG(wk)->fg_fid
#define USWI_NBMAPS(wk)		fcmh_2_nbmaps((wk)->uswi_fcmh)

#define USWI_GETREPL(wk, n)	((n) < SL_DEF_REPLICAS ?		\
				    USWI_INO(wk)->ino_repls[n] :	\
				    USWI_INOX(wk)->inox_repls[(n) - 1])

/* uswi_init() flags */
#define USWI_INITF_NOPERSIST	(1 << 0)	/* do not link in .slussr */

#define uswi_init(wk, fid)	uswi_initf((wk), (fid), 0)

struct up_sched_work_item *
	 uswi_find(const struct slash_fidgen *, int *);
int	 uswi_access(struct up_sched_work_item *);
int	 uswi_cmp(const void *, const void *);
int	 uswi_initf(struct up_sched_work_item *, slfid_t, int);
void	 uswi_kill(struct up_sched_work_item *);
void	 uswi_unref(struct up_sched_work_item *);

void	 upsched_scandir(void);

SPLAY_HEAD(upschedtree, up_sched_work_item);
SPLAY_PROTOTYPE(upschedtree, up_sched_work_item, uswi_tentry, uswi_cmp);

#define UPSCHED_MGR_LOCK()		PLL_LOCK(&upsched_listhd)
#define UPSCHED_MGR_UNLOCK()		PLL_ULOCK(&upsched_listhd)
#define UPSCHED_MGR_RLOCK()		PLL_RLOCK(&upsched_listhd)
#define UPSCHED_MGR_URLOCK(lk)		PLL_URLOCK(&upsched_listhd, (lk))
#define UPSCHED_MGR_ENSURE_LOCKED()	PLL_ENSURE_LOCKED(&upsched_listhd)

extern struct psc_poolmgr	*upsched_pool;
extern struct upschedtree	 upsched_tree;
extern struct psc_lockedlist	 upsched_listhd;

#endif /* _UP_SCHED_RES_H_ */
