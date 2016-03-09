/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
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
