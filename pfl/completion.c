/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

/*
 * Completion - Barrier-like API for thread(s) to wait on an arbitrary
 * event.
 */

#include "pfl/completion.h"

void
psc_compl_init(struct psc_compl *pc)
{
	memset(pc, 0, sizeof(*pc));
	INIT_SPINLOCK(&pc->pc_lock);
	psc_waitq_init(&pc->pc_wq, "completion");
}

void
psc_compl_destroy(struct psc_compl *pc)
{
	spinlock(&pc->pc_lock);
	psc_waitq_destroy(&pc->pc_wq);
	freelock(&pc->pc_lock);
}

void
_psc_compl_ready(struct psc_compl *pc, int rc, int one)
{
	spinlock(&pc->pc_lock);
	if (one)
		psc_waitq_wakeone(&pc->pc_wq);
	else {
		pc->pc_rc = rc;
		pc->pc_done = 1;
		psc_waitq_wakeall(&pc->pc_wq);
	}
	pc->pc_counter++;
	freelock(&pc->pc_lock);
}

int
psc_compl_waitrel(struct psc_compl *pc, enum pfl_lockprim type,
    void *lockp, long sec, long nsec)
{
	reqlock(&pc->pc_lock);

	PFL_LOCKPRIM_ULOCK(type, lockp);

	if (pc->pc_done) {
		freelock(&pc->pc_lock);
	} else {
		if (sec || nsec) {
			if (psc_waitq_waitrel(&pc->pc_wq, &pc->pc_lock,
			    sec, nsec))
				return (0);
		} else
			psc_waitq_wait(&pc->pc_wq, &pc->pc_lock);
	}
	return (pc->pc_rc);
}
