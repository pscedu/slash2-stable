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

#ifndef _PSCRPC_SERVICE_H_
#define _PSCRPC_SERVICE_H_

#include "pfl/list.h"
#include "pfl/lock.h"

struct pscrpc_request;
struct pscrpc_service;
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

#define PSCRPC_SVCF_COUNT_PEER_QLENS	(1 << 0)

struct pscrpc_thread {
	struct pscrpc_svc_handle *prt_svh;
	struct psclist_head	  prt_lentry;	/* link among thrs in service */
	int			  prt_alive;
	char			  prt_peer_addrbuf[64];
	uint64_t		  prt_peer_addr;
};

#define pscrpcthr(thr)		((struct pscrpc_thread *)(thr)->pscthr_private)

#define pscrpc_thread_spawn(svh, type)				\
	do {							\
		(svh)->svh_thrsiz = sizeof(type);		\
		_pscrpc_svh_spawn(svh);				\
	} while (0)

void	_pscrpc_svh_spawn(struct pscrpc_svc_handle *);
void	 pscrpc_svh_destroy(struct pscrpc_svc_handle *);
int	 pscrpcsvh_addthr(struct pscrpc_svc_handle *);
int	 pscrpcsvh_delthr(struct pscrpc_svc_handle *);

extern struct psclist_head pscrpc_all_services;
extern psc_spinlock_t pscrpc_all_services_lock;

#endif /* _PSCRPC_SERVICE_H_ */
