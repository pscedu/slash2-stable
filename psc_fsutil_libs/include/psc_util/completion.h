/* $Id$ */

#ifndef _PFL_COMPLETION_H_
#define _PFL_COMPLETION_H_

#define PCV_DONE	42
#define PCV_NOTDONE	43

struct psc_completion {
	struct psc_waitq	pc_wq;
	psc_spinlock_t		pc_lock;
	int			pc_done;
	int			pc_rc;
};

#define PSC_COMPLETION_INIT	{ PSC_WAITQ_INIT, LOCK_INITIALIZER, PCV_NOTDONE, 0 }

static __inline void
psc_completion_init(struct psc_completion *pc)
{
	pc->pc_done = PCV_NOTDONE;
	psc_waitq_init(&pc->pc_wq);
	LOCK_INIT(&pc->pc_lock);
	pc->pc_rc = 1;
}

static __inline int
psc_completion_wait(struct psc_completion *pc)
{
	if (pc->pc_done != PCV_DONE) {
		reqlock(&pc->pc_lock);
		if (pc->pc_done == PCV_NOTDONE) {
			psc_waitq_wait(&pc->pc_wq, &pc->pc_lock);
			spinlock(&pc->pc_lock);
			psc_assert(pc->pc_done == PCV_DONE);
		}
		freelock(&pc->pc_lock);
	}
	return (pc->pc_rc);
}

static __inline void
psc_completion_done(struct psc_completion *pc, int rc)
{
	reqlock(&pc->pc_lock);
	pc->pc_rc = rc;
	psc_assert(pc->pc_done == PCV_NOTDONE);
	pc->pc_done = PCV_DONE;
	psc_waitq_wakeall(&pc->pc_wq);
	freelock(&pc->pc_lock);
}

#endif /* _PFL_COMPLETION_H_ */
