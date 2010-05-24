/* $Id$ */
/* %PSC_NO_COPYRIGHT% */

#ifndef _PSCRPC_SERVICE_H_
#define _PSCRPC_SERVICE_H_

#include "psc_rpc/rpc.h"
#include "psc_ds/list.h"

struct psc_thread;

typedef int (*pscrpc_service_func_t)(struct pscrpc_request *);

#define PSCRPC_SVCNAME_MAX 32

struct pscrpc_svc_handle {
	struct psclist_head	  svh_lentry;
	struct pscrpc_service	 *svh_service;
	pscrpc_service_func_t	  svh_handler;
	int			  svh_nthreads;
	int			  svh_nbufs;
	int			  svh_bufsz;
	int			  svh_reqsz;
	int			  svh_repsz;
	int			  svh_req_portal;
	int			  svh_rep_portal;
	int			  svh_type;
	int			  svh_flags;
	size_t			  svh_thrsiz;
	char			  svh_svc_name[PSCRPC_SVCNAME_MAX];
	void			(*svh_initf)(void);
};

typedef struct pscrpc_svc_handle pscrpc_svc_handle_t;

#define PSCRPC_SVCF_COUNT_PEER_QLENS	(1 << 0)

struct pscrpc_thread {
	struct pscrpc_svc_handle *prt_svh;
	struct psclist_head	  prt_lentry;	/* link among thrs in service */
	int			  prt_alive;
};

#define pscrpcthr(thr)		((struct pscrpc_thread *)(thr)->pscthr_private)

#define pscrpc_thread_spawn(svh, type)				\
	do {							\
		(svh)->svh_thrsiz = sizeof(type);		\
		_pscrpc_svh_spawn((svh));			\
	} while (0)

void _pscrpc_svh_spawn(struct pscrpc_svc_handle *);
int pscrpcsvh_addthr(struct pscrpc_svc_handle *);
int pscrpcsvh_delthr(struct pscrpc_svc_handle *);

#endif /* _PSCRPC_SERVICE_H_ */
