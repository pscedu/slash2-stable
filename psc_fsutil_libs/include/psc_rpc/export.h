/* $Id$ */

#ifndef _PFL_EXPORT_H_
#define _PFL_EXPORT_H_

#include "psc_rpc/rpc.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"

#define pscrpc_export_hldrop(e)					\
	do {							\
		if ((e)->exp_hldropf && (e)->exp_private)	\
			(e)->exp_hldropf((e)->exp_private);	\
		(e)->exp_private = NULL;			\
	} while (0)

void _pscrpc_export_put(struct pscrpc_export *);

static inline struct pscrpc_export *
pscrpc_export_get(struct pscrpc_export *exp)
{
	int rc;

	rc = atomic_inc_return(&exp->exp_refcount);
	psc_trace("GETting export %p : new refcount %d", exp, rc);
	return (exp);
}

static inline struct pscrpc_export *
pscrpc_export_rpc_get(struct pscrpc_export *exp)
{
	int rc;

	rc = atomic_inc_return(&exp->exp_rpc_count);
	psc_trace("RPC GETting export %p : new rpc_count %d", exp, rc);
	return (pscrpc_export_get(exp));
}

static inline void
pscrpc_export_put(struct pscrpc_export *exp)
{
	int rc;

	rc = atomic_read(&exp->exp_refcount);
	psc_trace("PUTting export %p : new refcount %d", exp, rc - 1);
	psc_assert(rc > 0);
	psc_assert(rc < 0x5a5a5a);
	_pscrpc_export_put(exp);
}

static inline void
pscrpc_export_rpc_put(struct pscrpc_export *exp)
{
	int rc;

	rc = atomic_dec_return(&exp->exp_rpc_count);
	psc_trace("RPC PUTting export %p : new rpc_count %d", exp, rc);
	pscrpc_export_put(exp);
}

#endif /* _PFL_EXPORT_H_ */
