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

#include "pfl/cdefs.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/atomic.h"
#include "psc_util/iostats.h"
#include "psc_util/thread.h"
#include "psc_util/time.h"

#define psc_timercmp_addsec(tvp, s, uvp, cmp)				\
	(((tvp)->tv_sec + (s) == (uvp)->tv_sec) ?			\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec + (s) cmp (uvp)->tv_sec))


struct timeval psc_tiosthr_lastv[IST_NINTV];

void *
psc_tiosthr_main(__unusedx void *arg)
{
	struct psc_iostatv *istv;
	struct psc_iostats *ist;
	struct timeval tv;
	uint64_t intv_len;
	int i, stoff;

	for (;;) {
		PFL_GETTIME(&tv);
		usleep(1000000 - tv.tv_usec);
		PFL_GETTIME(&tv);

		/* find largest interval to update */
		for (stoff = 0; stoff < IST_NINTV; stoff++) {
			if (psc_timercmp_addsec(&psc_tiosthr_lastv[stoff],
			    psc_iostat_intvs[stoff], &tv, >))
				break;
			psc_tiosthr_lastv[stoff] = tv;
		}

		/* if we woke from signal, skip */
		if (stoff == 0)
			continue;

		PLL_LOCK(&psc_iostats);
		psclist_for_each_entry(ist,
		    &psc_iostats.pll_listhd, ist_lentry)
			for (i = 0; i < stoff; i++) {
				istv = &ist->ist_intv[i];

				/* reset counter to zero for this interval */
				intv_len = 0;
				intv_len = psc_atomic64_xchg(
				    &ist->ist_intv[i].istv_cur_len, intv_len);

				PFL_GETTIME(&tv);

				if (i == stoff - 1 && i < IST_NINTV - 1)
					psc_atomic64_add(&ist->ist_intv[i +
					    1].istv_cur_len, intv_len);

				ist->ist_intv[i].istv_intv_len = intv_len;

				/* calculate acculumation duration */
				timersub(&tv, &ist->ist_intv[i].istv_lastv,
				    &ist->ist_intv[i].istv_intv_dur);
				ist->ist_intv[i].istv_lastv = tv;

				if (i == 0)
					ist->ist_len_total += intv_len;
			}
		PLL_ULOCK(&psc_iostats);
	}
}

void
psc_tiosthr_spawn(int thrtype, const char *name)
{
	pscthr_init(thrtype, 0, psc_tiosthr_main, NULL, 0, name);
}
