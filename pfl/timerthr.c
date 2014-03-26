/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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
#include "pfl/lockedlist.h"
#include "pfl/time.h"
#include "pfl/atomic.h"
#include "pfl/iostats.h"
#include "pfl/thread.h"

#define psc_timercmp_addsec(tvp, s, uvp, cmp)				\
	(((tvp)->tv_sec + (s) == (uvp)->tv_sec) ?			\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec + (s) cmp (uvp)->tv_sec))

struct timeval psc_tiosthr_lastv[IST_NINTV];

void
psc_tiosthr_main(struct psc_thread *thr)
{
	struct psc_waitq dummy = PSC_WAITQ_INIT;
	struct psc_iostatv *istv;
	struct psc_iostats *ist;
	struct timespec ts;
	struct timeval dtv, tv;
	uint64_t intv_len;
	int i, stoff;

	PFL_GETTIMEVAL(&tv);
	tv.tv_usec = 0;

	ts.tv_nsec = 0;

	while (pscthr_run(thr)) {
		ts.tv_sec = ++tv.tv_sec;
		psc_waitq_waitabs(&dummy, NULL, &ts);

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
		PLL_FOREACH(ist, &psc_iostats)
			for (i = 0; i < stoff; i++) {
				istv = &ist->ist_intv[i];

				/* reset counter to zero for this interval */
				intv_len = 0;
				intv_len = psc_atomic64_xchg(
				    &istv->istv_cur_len, intv_len);

				PFL_GETTIMEVAL(&dtv);

				if (i == stoff - 1 && i < IST_NINTV - 1)
					psc_atomic64_add(&ist->ist_intv[i +
					    1].istv_cur_len, intv_len);

				istv->istv_intv_len = intv_len;

				/* calculate acculumation duration */
				timersub(&dtv, &istv->istv_lastv,
				    &istv->istv_intv_dur);
				istv->istv_lastv = dtv;

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
