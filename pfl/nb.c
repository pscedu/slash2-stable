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

#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/log.h"
#include "pfl/rpc.h"
#include "pfl/rpclog.h"
#include "pfl/thread.h"
#include "pfl/waitq.h"

struct pscrpc_nbreapthr {
	struct pscrpc_nbreqset	*pnbt_nbset;
};

/*
 * Create a new a non-blocking set.
 * @nb_interpret: this must only take into account completed RPCs.
 */
struct pscrpc_nbreqset *
pscrpc_nbreqset_init(pscrpc_set_interpreterf nb_interpret)
{
	struct pscrpc_nbreqset *nbs;

	nbs = PSCALLOC(sizeof(*nbs));
	INIT_SPINLOCK(&nbs->nb_lock);
	psc_compl_init(&nbs->nb_compl);
	nbs->nb_reqset = pscrpc_prep_set();
	nbs->nb_reqset->set_interpret = nb_interpret;
	return (nbs);
}

void
pscrpc_nbreqset_destroy(struct pscrpc_nbreqset *nbs)
{
	psc_compl_destroy(&nbs->nb_compl);
	pscrpc_set_destroy(nbs->nb_reqset);
	PSCFREE(nbs);
}

/*
 * Add a new non-blocking request to a non-blocking set.
 */
int
pscrpc_nbreqset_add(struct pscrpc_nbreqset *nbs,
    struct pscrpc_request *rq)
{
	int rc;

	pscrpc_req_setcompl(rq, &nbs->nb_compl);
	pscrpc_set_add_new_req(&nbs->nb_reqset, rq);
	rc = pscrpc_push_req(rq);
	if (rc) {
		pscrpc_set_remove_req(&nbs->nb_reqset, rq);
		pscrpc_req_setcompl(rq, NULL);
		DEBUG_REQ(PLL_ERROR, rq, "send failure: %s",
		    strerror(rc));
	}
	return (rc);
}

/*
 * Wait for all outstanding requests sent out to be completed in an
 * otherwise non-blocking set.
 */
int
pscrpc_nbreqset_flush(struct pscrpc_nbreqset *nbs)
{
	return (pscrpc_set_wait(nbs->nb_reqset));
}

/**
 * Remove completed requests from the request set and place them into
 * the 'completed' list.
 *
 * @nbs: the non-blocking set
 *
 * Notes: call before pscrpc_check_set(); pscrpc_check_set() manages
 * 'set_remaining'.  If there are problems look at pscrpc_set_wait(),
 * there you'll find a more sophisticated and proper use of sets.
 * pscrpc_set_wait() deals with blocking set nevertheless much applies
 * here as well.
 */
int
pscrpc_nbreqset_reap(struct pscrpc_nbreqset *nbs)
{
	struct pscrpc_request_set *set = nbs->nb_reqset;
	struct pscrpc_request *rq, *next;
	int rc = 0, nchecked = 0;

	spinlock(&nbs->nb_lock);
	if (nbs->nb_flags & NBREQSET_WORK_INPROG) {
		freelock(&nbs->nb_lock);
		return (0);
	} else {
		nbs->nb_flags |= NBREQSET_WORK_INPROG;
		freelock(&nbs->nb_lock);
	}

	if (!pscrpc_check_set(set, 0)) {
		spinlock(&nbs->nb_lock);
		nbs->nb_flags &= ~NBREQSET_WORK_INPROG;
		freelock(&nbs->nb_lock);
		return (0);
	}

	pscrpc_set_lock(set);
	psclist_for_each_entry_safe(rq, next,
	    &set->set_requests, rq_set_chain_lentry) {
		nchecked++;
		DEBUG_REQ(PLL_DIAG, rq, "reap if completed");

		if (rq->rq_phase == PSCRPC_RQ_PHASE_COMPLETE) {
			pscrpc_set_remove_req(set, rq);

			/* Return the first error. */
			if (!rc)
				rc = rq->rq_status;

			pscrpc_req_finished(rq);
		}
	}
	freelock(&set->set_lock);

	spinlock(&nbs->nb_lock);
	psc_assert(nbs->nb_flags & NBREQSET_WORK_INPROG);
	nbs->nb_flags &= ~NBREQSET_WORK_INPROG;
	freelock(&nbs->nb_lock);

	psclog_debug("checked %d requests", nchecked);

	return (rc);
}

void
pscrpc_nbreapthr_main(struct psc_thread *thr)
{
	struct pscrpc_nbreapthr *pnbt;
	struct pscrpc_nbreqset *nbs;
	int cntr;

	pnbt = thr->pscthr_private;
	nbs = pnbt->pnbt_nbset;
	while (pscthr_run(thr)) {
		cntr = nbs->nb_compl.pc_counter;

		pscrpc_nbreqset_reap(nbs);

		spinlock(&nbs->nb_compl.pc_lock);
		if (cntr == nbs->nb_compl.pc_counter)
			psc_compl_waitrel_s(&nbs->nb_compl, 1);
		else
			freelock(&nbs->nb_compl.pc_lock);
	}
}

void
pscrpc_nbreapthr_spawn(struct pscrpc_nbreqset *nbset, int thrtype,
    int nthr, const char *thrname)
{
	struct pscrpc_nbreapthr *pnbt;
	struct psc_thread *thr;
	int i;

	for (i = 0; i < nthr; i++) {
		thr = pscthr_init(thrtype, 0, pscrpc_nbreapthr_main,
		    NULL, sizeof(*pnbt), thrname, i);
		pnbt = thr->pscthr_private;
		pnbt->pnbt_nbset = nbset;
		pscthr_setready(thr);
	}
}
