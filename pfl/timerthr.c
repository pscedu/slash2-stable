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

#define psc_timercmp_addsec(tvp, s, uvp, cmp)				\
	(((tvp)->tv_sec + (s) == (uvp)->tv_sec) ?			\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec + (s) cmp (uvp)->tv_sec))

struct timeval psc_tiosthr_lastv[IST_NINTV];

void
psc_tiosthr_main(struct psc_thread *thr)
{
	struct psc_waitq dummy = PSC_WAITQ_INIT;
	struct pfl_iostatv *istv;
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
	pscthr_init(thrtype, psc_tiosthr_main, NULL, 0, name);
}
