/* $Id$ */
/*
 * %PSCGPL_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License contained in the file
 * `COPYING-GPL' at the top of this distribution or at
 * https://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include "pfl/cdefs.h"
#include "pfl/listcache.h"
#include "pfl/pool.h"
#include "pfl/thread.h"
#include "pfl/workthr.h"

struct psc_poolmaster	 pfl_workrq_poolmaster;
struct psc_poolmgr	*pfl_workrq_pool;
struct psc_listcache	 pfl_workq;

void *
_pfl_workq_getitem(int (*cb)(void *), size_t len, int flags)
{
	struct pfl_workrq *wk;
	void *p;

	psc_assert(len <= pfl_workrq_pool->ppm_entsize - sizeof(*wk));
	if (flags & PFL_WKF_NONBLOCK) {
		wk = psc_pool_tryget(pfl_workrq_pool);
		if (wk == NULL)
			return (NULL);
	} else
		wk = psc_pool_get(pfl_workrq_pool);
	wk->wkrq_cbf = cb;
	p = PSC_AGP(wk, sizeof(*wk));
	memset(p, 0, len);
	return (p);
}

void
_pfl_workq_putitemq(struct psc_listcache *lc, void *p, int tails)
{
	struct pfl_workrq *wk;

	psc_assert(p);
	wk = PSC_AGP(p, -sizeof(*wk));
	psclog_debug("placing work %p on queue %p", wk, lc);
	if (tails)
		lc_addtail(lc, wk);
	else
		lc_addhead(lc, wk);
}

void
pfl_wkthr_main(struct psc_thread *thr)
{
	struct psc_listcache *lc;
	struct pfl_workrq *wkrq;
	void *p;

	lc = pfl_wkthr(thr)->wkt_workq;
	while (pscthr_run(thr)) {
		wkrq = lc_getwait(lc);
		if (wkrq == NULL)
			break;
		p = PSC_AGP(wkrq, sizeof(*wkrq));
		if (wkrq->wkrq_cbf(p)) {
			LIST_CACHE_LOCK(lc);
			lc_addtail(lc, wkrq);
			if (lc_nitems(lc) == 1)
				psc_waitq_waitrel_us(&lc->plc_wq_empty,
				    &lc->plc_lock, 1);
			else
				LIST_CACHE_ULOCK(lc);
		} else
			psc_pool_return(pfl_workrq_pool, wkrq);
	}
}

void
pfl_workq_init(size_t bufsiz)
{
	_psc_poolmaster_init(&pfl_workrq_poolmaster,
	    sizeof(struct pfl_workrq) + bufsiz,
	    offsetof(struct pfl_workrq, wkrq_lentry), PPMF_AUTO, 128,
	    128, 0, NULL, NULL, NULL, NULL, "workrq");
	pfl_workrq_pool = psc_poolmaster_getmgr(&pfl_workrq_poolmaster);
	lc_reginit(&pfl_workq, struct pfl_workrq, wkrq_lentry, "workq");
}

void
pfl_wkthr_spawn(int thrtype, int nthr, const char *thrname)
{
	struct psc_thread *thr;
	int i;

	for (i = 0; i < nthr; i++) {
		thr = pscthr_init(thrtype, pfl_wkthr_main, NULL,
		    sizeof(struct pfl_wk_thread), thrname, i);
		pfl_wkthr(thr)->wkt_workq = &pfl_workq;
		pscthr_setready(thr);
	}
}

void
pfl_wkthr_killall(void)
{
	lc_kill(&pfl_workq);
}
