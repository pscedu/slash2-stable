/* $Id: zestExport.h 2010 2007-10-28 19:46:36Z pauln $ */
#include "zestAtomic.h"
#include "zestLog.h"

#define zclass_export_rpc_get(exp)                                       \
	({								\
		atomic_inc(&(exp)->exp_rpc_count);			\
		ztrace("RPC GETting export %p : new rpc_count %d\n",	\
			(exp), atomic_read(&(exp)->exp_rpc_count));	\
		zclass_export_get(exp);					\
	})

#define zclass_export_rpc_put(exp)                                       \
	({								\
		atomic_dec(&(exp)->exp_rpc_count);			\
		ztrace("RPC PUTting export %p : new rpc_count %d\n",	\
			(exp), atomic_read(&(exp)->exp_rpc_count));	\
		zclass_export_put(exp);					\
	})

#define zclass_export_get(exp)						\
	({								\
		struct zestrpc_export *exp_ = exp;			\
		atomic_inc(&exp_->exp_refcount);			\
		ztrace("GETting export %p : new refcount %d\n", exp_,	\
			atomic_read(&exp_->exp_refcount));		\
		exp_;							\
	})

#define zclass_export_put(exp)						\
	do {								\
		LASSERT((exp) != NULL);					\
		ztrace("PUTting export %p : new refcount %d\n", (exp),	\
			atomic_read(&(exp)->exp_refcount) - 1);		\
		LASSERT(atomic_read(&(exp)->exp_refcount) > 0);		\
		LASSERT(atomic_read(&(exp)->exp_refcount) < 0x5a5a5a);	\
		__zclass_export_put(exp);				\
	} while (0)


struct zestrpc_export;
extern void 
__zclass_export_put(struct zestrpc_export *exp);
