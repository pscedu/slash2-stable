/* $Id: zestExport.h 2010 2007-10-28 19:46:36Z pauln $ */
#include "psc_util/atomic.h"
#include "psc_util/log.h"

#define pscrpc_export_rpc_get(exp)                                       \
	({								\
		atomic_inc(&(exp)->exp_rpc_count);			\
		psc_trace("RPC GETting export %p : new rpc_count %d\n",	\
			(exp), atomic_read(&(exp)->exp_rpc_count));	\
		pscrpc_export_get(exp);					\
	})

#define pscrpc_export_rpc_put(exp)                                       \
	({								\
		atomic_dec(&(exp)->exp_rpc_count);			\
		psc_trace("RPC PUTting export %p : new rpc_count %d\n",	\
			(exp), atomic_read(&(exp)->exp_rpc_count));	\
		pscrpc_export_put(exp);					\
	})

#define pscrpc_export_get(exp)						\
	({								\
		struct pscrpc_export *exp_ = exp;			\
		atomic_inc(&exp_->exp_refcount);			\
		psc_trace("GETting export %p : new refcount %d\n", exp_,	\
			atomic_read(&exp_->exp_refcount));		\
		exp_;							\
	})

#define pscrpc_export_put(exp)						\
	do {								\
		LASSERT((exp) != NULL);					\
		psc_trace("PUTting export %p : new refcount %d\n", (exp),	\
			atomic_read(&(exp)->exp_refcount) - 1);		\
		LASSERT(atomic_read(&(exp)->exp_refcount) > 0);		\
		LASSERT(atomic_read(&(exp)->exp_refcount) < 0x5a5a5a);	\
		__pscrpc_export_put(exp);				\
	} while (0)


struct pscrpc_export;
extern void 
__pscrpc_export_put(struct pscrpc_export *exp);
