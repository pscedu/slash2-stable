/* $Id$ */

/*
 *  Copyright (c) 2002, 2003 Cluster File Systems, Inc.
 *
 *   This file is part of the Lustre file system, http://www.lustre.org
 *   Lustre is a trademark of Cluster File Systems, Inc.
 *
 *   You may have signed or agreed to another license before downloading
 *   this software.  If so, you are bound by the terms and conditions
 *   of that agreement, and the following does not apply to you.  See the
 *   LICENSE file included with this distribution for more information.
 *
 *   If you did not agree to a different license, then this copy of Lustre
 *   is open source software; you can redistribute it and/or modify it
 *   under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   In either case, Lustre is distributed in the hope that it will be
 *   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   license text for more details.
 *
 */

#define PSC_SUBSYS PSS_RPC

#include <inttypes.h>

#include "pfl/rpc.h"
#include "pfl/rpclog.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

struct pscrpc_nbreapthr {
	struct pscrpc_nbreqset	*pnbt_nbset;
};

/**
 * pscrpc_nbreqset_init - Make a non-blocking set.
 * @nb_interpret: this must only take into account completed RPCs.
 * @nb_callback: application callback.
 */
struct pscrpc_nbreqset *
pscrpc_nbreqset_init(pscrpc_set_interpreterf nb_interpret,
    pscrpc_nbreq_callback nb_callback)
{
	struct pscrpc_nbreqset *nbs;

	nbs = PSCALLOC(sizeof(*nbs));
	psc_waitq_init(&nbs->nb_waitq);
	pscrpc_set_init(&nbs->nb_reqset);
	nbs->nb_reqset.set_interpret = nb_interpret;
	nbs->nb_callback = nb_callback;
	atomic_set(&nbs->nb_outstanding, 0);
	INIT_SPINLOCK(&nbs->nb_lock);
	return (nbs);
}

/**
 * pscrpc_nbreqset_add - Add a new non-blocking request to a
 *	non-blocking set.
 */
int
pscrpc_nbreqset_add(struct pscrpc_nbreqset *nbs,
    struct pscrpc_request *req)
{
	int rc;

	psc_assert(req->rq_waitq == NULL);
	req->rq_waitq = &nbs->nb_waitq;
	atomic_inc(&nbs->nb_outstanding);
	rc = pscrpc_push_req(req);
	if (rc) {
		req->rq_waitq = NULL;
		atomic_dec(&nbs->nb_outstanding);
		DEBUG_REQ(PLL_ERROR, req, "send failure: %s", strerror(rc));
	} else
		pscrpc_set_add_new_req(&nbs->nb_reqset, req);

	return (rc);
}

/**
 * pscrpc_nbrequest_flush - Wait for all outstanding requests sent out
 *	in a non-blocking set.
 */
int
pscrpc_nbreqset_flush(struct pscrpc_nbreqset *nbs)
{
	return (pscrpc_set_wait(&nbs->nb_reqset));
}

/**
 * pscrpc_nbrequest_reap - Remove completed requests from the request
 *	set and place them into the 'completed' list.
 * @nbs: the non-blocking set
 * Notes: Call before pscrpc_check_set(), pscrpc_check_set() manages
 *	'set_remaining'.  If there are problems look at
 *	pscrpc_set_wait(), there you'll find a more sophisticated and
 *	proper use of sets.  pscrpc_set_wait() deals with blocking set
 *	nevertheless much applies here as well.
 */
int
pscrpc_nbreqset_reap(struct pscrpc_nbreqset *nbs)
{
	int saved_rc = 0;
	int rc, nreaped = 0, nchecked = 0;
	struct pscrpc_request_set *set = &nbs->nb_reqset;
	struct pscrpc_request *req, *next;

	spinlock(&nbs->nb_lock);
	if (nbs->nb_flags & NBREQSET_WORK_INPROG) {
		freelock(&nbs->nb_lock);
		RETURN(0);
	} else {
		nbs->nb_flags |= NBREQSET_WORK_INPROG;
		freelock(&nbs->nb_lock);
	}

	if (!(nreaped = pscrpc_check_set(set, 0))) {
		spinlock(&nbs->nb_lock);
		nbs->nb_flags &= ~NBREQSET_WORK_INPROG;
		freelock(&nbs->nb_lock);
		return (0);
	}

	pscrpc_set_lock(set);

	psclist_for_each_entry_safe(req, next,
	    &set->set_requests, rq_set_chain_lentry) {
		nchecked++;
		DEBUG_REQ(PLL_INFO, req, "reap if completed");

		/*
		 * Move sent RPCs to the set_requests list
		 */
		if (req->rq_phase == PSCRPC_RQ_PHASE_COMPLETE) {
			psclist_del(&req->rq_set_chain_lentry,
			    &set->set_requests);
			atomic_dec(&nbs->nb_outstanding);
			nreaped++;

			/* paranoia */
			psc_assert(atomic_read(&nbs->nb_outstanding) >= 0);

			/*
			 * This is the caller's last shot at accessing
			 *  this msg.
			 * Let the callback deal with its own
			 *  error handling; we can't do much from here.
			 */
			rc = 0;
			if (nbs->nb_callback)
				rc = nbs->nb_callback(req,
				    &req->rq_async_args);

			/* Be done with it. */
			pscrpc_req_finished(req);

			/* Record the first error. */
			if (saved_rc == 0 && rc)
				saved_rc = rc;
		}
	}
	freelock(&set->set_lock);

	spinlock(&nbs->nb_lock);
	psc_assert(nbs->nb_flags & NBREQSET_WORK_INPROG);
	nbs->nb_flags &= ~NBREQSET_WORK_INPROG;
	freelock(&nbs->nb_lock);

	psclog_trace("checked %d requests", nchecked);

	return (saved_rc);
}

void
pscrpc_nbreapthr_main(struct psc_thread *thr)
{
	struct pscrpc_nbreapthr *pnbt;

	pnbt = thr->pscthr_private;
	while (pscthr_run()) {
		pscrpc_nbreqset_reap(pnbt->pnbt_nbset);
		psc_waitq_waitrel_s(&pnbt->pnbt_nbset->nb_waitq, NULL,
		    1);
	}
}

void
pscrpc_nbreapthr_spawn(struct pscrpc_nbreqset *nbset, int thrtype,
    const char *thrname)
{
	struct pscrpc_nbreapthr *pnbt;
	struct psc_thread *thr;

	thr = pscthr_init(thrtype, 0, pscrpc_nbreapthr_main,
	    NULL, sizeof(*pnbt), "%s", thrname);
	pnbt = thr->pscthr_private;
	pnbt->pnbt_nbset = nbset;
	pscthr_setready(thr);
}
