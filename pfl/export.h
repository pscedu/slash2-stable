/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_EXPORT_H_
#define _PFL_EXPORT_H_

#include "pfl/atomic.h"
#include "pfl/log.h"
#include "pfl/rpc.h"

#define pscrpc_export_hldrop(e)					\
	do {							\
		if ((e)->exp_hldropf)				\
			(e)->exp_hldropf(e);			\
		psc_assert((e)->exp_private == NULL);		\
	} while (0)

#define EXPORT_LOCK(e)		spinlock(&(e)->exp_lock)
#define EXPORT_RLOCK(e)		reqlock(&(e)->exp_lock)
#define EXPORT_ULOCK(e)		freelock(&(e)->exp_lock)
#define EXPORT_URLOCK(e, lk)	ureqlock(&(e)->exp_lock, (lk))

#define PFLOG_EXP(level, exp, fmt, ...)				\
	psclog((level), "export@%p ref=%d rpccnt=%d:: " fmt,	\
	    (exp),						\
	    atomic_read(&(exp)->exp_refcount),			\
	    atomic_read(&(exp)->exp_rpc_count),			\
	    ##__VA_ARGS__)

void _pscrpc_export_put(struct pscrpc_export *);

static __inline struct pscrpc_export *
pscrpc_export_get(struct pscrpc_export *exp)
{
	atomic_inc_return(&exp->exp_refcount);
	PFLOG_EXP(PLL_DEBUG, exp, "incr refcount");
	return (exp);
}

static __inline struct pscrpc_export *
pscrpc_export_rpc_get(struct pscrpc_export *exp)
{
	atomic_inc_return(&exp->exp_rpc_count);
	PFLOG_EXP(PLL_DEBUG, exp, "incr rpc_count");
	return (pscrpc_export_get(exp));
}

static __inline void
pscrpc_export_put(struct pscrpc_export *exp)
{
	int rc;

	rc = atomic_read(&exp->exp_refcount);
	psc_assert(rc > 0);
	psc_assert(rc < 0x5a5a5a);
	PFLOG_EXP(PLL_DEBUG, exp, "decr refcount");
	_pscrpc_export_put(exp);
}

static __inline void
pscrpc_export_rpc_put(struct pscrpc_export *exp)
{
	atomic_dec_return(&exp->exp_rpc_count);
	PFLOG_EXP(PLL_DEBUG, exp, "decr rpc_count");
	pscrpc_export_put(exp);
}

#endif /* _PFL_EXPORT_H_ */
