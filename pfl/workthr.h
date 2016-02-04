/* $Id$ */
/* %ISC_COPYRIGHT% */

#ifndef _PFL_WORKTHR_H_
#define _PFL_WORKTHR_H_

#include "pfl/list.h"
#include "pfl/listcache.h"

struct pfl_workrq {
	int				(*wkrq_cbf)(void *);
	struct psc_listentry		  wkrq_lentry;
};

struct pfl_wk_thread {
	struct psc_listcache		 *wkt_workq;
};

#define pfl_wkthr(thr)			((struct pfl_wk_thread *)(thr)->pscthr_private)

#define PFL_WKF_NONBLOCK		(1 << 0)

#define pfl_workq_getitem(cb, type)	_pfl_workq_getitem((cb), sizeof(type), 0)
#define pfl_workq_getitem_nb(cb, type)	_pfl_workq_getitem((cb), sizeof(type), PFL_WKF_NONBLOCK)

#define	pfl_workq_lock()		LIST_CACHE_LOCK(&pfl_workq)
#define	pfl_workq_unlock()		LIST_CACHE_LOCK(&pfl_workq)
#define	pfl_workq_waitempty()		psc_waitq_wait(&pfl_workq.plc_wq_want,	\
					    &pfl_workq.plc_lock)

void   pfl_wkthr_main(struct psc_thread *);
void   pfl_wkthr_spawn(int, int, const char *);
void *_pfl_workq_getitem(int (*)(void *), size_t, int);
void   pfl_workq_init(size_t);
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
