/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2014-2015, Pittsburgh Supercomputing Center
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

#include "pfl/cdefs.h"
#include "pfl/listcache.h"
#include "pfl/pool.h"
#include "pfl/thread.h"
#include "pfl/workthr.h"

struct psc_poolmaster	 pfl_workrq_poolmaster;
struct psc_poolmgr	*pfl_workrq_pool;
struct psc_listcache	 pfl_workq;

void *
_pfl_workq_getitem(const char *typename, int (*cb)(void *), size_t len,
    int flags)
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
	wk->wkrq_type = typename;
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
pfl_workq_init(size_t bufsiz, int min, int total)
{
	_psc_poolmaster_init(&pfl_workrq_poolmaster,
	    sizeof(struct pfl_workrq) + bufsiz,
	    offsetof(struct pfl_workrq, wkrq_lentry), PPMF_AUTO, total,
	    min, 0, NULL, NULL, "workrq");
	pfl_workrq_pool = psc_poolmaster_getmgr(&pfl_workrq_poolmaster);
	lc_reginit(&pfl_workq, struct pfl_workrq, wkrq_lentry, "workq");
}

void
pfl_wkthr_spawn(int thrtype, int nthr, int extra, const char *thrname)
{
	struct psc_thread *thr;
	int i;

	for (i = 0; i < nthr; i++) {
		thr = pscthr_init(thrtype, pfl_wkthr_main,
		    sizeof(struct pfl_wk_thread)+extra, thrname, i);
		pfl_wkthr(thr)->wkt_workq = &pfl_workq;
		pscthr_setready(thr);
	}
}

void
pfl_wkthr_killall(void)
{
	lc_kill(&pfl_workq);
}
