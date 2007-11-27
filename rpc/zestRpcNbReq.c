/* $Id: zestRpcNbReq.c 2195 2007-11-08 16:35:41Z yanovich $ */

#include "zestAlloc.h"
#include "zestRpc.h"
#include "zestRpcLog.h"


/**
 * nbreqset_push - send out new requests
 *
 */
static int
nbreqset_push(struct zestrpc_request *req) {
	return (zestrpc_push_req(req));
}

#define breqset_push nbreqset_push

/**
 * nbreqset_init - make a non-blocking set
 * @nb_interpret:  this must only take into account completed rpcs
 * @nb_callback:   application callback
 */
struct zestrpc_nbreqset *
nbreqset_init(set_interpreter_func nb_interpret, 
	      nbreq_callback       nb_callback) {
	struct zestrpc_nbreqset *nbs;

	nbs = ZALLOC(sizeof(struct zestrpc_nbreqset));
	
	//LOCK_INIT(&nbs->nb_lock);

	nbs->nb_reqset                = zestrpc_prep_set();
	nbs->nb_reqset->set_interpret = nb_interpret;
	nbs->nb_callback              = nb_callback;
	atomic_set(&nbs->nb_outstanding, 0);

	return nbs;
}

/**
 * nbreqset_add - add a new non-blocking request to the mix
 *
 */
void
nbreqset_add(struct zestrpc_nbreqset *nbs, 
	     struct zestrpc_request  *req) {
	
	atomic_inc(&nbs->nb_outstanding);
	zestrpc_set_add_new_req(nbs->nb_reqset, req);
	if (nbreqset_push(req)) { 
		DEBUG_REQ(ZLL_ERROR, req, "Send Failure");
		zfatalx("Send Failure");
	}
}

/**
 * nbrequest_flush - sync all outstanding requests
 */
int
nbrequest_flush(struct zestrpc_nbreqset *nbs) {
	int rc;

	rc = zestrpc_set_wait(nbs->nb_reqset);
	return rc;
}

/**
 * nbrequest_reap - remove completed requests from the 
 *                  request set and place them into the
 *                  'completed' list.  
 * @nbs: the non-blocking set 
 * Notes:  Call before zestrpc_check_set(), zestrpc_check_set() manages 
 *        'set_remaining'..  If there are problems look at zestrpc_set_wait(), 
 *        there you'll find a more sophisticated and proper use of sets.  
 *        zestrpc_set_wait() deals with blocking set nevertheless much applies
 *        here as well.
 */
int
nbrequest_reap(struct zestrpc_nbreqset *nbs) {
	int    nreaped = 0, nchecked=0;
	struct zlist_head          *i, *j;
	struct zestrpc_request     *req;
	struct zestrpc_request_set *set = nbs->nb_reqset;
        struct l_wait_info lwi;
        int timeout = 1;

	ENTRY;

        lwi = LWI_TIMEOUT(timeout, NULL, NULL);
        zcli_wait_event(set->set_waitq, (nreaped=zestrpc_check_set(set, 0)), &lwi);
	
	if (!nreaped)
		RETURN(0);
	
	spinlock(&set->set_new_req_lock);	

	zlist_for_each_safe(i, j, &set->set_requests) { 
		nchecked++;
		req = zlist_entry(i, struct zestrpc_request, rq_set_chain);
		DEBUG_REQ(ZLL_INFO, req, "reap if Completed");			
		/*
		 * Move sent rpcs to the set_requests list
		 */
		if (req->rq_phase == ZRQ_PHASE_COMPLETE) {	
			zlist_del_init(&req->rq_set_chain);	
			//atomic_dec(&nbs->nb_outstanding);
			nreaped++;
			/*
			 * paranoia
			 */
			//zest_assert(atomic_read(&nbs->nb_outstanding) >= 0);
			/*
			 * This is the caller's last shot at accessing 
			 *  this msg..
			 * Let the callback deal with it's own
			 *  error handling, we can't do much from here
			 */			
			//if (nbs->nb_callback != NULL)
			//	(int)nbs->nb_callback(req, 
			//			      &req->rq_async_args);
			/*
			 * Be done with it..
			 */
			zestrpc_req_finished(req);
		}
	}
	freelock(&set->set_new_req_lock);
	zdbg("checked %d requests", nchecked);
	RETURN(nreaped);
}

