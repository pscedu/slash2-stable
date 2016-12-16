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

#ifndef _PFL_WORKTHR_H_
#define _PFL_WORKTHR_H_

#include "pfl/list.h"
#include "pfl/listcache.h"

struct pfl_workrq {
	int				(*wkrq_cbf)(void *);
	const char 			 *wkrq_type;
	struct psc_listentry		  wkrq_lentry;
};

struct pfl_wk_thread {
	struct psc_listcache		 *wkt_workq;
};

#define pfl_wkthr(thr)			((struct pfl_wk_thread *)(thr)->pscthr_private)

#define PFL_WKF_NONBLOCK		(1 << 0)

#define pfl_workq_getitem(cb, type)	_pfl_workq_getitem(#type, (cb), sizeof(type), 0)
#define pfl_workq_getitem_nb(cb, type)	_pfl_workq_getitem(#type, (cb), sizeof(type), PFL_WKF_NONBLOCK)

#define	pfl_workq_lock()		LIST_CACHE_LOCK(&pfl_workq)
#define	pfl_workq_unlock()		LIST_CACHE_LOCK(&pfl_workq)
#define	pfl_workq_waitempty()		psc_waitq_wait(&pfl_workq.plc_wq_want,	\
					    &pfl_workq.plc_lock)

void   pfl_wkthr_main(struct psc_thread *);
void   pfl_wkthr_spawn(int, int, int, const char *);
void *_pfl_workq_getitem(const char *, int (*)(void *), size_t, int);
void   pfl_workq_init(size_t, int, int);
void  _pfl_workq_putitemq(struct psc_listcache *, void *, int);
void   pfl_wkthr_killall(void);

#define _pfl_workq_putitem(p, tail)	_pfl_workq_putitemq(&pfl_workq, (p), (tail))
#define  pfl_workq_putitem_head(p)	_pfl_workq_putitem((p), 0)
#define  pfl_workq_putitem_tail(p)	_pfl_workq_putitem((p), 1)
#define  pfl_workq_putitem(p)		_pfl_workq_putitem((p), 1)
#define  pfl_workq_putitemq(lc, p)	_pfl_workq_putitemq((lc), (p), 1)
#define  pfl_workq_putitemq_head(lc, p)	_pfl_workq_putitemq((lc), (p), 0)

extern struct psc_listcache		 pfl_workq;
extern struct psc_poolmgr		*pfl_workrq_pool;

#endif /* _PFL_WORKTHR_H_ */
