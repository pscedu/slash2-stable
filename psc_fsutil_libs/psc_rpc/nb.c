/* $Id$ */

#define PSC_SUBSYS PSS_RPC

#include <inttypes.h>

#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

struct pscrpc_nbreapthr {
	struct pscrpc_nbreqset	*pnbt_nbset;
};

/**
 * pscrpc_nbreqset_push - send out new requests
 *
 */
static int
pscrpc_nbreqset_push(struct pscrpc_request *req)
{
	return (pscrpc_push_req(req));
}

/**
 * pscrpc_nbreqset_init - make a non-blocking set
 * @nb_interpret:  this must only take into account completed rpcs
 * @nb_callback:   application callback
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
	LOCK_INIT(&nbs->nb_lock);
	return nbs;
}

/**
 * pscrpc_nbreqset_add - add a new non-blocking request to the mix
 *
 */
void
pscrpc_nbreqset_add(struct pscrpc_nbreqset *nbs, struct pscrpc_request *req)
{
	req->rq_waitq = &nbs->nb_waitq;
	atomic_inc(&nbs->nb_outstanding);
	pscrpc_set_add_new_req(&nbs->nb_reqset, req);
	if (pscrpc_nbreqset_push(req)) {
		DEBUG_REQ(PLL_ERROR, req, "Send Failure");
		psc_fatalx("Send Failure");
	}
}

/**
 * pscrpc_nbrequest_flush - sync all outstanding requests
 */
int
pscrpc_nbreqset_flush(struct pscrpc_nbreqset *nbs)
{
	return (pscrpc_set_wait(&nbs->nb_reqset));
}

/**
 * pscrpc_nbrequest_reap - remove completed requests from the
 *                  request set and place them into the
 *                  'completed' list.
 * @nbs: the non-blocking set
 * Notes:  Call before pscrpc_check_set(), pscrpc_check_set() manages
 *        'set_remaining'..  If there are problems look at pscrpc_set_wait(),
 *        there you'll find a more sophisticated and proper use of sets.
 *        pscrpc_set_wait() deals with blocking set nevertheless much applies
 *        here as well.
 */
int
pscrpc_nbreqset_reap(struct pscrpc_nbreqset *nbs)
{
	int    nreaped=0, nchecked=0;
	struct psclist_head          *i, *j;
	struct pscrpc_request     *req;
	struct pscrpc_request_set *set = &nbs->nb_reqset;
	struct l_wait_info lwi;
	int timeout = 1;

	spinlock(&nbs->nb_lock);
	if (nbs->nb_flags & NBREQSET_WORK_INPROG) {
		freelock(&nbs->nb_lock);
		RETURN(0);
	} else {
		nbs->nb_flags |= NBREQSET_WORK_INPROG;
		freelock(&nbs->nb_lock);
	}
		
	lwi = LWI_TIMEOUT(timeout, NULL, NULL);
	psc_cli_wait_event(&set->set_waitq,
		       (nreaped=pscrpc_check_set(set, 0)), &lwi);

	if (!nreaped)
		return (0);

	pscrpc_set_lock(set);

	psclist_for_each_safe(i, j, &set->set_requests) {
		nchecked++;
		req = psclist_entry(i, struct pscrpc_request,
				    rq_set_chain_lentry);
		DEBUG_REQ(PLL_INFO, req, "reap if Completed");
		/*
		 * Move sent rpcs to the set_requests list
		 */
		if (req->rq_phase == PSCRQ_PHASE_COMPLETE) {
			psclist_del(&req->rq_set_chain_lentry);
			atomic_dec(&nbs->nb_outstanding);
			nreaped++;
			/*
			 * paranoia
			 */
			psc_assert(atomic_read(&nbs->nb_outstanding) >= 0);
			/*
			 * This is the caller's last shot at accessing
			 *  this msg..
			 * Let the callback deal with it's own
			 *  error handling, we can't do much from here
			 */
			if (nbs->nb_callback != NULL)
				nbs->nb_callback(req, &req->rq_async_args);
			/*
			 * Be done with it..
			 */
			pscrpc_req_finished(req);
		}
	}
	freelock(&set->set_lock);

	spinlock(&nbs->nb_lock);
	psc_assert(nbs->nb_flags & NBREQSET_WORK_INPROG);
	nbs->nb_flags &= ~NBREQSET_WORK_INPROG;
	freelock(&nbs->nb_lock);

	psc_trace("checked %d requests", nchecked);

	return (nreaped);
}

__dead void *
_pscrpc_nbreapthr_main(void *arg)
{
	struct pscrpc_nbreapthr *pnbt;
	struct psc_thread *thr = arg;

	pnbt = thr->pscthr_private;
	for (;;) {
		pscrpc_nbreqset_reap(pnbt->pnbt_nbset);
		psc_waitq_waitrel_s(&pnbt->pnbt_nbset->nb_waitq, NULL, 1);
	}
}

void
pscrpc_nbreapthr_spawn(struct pscrpc_nbreqset *nbset, int thrtype,
    const char *thrname)
{
	struct pscrpc_nbreapthr *pnbt;
	struct psc_thread *thr;

	thr = pscthr_init(thrtype, 0, _pscrpc_nbreapthr_main,
	    NULL, sizeof(*pnbt), "%s", thrname);
	pnbt = thr->pscthr_private;
	pnbt->pnbt_nbset = nbset;
	pscthr_setready(thr);
}
