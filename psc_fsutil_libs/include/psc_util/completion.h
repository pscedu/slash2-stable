/* $Id$ */

struct psc_completion {
	struct psc_waitq	pc_wq;
	psc_spinlock_t		pc_lock;
	int			pc_done;
};

static inline void
psc_completion_init(struct psc_completion *pc)
{
	memset(pc, 0, sizeof(*pc));
	psc_waitq_init(&pc->pc_wq);
	LOCK_INIT(&pc->pc_lock);
}

static inline void
psc_completion_wait(struct psc_completion *pc)
{
	spinlock(&pc->pc_lock);
	if (!pc->pc_done)
		psc_waitq_wait(&pc->pc_wq, &pc->pc_lock);
	else
		freelock(&pc->pc_lock);
}

static inline void
psc_completion_done(struct psc_completion *pc)
{
	spinlock(&pc->pc_lock);
	pc->pc_done = 1;
	psc_waitq_wakeall(&pc->pc_wq);
	freelock(&pc->pc_lock);
}
