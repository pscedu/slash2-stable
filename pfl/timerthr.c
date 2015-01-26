/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>

#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/iostats.h"
#include "pfl/lockedlist.h"
#include "pfl/thread.h"
#include "pfl/time.h"

/*
 * Logic for recomputing instantaneous and weighted averages for opstats.
 * The theory is, under normal operation, the number of opstats will be
 * constant; so unless CPU is scarce, the opstats will be calculated at
 * the same time, thus avoiding syscalls to get clock time used to
 * measure rates.
 */
void
pfl_opstimerthr_main(struct psc_thread *thr)
{
	struct psc_waitq dummy = PSC_WAITQ_INIT;
	struct pfl_opstat *opst;
	struct timespec ts;
	uint64_t len;
	int i;

	PFL_GETTIMESPEC(&ts);
	ts.tv_nsec = 0;

	while (pscthr_run(thr)) {
		ts.tv_sec++;
		psc_waitq_waitabs(&dummy, NULL, &ts);

		spinlock(&pfl_opstats_lock);
		DYNARRAY_FOREACH(opst, i, &pfl_opstats) {
			/* reset instantaneous interval counter to zero */
			len = 0;
			len = psc_atomic64_xchg(&opst->opst_intv, len);

			/* update last second rate */
			opst->opst_last = len;

			/* compute time weighted average */
			opst->opst_avg = 9 * opst->opst_avg / 10 +
			    len / 10;

			/* add to lifetime */
			opst->opst_lifetime += len;
		}
		spinlock(&pfl_opstats_lock);
	}
}

void
pfl_opstimerthr_spawn(int thrtype, const char *name)
{
	pscthr_init(thrtype, pfl_opstimerthr_main, NULL, 0, name);
}
