/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2012, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Completion - Barrier-like API for thread(s) to wait on an arbitrary event.
 */

#include "psc_util/completion.h"

void
psc_compl_init(struct psc_compl *pc)
{
	memset(pc, 0, sizeof(*pc));
	INIT_SPINLOCK(&pc->pc_lock);
	psc_waitq_init(&pc->pc_wq);
	pc->pc_rc = 1;
}

void
psc_compl_destroy(struct psc_compl *pc)
{
	psc_waitq_destroy(&pc->pc_wq);
}

void
_psc_compl_ready(struct psc_compl *pc, int rc, int one)
{
	spinlock(&pc->pc_lock);
	if (rc)
		pc->pc_rc = rc;
	if (one)
		psc_waitq_wakeone(&pc->pc_wq);
	else {
		pc->pc_done = 1;
		psc_waitq_wakeall(&pc->pc_wq);
	}
	freelock(&pc->pc_lock);
}

int
psc_compl_waitrel_s(struct psc_compl *pc, int secs)
{
	spinlock(&pc->pc_lock);
	if (!pc->pc_done) {
		if (secs) {
			if (psc_waitq_waitrel_s(&pc->pc_wq,
			    &pc->pc_lock, secs))
				return (0);
		} else
			psc_waitq_wait(&pc->pc_wq, &pc->pc_lock);
	} else
		freelock(&pc->pc_lock);
	return (pc->pc_rc);
}
