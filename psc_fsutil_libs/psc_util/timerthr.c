/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>

#include "psc_ds/lockedlist.h"
#include "psc_util/atomic.h"
#include "pfl/cdefs.h"
#include "psc_util/iostats.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

struct psc_waitq psc_timerwtq;

__static void *
psc_timerthr_main(__unusedx void *arg)
{
	for (;;) {
		sleep(1);
		psc_waitq_wakeall(&psc_timerwtq);
		/* XXX subtract the time this took from next sleep */
	}
}

void *
psc_timer_iosthr_main(__unusedx void *arg)
{
	struct iostats *ist;
	struct timeval tv;
	unsigned long intv;

	for (;;) {
		psc_waitq_wait(&psc_timerwtq, NULL);

		PLL_LOCK(&psc_iostats);
		if (gettimeofday(&tv, NULL) == -1)
			psc_fatal("gettimeofday");
		psclist_for_each_entry(ist,
		    &psc_iostats.pll_listhd, ist_lentry) {
			if (tv.tv_sec != ist->ist_lasttv.tv_sec) {
				timersub(&tv, &ist->ist_lasttv,
				    &ist->ist_intv);
				ist->ist_lasttv = tv;

				intv = 0;
				intv = atomic_xchg(&ist->ist_bytes_intv, intv);
				ist->ist_rate = intv /
				    ((ist->ist_intv.tv_sec * UINT64_C(1000000) +
				    ist->ist_intv.tv_usec) * 1e-6);
				ist->ist_bytes_total += intv;

				intv = 0;
				intv = atomic_xchg(&ist->ist_errors_intv, intv);
				ist->ist_erate = intv /
				    ((ist->ist_intv.tv_sec * UINT64_C(1000000) +
				    ist->ist_intv.tv_usec) * 1e-6);
				ist->ist_errors_total += intv;
			}
		}
		PLL_ULOCK(&psc_iostats);
		sched_yield();
	}
}

void
psc_timerthr_spawn(int thrtype, const char *name)
{
	psc_waitq_init(&psc_timerwtq);
	pscthr_init(thrtype, 0, psc_timerthr_main, NULL, 0, name);
}
