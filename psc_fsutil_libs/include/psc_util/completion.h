/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_COMPLETION_H_
#define _PFL_COMPLETION_H_

#include "psc_util/lock.h"
#include "psc_util/waitq.h"

#define PCV_DONE	42
#define PCV_NOTDONE	43

struct psc_completion {
	struct psc_waitq	pc_wq;
	psc_spinlock_t		pc_lock;	/* XXX use the waitq mutex? */
	int			pc_done;
	int			pc_rc;
};

#define PSC_COMPLETION_INIT	{ PSC_WAITQ_INIT, SPINLOCK_INIT, PCV_NOTDONE, 0 }

static __inline void
psc_completion_init(struct psc_completion *pc)
{
	pc->pc_done = PCV_NOTDONE;
	psc_waitq_init(&pc->pc_wq);
	INIT_SPINLOCK(&pc->pc_lock);
	pc->pc_rc = 1;
}

static __inline int
psc_completion_wait(struct psc_completion *pc)
{
	if (pc->pc_done != PCV_DONE) {
		(void)reqlock(&pc->pc_lock);
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
	(void)reqlock(&pc->pc_lock);
	pc->pc_rc = rc;
	psc_assert(pc->pc_done == PCV_NOTDONE);
	pc->pc_done = PCV_DONE;
	psc_waitq_wakeall(&pc->pc_wq);
	freelock(&pc->pc_lock);
}

#endif /* _PFL_COMPLETION_H_ */
