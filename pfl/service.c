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
#include <stdio.h>

#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/ctl.h"
#include "pfl/ctlsvr.h"
#include "pfl/export.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/opstats.h"
#include "pfl/rpc.h"
#include "pfl/rpclog.h"
#include "pfl/service.h"
#include "pfl/str.h"
#include "pfl/waitq.h"

static int test_req_buffer_pressure;

static int pscrpc_server_post_idle_rqbds(struct pscrpc_service *);

PSCLIST_HEAD(pscrpc_all_services);
psc_spinlock_t pscrpc_all_services_lock = SPINLOCK_INIT;

PSCLIST_HEAD(pscrpc_svh_list);

static char *
pscrpc_alloc_request_buffer(int size)
{
	char *ptr;

	PSCRPC_OBD_ALLOC(ptr, size);
	return (ptr);
}

static void
pscrpc_free_request_buffer(char *ptr, __unusedx int size)
{
	PSCRPC_OBD_FREE(ptr, size);
}

/*
 * Create a new request buffer desc and alloc request buffer memory.
 * This call sets request_in_callback as the callback handler.
 * @svc: pointer to the service which owns this request buffer ptr.
 */
struct pscrpc_request_buffer_desc *
pscrpc_alloc_rqbd(struct pscrpc_service *svc)
{
	struct pscrpc_request_buffer_desc *rqbd;

	rqbd = psc_pool_get(svc->srv_pool);
	memset(rqbd, 0, sizeof(*rqbd));

	rqbd->rqbd_service = svc;
	rqbd->rqbd_cbid.cbid_fn = pscrpc_request_in_callback;
	rqbd->rqbd_cbid.cbid_arg = rqbd;

	INIT_PSCLIST_HEAD(&rqbd->rqbd_reqs);
	INIT_PSC_LISTENTRY(&rqbd->rqbd_lentry);
	rqbd->rqbd_buffer = pscrpc_alloc_request_buffer(svc->srv_buf_size);

	if (rqbd->rqbd_buffer == NULL) {
		psc_pool_return(svc->srv_pool, rqbd);
		return (NULL);
	}

	SVC_LOCK(svc);
	psclist_add(&rqbd->rqbd_lentry, &svc->srv_idle_rqbds);
	svc->srv_nbufs++;
	SVC_ULOCK(svc);

	return (rqbd);
}

void
pscrpc_free_rqbd(struct pscrpc_request_buffer_desc *rqbd)
{
	struct pscrpc_service *svc = rqbd->rqbd_service;

	LASSERT(rqbd->rqbd_refcount == 0);
	LASSERT(psc_listhd_empty(&rqbd->rqbd_reqs));

	SVC_LOCK(svc);
	psclist_del(&rqbd->rqbd_lentry, psc_lentry_hd(&rqbd->rqbd_lentry));
	svc->srv_nbufs--;
	SVC_ULOCK(svc);

	pscrpc_free_request_buffer(rqbd->rqbd_buffer, svc->srv_buf_size);

	psc_pool_return(svc->srv_pool, rqbd);
}

/*
 * Iterate over the srv_idle_rqbds list and repost buffer desc's found
 * there.  Calls pscrpc_register_rqbd() which does LNetMEAttach and
 * LNetMDAttach.
 * @svc: pointer to the service
 */
int
pscrpc_server_post_idle_rqbds(struct pscrpc_service *svc)
{
	struct pscrpc_request_buffer_desc *rqbd;
	int rc, posted = 0;

	for (;;) {
		SVC_LOCK(svc);

		if (psc_listhd_empty(&svc->srv_idle_rqbds)) {
			SVC_ULOCK(svc);
			return (posted);
		}

		rqbd = psc_listhd_first_obj(&svc->srv_idle_rqbds,
		    struct pscrpc_request_buffer_desc, rqbd_lentry);
		psclist_del(&rqbd->rqbd_lentry, &svc->srv_idle_rqbds);

		/* assume we will post successfully */
		svc->srv_nrqbd_receiving++;
		psclist_add(&rqbd->rqbd_lentry, &svc->srv_active_rqbds);

		SVC_ULOCK(svc);

		psc_assert(rqbd->rqbd_buffer);
		rc = pscrpc_register_rqbd(rqbd);
		if (rc)
			break;

		posted = 1;
	}

	SVC_LOCK(svc);

	svc->srv_nrqbd_receiving--;
	psclist_del(&rqbd->rqbd_lentry, &svc->srv_active_rqbds);
	psclist_add_tail(&rqbd->rqbd_lentry, &svc->srv_idle_rqbds);

	/*
	 * Don't complain if no request buffers are posted right now;
	 * LNET won't drop requests because we set the portal lazy!
	 */

	SVC_ULOCK(svc);

	return (-1);
}

static void
_pscrpc_server_free_request(struct pscrpc_request *req)
{
	struct pscrpc_request_buffer_desc *rqbd = req->rq_rqbd;

	psclist_del(&req->rq_lentry, psc_lentry_hd(&req->rq_lentry));

	if (req->rq_reply_state != NULL) {
		pscrpc_rs_decref(req->rq_reply_state);
		req->rq_reply_state = NULL;
	}

	if (req->rq_conn)
		pscrpc_put_connection(req->rq_conn);
	req->rq_conn = NULL;

	pll_remove(&pscrpc_requests, req);
	if (req != &rqbd->rqbd_req) {
		/*
		 * NB request buffers use an embedded
		 * req if the incoming req unlinked the
		 * MD; this isn't one of them!
		 */
		psc_pool_return(pscrpc_rq_pool, req);
	}
}

static void
pscrpc_server_free_request(struct pscrpc_request *req)
{
	struct pscrpc_request_buffer_desc *rqbd = req->rq_rqbd;
	struct pscrpc_service             *svc  = rqbd->rqbd_service;
	struct pscrpc_request *nxt;
	int refcount;

	SVC_LOCK(svc);

	svc->srv_n_active_reqs--;
	psclist_add(&req->rq_lentry, &rqbd->rqbd_reqs);

	refcount = --(rqbd->rqbd_refcount);
	if (refcount == 0) {
		/* request buffer is now idle: add to history */
		psclist_del(&rqbd->rqbd_lentry, psc_lentry_hd(&rqbd->rqbd_lentry));
		psclist_add_tail(&rqbd->rqbd_lentry, &svc->srv_history_rqbds);
		svc->srv_n_history_rqbds++;

		/* cull some history?
		 * I expect only about 1 or 2 rqbds need to be recycled here */
		while (svc->srv_n_history_rqbds > svc->srv_max_history_rqbds) {
			rqbd = psc_listhd_first_obj(&svc->srv_history_rqbds,
			    struct pscrpc_request_buffer_desc, rqbd_lentry);

			psclist_del(&rqbd->rqbd_lentry, &svc->srv_history_rqbds);
			svc->srv_n_history_rqbds--;

			/* remove rqbd's reqs from svc's req history while
			 * I've got the service lock */
			psclist_for_each_entry(req, &rqbd->rqbd_reqs,
			    rq_lentry) {
				/* Track the highest culled req seq */
				if (req->rq_history_seq >
				    svc->srv_request_max_cull_seq)
					svc->srv_request_max_cull_seq =
						req->rq_history_seq;
				psclist_del(&req->rq_history_lentry,
				    &svc->srv_request_history);
			}

			SVC_ULOCK(svc);

			psclist_for_each_entry_safe(req, nxt,
			    &rqbd->rqbd_reqs, rq_lentry)
				_pscrpc_server_free_request(req);

			SVC_LOCK(svc);

			/* schedule request buffer for re-use.
			 * NB I can only do this after I've disposed of their
			 * reqs; particularly the embedded req */
			psclist_add_tail(&rqbd->rqbd_lentry, &svc->srv_idle_rqbds);
		}
	} else if (req->rq_reply_state && req->rq_reply_state->rs_prealloc) {
		/* If we are low on memory, we are not interested in
		   history */
		psclist_del(&req->rq_history_lentry,
		    psc_lentry_hd(&req->rq_history_lentry));
		_pscrpc_server_free_request(req);
	}

	SVC_ULOCK(svc);
}

static int
pscrpc_server_handle_request(struct pscrpc_service *svc,
			     struct psc_thread     *thread)
{
	struct pscrpc_request *request;
	struct timeval         work_start;
	struct timeval         work_end;
	struct pscrpc_thread *prt;
	long                   timediff;
	int                    rc;
	char buf[PSCRPC_NIDSTR_SIZE];

	LASSERT(svc);

	SVC_LOCK(svc);

	if (psc_listhd_empty(&svc->srv_request_queue) ||
	    (svc->srv_n_difficult_replies != 0 &&
	     svc->srv_n_active_reqs >= (svc->srv_nthreads - 1))) {
		/*
		 * If all the other threads are handling requests, I must
		 * remain free to handle any 'difficult' reply that might
		 * block them
		 */
		SVC_ULOCK(svc);
		return (0);
	}

	request = psc_listhd_first_obj(&svc->srv_request_queue,
	    struct pscrpc_request, rq_lentry);

	psclist_del(&request->rq_lentry, &svc->srv_request_queue);
	svc->srv_n_queued_reqs--;
	svc->srv_n_active_reqs++;

	SVC_ULOCK(svc);

	do_gettimeofday(&work_start);
	timediff = cfs_timeval_sub(&work_start,
				   &request->rq_arrival_time, NULL);

	pfl_opstats_grad_incr(&pfl_rpc_service_reply_latencies,
	    timediff / 1000000);

#if WETRACKSTATSSOMEDAY
	if (svc->srv_stats != NULL) {
		lprocfs_counter_add(svc->srv_stats, PTLRPC_REQWAIT_CNTR,
			timediff);
		lprocfs_counter_add(svc->srv_stats, PTLRPC_REQQDEPTH_CNTR,
			svc->srv_n_queued_reqs);
		lprocfs_counter_add(svc->srv_stats, PTLRPC_REQACTIVE_CNTR,
			svc->srv_n_active_reqs);
	}
#endif
#if SWAB_PARANOIA
	/* Clear request swab mask; this is a new request */
	request->rq_req_swab_mask = 0;
#endif
	rc = pscrpc_unpack_msg(request->rq_reqmsg, request->rq_reqlen);
	if (rc) {
		CERROR("error unpacking request: ptl %d from %s"
			" xid %"PRIx64, svc->srv_req_portal,
			libcfs_id2str(request->rq_peer), request->rq_xid);
		goto out;
	}

	if (request->rq_reqmsg->type != PSCRPC_MSG_REQUEST) {
		CERROR("wrong packet type received (type=%u) from %s",
		       request->rq_reqmsg->type,
		       libcfs_id2str(request->rq_peer));
		goto out;
	}

	DEBUG_REQ(PLL_DIAG, request, buf, "got req xid=%"PRId64, request->rq_xid);

	request->rq_svc_thread = thread;
	request->rq_conn = pscrpc_get_connection(request->rq_peer,
						 request->rq_self, NULL);
	if (request->rq_conn == NULL) {
		CERROR("null connection struct");
		return -ENOTCONN;
	}

	/*
	 * Here's a hack to trick lustre rpc into thinking there's a real export
	 */
	spinlock(&request->rq_conn->c_lock);
	if (request->rq_conn->c_exp == NULL) {
		struct pscrpc_export *exp;

		exp = psc_pool_get(pscrpc_export_pool);

		/*
		 * init and associate the connection and export structs
		 *  see pscrpc_new_export() for more detail
		 */
		INIT_SPINLOCK(&exp->exp_lock);
		atomic_set(&exp->exp_refcount, 1);
		atomic_set(&exp->exp_rpc_count, 0);
		exp->exp_connection = request->rq_conn;
		request->rq_conn->c_exp = exp;
	}
	freelock(&request->rq_conn->c_lock);
	pscrpc_export_rpc_get(request->rq_conn->c_exp);
	request->rq_export = request->rq_conn->c_exp;

	/* Discard requests queued for longer than my timeout.  If the
	 * client's timeout is similar to mine, she'll be timing out this
	 * REQ anyway (bug 1502) */
	if (timediff / 1000000 > pfl_rpc_timeout) {
		CERROR("Dropping timed-out opc %d request from %s"
		       ": %ld seconds old", request->rq_reqmsg->opc,
		       libcfs_id2str(request->rq_peer),
		       timediff / 1000000);
		goto put_rpc_export;
	}

	request->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;

	prt = pscrpcthr(thread);
	prt->prt_peer_addr = request->rq_peer.nid;

	DEBUG_REQ(PLL_DEBUG, request, buf, "handling RPC");

	/*
 	 * 07/07/2017: We have already registered the connection into the
 	 * pscrpc_conn_hashtbl hash table before handling the RPC.
 	 */
	rc = svc->srv_handler(request);

	request->rq_phase = PSCRPC_RQ_PHASE_COMPLETE;

	DEBUG_REQ(PLL_DEBUG, request, buf, "handled RPC");

	prt->prt_peer_addr = LNET_NID_ANY;
	prt->prt_peer_addrbuf[0] = '\0';

 put_rpc_export:
	pscrpc_export_rpc_put(request->rq_export);
	request->rq_export = NULL;

 out:
	if (request->rq_export != NULL)
		pscrpc_export_put(request->rq_export);

	do_gettimeofday(&work_end);

	/*
	 * peer queue length accounting
	 * XXX optimization: perform destruction only if export was destroyed.
	 */
	if (svc->srv_count_peer_qlens &&
	    atomic_dec_return(&request->rq_peer_qlen->pql_qlen) == 0) {
		struct pscrpc_peer_qlen *pq;
		struct psc_hashbkt *b;

		b = psc_hashbkt_get(&svc->srv_peer_qlentab,
		    &request->rq_peer.nid);
		/* Look up the struct again in case it disappeared. */
		pq = psc_hashbkt_search_cmp(&svc->srv_peer_qlentab, b,
		    &request->rq_peer, &request->rq_peer.nid);
		if (pq && atomic_read(&pq->pql_qlen) == 0)
			psc_hashent_remove(&svc->srv_peer_qlentab, pq);
		else
			pq = NULL;
		psc_hashbkt_put(&svc->srv_peer_qlentab, b);
		PSCFREE(pq);
	}

	timediff = cfs_timeval_sub(&work_end, &work_start, NULL);

	if (timediff / 1000000 > pfl_rpc_timeout)
		DEBUG_REQ(PLL_ERROR, request, buf,
		    "timeout, processed in %lds",
		    cfs_timeval_sub(&work_end, &request->rq_arrival_time,
		      NULL) / 1000000);
	else
		psclog_diag("request %"PRIu64" opc %u from %s "
		    "processed in %ldus (%ldus total) trans %"PRIu64" "
		    "rc %d/%d",
		    request->rq_xid, request->rq_reqmsg->opc,
		    libcfs_id2str(request->rq_peer), timediff,
		    cfs_timeval_sub(&work_end,
		      &request->rq_arrival_time, NULL),
		    request->rq_transno, request->rq_status,
		    request->rq_repmsg ?
		      (int)request->rq_repmsg->status : -999);

#if 0 // We have no proc counter in user mode
	if (svc->srv_stats != NULL) {
		int opc = opcode_offset(request->rq_reqmsg->opc);
		if (opc > 0) {
			LASSERT(opc < LUSTRE_MAX_OPCODES);
			lprocfs_counter_add(svc->srv_stats,
				opc + PTLRPC_LAST_CNTR,
				timediff);
		}
	}
#endif

	pscrpc_server_free_request(request);

	return (1);
}

static int
pscrpc_server_handle_reply(struct pscrpc_service *svc)
{
	LASSERT(svc != NULL);
#if 0
	struct pscrpc_reply_state *rs;
	struct obd_export         *exp;
	struct obd_device         *obd;
	int                        nlocks;
	int                        been_handled;

	SVC_LOCK(svc);
	if (psc_listhd_empty(&svc->srv_reply_queue)) {
		SVC_ULOCK(svc);
		return (0);
	}

	rs = psc_lentry_obj(svc->srv_reply_queue.next,
	    struct pscrpc_reply_state, rs_list_entry);

	exp = rs->rs_export;
	obd = exp->exp_obd;

	LASSERT(rs->rs_difficult);
	LASSERT(rs->rs_scheduled);

	psclist_del(&rs->rs_list_entry);

	/* Disengage from notifiers carefully (lock order - irqrestore below!)*/
	SVC_ULOCK(svc);

	spinlock(&obd->obd_uncommitted_replies_lock);
	/* Noop if removed already */
	psclist_del_init(&rs->rs_obd_list);
	freelock(&obd->obd_uncommitted_replies_lock);

	spinlock(&exp->exp_lock);
	/* Noop if removed already */
	psclist_del_init(&rs->rs_exp_list);
	freelock(&exp->exp_lock);

	SVC_LOCK(svc);

	been_handled = rs->rs_handled;
	rs->rs_handled = 1;

	nlocks = rs->rs_nlocks;			/* atomic "steal", but */
	rs->rs_nlocks = 0;			/* locks still on rs_locks! */

	if (nlocks == 0 && !been_handled) {
		/* If we see this, we should already have seen the warning
		 * in mds_steal_ack_locks()  */
		CWARN("All locks stolen from rs %p x%"PRId64".t%"PRId64
		      " o%d NID %s",
		      rs,
		      rs->rs_xid, rs->rs_transno,
		      rs->rs_msg.opc,
		      libcfs_nid2str(exp->exp_connection->c_peer.nid));
	}

	if ((!been_handled && rs->rs_on_net) ||
	    nlocks > 0) {
		SVC_ULOCK(svc);

		if (!been_handled && rs->rs_on_net) {
			LNetMDUnlink(rs->rs_md_h);
			/* Ignore return code; we're racing with
			 * completion... */
		}

		while (nlocks-- > 0)
			ldlm_lock_decref(&rs->rs_locks[nlocks],
				rs->rs_modes[nlocks]);

		SVC_LOCK(svc);
	}

	rs->rs_scheduled = 0;

	if (!rs->rs_on_net) {
		/* Off the net */
		svc->srv_n_difficult_replies--;
		SVC_ULOCK(svc);

		pscrpc_export_put(exp);
		rs->rs_export = NULL;
		pscrpc_rs_decref(rs);
		atomic_dec(&svc->srv_outstanding_replies);
		return (1);
	}

	/* still on the net; callback will schedule */
	SVC_ULOCK(svc);
#endif
	return (1);
}

int
pscrpc_target_send_reply_msg(struct pscrpc_request *req, int rc,
    int fail_id)
{
	char buf[PSCRPC_NIDSTR_SIZE];

#if PAULS_TODO
	if (PSCRPC_OBD_FAIL_CHECK(fail_id | PSCRPC_OBD_FAIL_ONCE)) {
		obd_fail_loc |= PSCRPC_OBD_FAIL_ONCE | PSCRPC_OBD_FAILED;
		DEBUG_REQ(PLL_ERROR, req, buf, "dropping reply");
		return (-ECOMM);
	}
#endif
	if (fail_id) {
		DEBUG_REQ(PLL_ERROR, req, buf,"dropping reply");
		return (-ECOMM);
	}

	if (rc) {
		DEBUG_REQ(PLL_ERROR, req, buf, "processing error (%d)", rc);
		req->rq_status = rc;
		return (pscrpc_error(req));
	} else {
		DEBUG_REQ(PLL_DIAG, req, buf, "sending reply");
	}

	return (pscrpc_send_reply(req, 1));
}

int
pscrpc_grow_req_bufs(struct pscrpc_service *svc)
{
	struct pscrpc_request_buffer_desc *rqbd;
	int i;

	CDEBUG(D_RPCTRACE, "%s: allocate %d new %d-byte reqbufs (%d/%d left)",
	       svc->srv_name, svc->srv_nbuf_per_group, svc->srv_buf_size,
	       svc->srv_nrqbd_receiving, svc->srv_nbufs);
	for (i = 0; i < svc->srv_nbuf_per_group; i++) {
		rqbd = pscrpc_alloc_rqbd(svc);

		if (rqbd == NULL) {
			CERROR("%s: Can't allocate request buffer",
				svc->srv_name);
			return (-ENOMEM);
		}

		if (pscrpc_server_post_idle_rqbds(svc) < 0)
			return (-EAGAIN);
	}

	return (0);
}

static void
pscrpc_check_rqbd_pool(struct pscrpc_service *svc)
{
	int avail = svc->srv_nrqbd_receiving;
	int low_water = test_req_buffer_pressure ? 0 :
		svc->srv_nbuf_per_group/2;

	/* NB I'm not locking; just looking. */

	/* CAVEAT EMPTOR: We might be allocating buffers here because we've
	 * allowed the request history to grow out of control.  We could put a
	 * sanity check on that here and cull some history if we need the
	 * space. */

	if (avail <= low_water)
		pscrpc_grow_req_bufs(svc);

	//lprocfs_counter_add(svc->srv_stats, PTLRPC_REQBUF_AVAIL_CNTR, avail);
}

static int
pscrpc_retry_rqbds(void *arg)
{
	struct pscrpc_service *svc = arg;

	svc->srv_rqbd_timeout = 0;
	return (-ETIMEDOUT);
}

int
pscrpcthr_waitevent(struct psc_thread *thr,
    struct pscrpc_service *svc)
{
	int rc;

	SVC_LOCK(svc);
	rc = (!pscthr_run(thr) &&
	     svc->srv_n_difficult_replies == 0) ||
	    (!psc_listhd_empty(&svc->srv_idle_rqbds) &&
	     svc->srv_rqbd_timeout == 0) ||
	    !psc_listhd_empty(&svc->srv_reply_queue) ||
	    (!psc_listhd_empty(&svc->srv_request_queue) &&
	     (svc->srv_n_difficult_replies == 0 ||
	      svc->srv_n_active_reqs <
	      (svc->srv_nthreads - 1)));
	SVC_ULOCK(svc);
	return (rc);
}

__static void
pscrpcthr_main(struct psc_thread *thr)
{
	struct pscrpc_reply_state *rs;
	struct pscrpc_service *svc;
	struct pscrpc_thread *prt;
	int rc = 0;

	prt = pscrpcthr(thr);
	if (prt->prt_svh->svh_initf)
		prt->prt_svh->svh_initf();
	svc = prt->prt_svh->svh_service;
	psc_assert(svc);

	if (svc->srv_init) {
		rc = svc->srv_init(thr);
		if (rc)
			goto out;
	}
	/* Alloc reply state structure for this one */
	PSCRPC_OBD_ALLOC(rs, svc->srv_max_reply_size);
	INIT_PSC_LISTENTRY(&rs->rs_list_entry);

	SVC_LOCK(svc);
	svc->srv_nthreads++;
	psclist_add(&rs->rs_list_entry, &svc->srv_free_rs_list);
	psc_waitq_wakeall(&svc->srv_free_rs_waitq);
	SVC_ULOCK(svc);

	CDEBUG(D_NET, "service thread started");

	/* XXX maintain a list of all managed devices: insert here */
	while ((pscthr_run(thr) && prt->prt_alive) ||
	    svc->srv_n_difficult_replies != 0) {

		/* Don't exit while there are replies to be handled */
		struct l_wait_info lwi = LWI_TIMEOUT(svc->srv_rqbd_timeout,
						     pscrpc_retry_rqbds, svc);

#if 0
		lc_watchdog_disable(watchdog);
		l_wait_event_exclusive(svc->srv_waitq,
		psclog_debug("run %d, svc->srv_n_difficult_replies %d, "
		    "psc_listhd_empty(&svc->srv_idle_rqbds) %d,  svc->srv_rqbd_timeout %d "
		    "psc_listhd_empty(&svc->srv_reply_queue) %d, psc_listhd_empty(&svc->srv_request_queue) %d "
		    "svc->srv_n_active_reqs %d svc->srv_nthreads %d"
		    "COND 1=%d, COND 2=%d, COND 3=%d",

		    thr->pscthr_run, svc->srv_n_difficult_replies,
		    psc_listhd_empty(&svc->srv_idle_rqbds), svc->srv_rqbd_timeout,
		    psc_listhd_empty(&svc->srv_reply_queue), psc_listhd_empty(&svc->srv_request_queue),
		    svc->srv_n_active_reqs, svc->srv_nthreads,
		    (thr->pscthr_run != 0 && svc->srv_n_difficult_replies == 0),
		    (!psc_listhd_empty(&svc->srv_idle_rqbds) && svc->srv_rqbd_timeout == 0),
		    (!psc_listhd_empty(&svc->srv_request_queue) &&
			(svc->srv_n_difficult_replies == 0 ||
			 svc->srv_n_active_reqs < (svc->srv_nthreads - 1)))
		);
#endif
		(void)pscrpc_svr_wait_event(&svc->srv_waitq,
		    pscrpcthr_waitevent(thr, svc) || !prt->prt_alive,
		    &lwi, NULL);

		pscrpc_check_rqbd_pool(svc);
		/*
		 * this has to be mod'ed to support I/O threads
		 *  PUT'ing replies onto this list after they've sync'ed
		 *  the blocks to disk.. paul
		 */
		//if (!psc_listhd_empty(&svc->srv_reply_queue))
		//	pscrpc_server_handle_reply(svc);

		/* only handle requests if there are no difficult replies
		 * outstanding, or I'm not the last thread handling
		 * requests */
		if (!psc_listhd_empty_mutex_locked(&svc->srv_mutex,
		    &svc->srv_request_queue) &&
		    (svc->srv_n_difficult_replies == 0 ||
		     svc->srv_n_active_reqs < (svc->srv_nthreads - 1)))
			pscrpc_server_handle_request(svc, thr);

		if (!psc_listhd_empty_mutex_locked(&svc->srv_mutex,
		    &svc->srv_idle_rqbds) &&
		    pscrpc_server_post_idle_rqbds(svc) < 0) {
			/* I just failed to repost request buffers.  Wait
			 * for a timeout (unless something else happens)
			 * before I try again */
			svc->srv_rqbd_timeout = 10;
			CDEBUG(D_RPCTRACE, "Posted buffers: %d",
			       svc->srv_nrqbd_receiving);
		}
	}

	//lc_watchdog_delete(watchdog);

	// out_srv_init:
	/*
	 * deconstruct service specific state created by pscrpc_start_thread()
	 */
	if (svc->srv_done != NULL)
		svc->srv_done(thr);

 out:
	CDEBUG(D_NET, "service thread exiting: rc %d", rc);

	SVC_LOCK(svc);
	svc->srv_nthreads--;			/* must know immediately */
#if 0 //ptlrpc
	thr->t_id = rc;
	thr->t_flags = SVC_STOPPED;

	psc_waitq_wakeall(&thr->t_ctl_waitq);
#endif
	psclist_del(&prt->prt_lentry, &svc->srv_threads);
	psc_waitq_wakeall(&svc->srv_waitq);
	SVC_ULOCK(svc);
}

int
pscrpc_unregister_service(struct pscrpc_service *svc)
{
	struct pscrpc_request_buffer_desc *rqbd;
	struct pscrpc_reply_state *rs, *t;
	struct l_wait_info lwi;
	int rc;

	LASSERT(psc_listhd_empty(&svc->srv_threads));

	/* 05/08/2017: crash  psl_owner_file = 0xffffffffffffffff */
	spinlock(&pscrpc_all_services_lock);
	psclist_del(&svc->srv_lentry, &pscrpc_all_services);
	freelock(&pscrpc_all_services_lock);

	/*
	 * All history will be culled when the next request buffer is
	 * freed.
	 */
	svc->srv_max_history_rqbds = 0;

	CDEBUG(D_NET, "%s: tearing down", svc->srv_name);

	rc = LNetClearLazyPortal(svc->srv_req_portal);
	LASSERT(rc == 0);

	/*
	 * Unlink all the request buffers.  This forces a 'final' event with
	 * its 'unlink' flag set for each posted rqbd.
	 */
	psclist_for_each_entry(rqbd, &svc->srv_active_rqbds, rqbd_lentry) {
		rc = LNetMDUnlink(rqbd->rqbd_md_h);
		LASSERT(rc == 0 || rc == -ENOENT);
	}

	/* Wait for the network to release any buffers it's currently
	 * filling */
	for (;;) {
		SVC_LOCK(svc);
		rc = svc->srv_nrqbd_receiving;
		SVC_ULOCK(svc);

		if (rc == 0)
			break;

		/*
		 * Network access will complete in finite time but the HUGE
		 * timeout lets us CWARN for visibility of sluggish NALs.
		 */
		lwi = LWI_TIMEOUT(300 * 100, NULL, NULL);
		rc = pscrpc_svr_wait_event_mutex(&svc->srv_waitq,
		    svc->srv_nrqbd_receiving == 0, &lwi,
		    &svc->srv_mutex);
		if (rc == -ETIMEDOUT)
			CWARN("Service %s waiting for %d request buffers",
			      svc->srv_name, svc->srv_nrqbd_receiving);
	}

	/* schedule all outstanding replies to terminate them */
#if 0
	SVC_LOCK(svc);
	while (!psc_listhd_empty(&svc->srv_active_replies)) {
		rs = psc_listhd_first_obj(&svc->srv_active_replies,
			struct pscrpc_reply_state, rs_list_entry);
		CWARN("Active reply found?? %p", rs);
		//pscrpc_schedule_difficult_reply(rs);
	}
	SVC_ULOCK(svc);
#endif

	/*
	 * Purge the request queue.  NB No new replies (rqbds all unlinked)
	 * and no service threads, so I'm the only thread noodling the
	 * request queue now.
	 */
	while (!psc_listhd_empty(&svc->srv_request_queue)) {
		struct pscrpc_request *req = psc_listhd_first_obj(
		    &svc->srv_request_queue, struct pscrpc_request,
		    rq_lentry);

		psclist_del(&req->rq_lentry, &svc->srv_request_queue);
		svc->srv_n_queued_reqs--;
		svc->srv_n_active_reqs++;

		pscrpc_server_free_request(req);
	}
	LASSERT(svc->srv_n_queued_reqs == 0);
	LASSERT(svc->srv_n_active_reqs == 0);
	LASSERT(svc->srv_n_history_rqbds == 0);
	LASSERT(psc_listhd_empty(&svc->srv_active_rqbds));

	/* Now free all the request buffers since nothing references them
	 * any more... */
	while (!psc_listhd_empty(&svc->srv_idle_rqbds)) {
		rqbd = psc_listhd_first_obj(&svc->srv_idle_rqbds,
		    struct pscrpc_request_buffer_desc, rqbd_lentry);

		pscrpc_free_rqbd(rqbd);
	}

	/* wait for all outstanding replies to complete (they were
	 * scheduled having been flagged to abort above) */
	while (atomic_read(&svc->srv_outstanding_replies) != 0) {
		lwi = LWI_TIMEOUT(10 * 100, NULL, NULL);

		rc = pscrpc_svr_wait_event_mutex(&svc->srv_waitq,
		    !psc_listhd_empty(&svc->srv_reply_queue),
		    &lwi, &svc->srv_mutex);

		LASSERT(rc == 0 || rc == -ETIMEDOUT);

		if (rc == 0) {
			pscrpc_server_handle_reply(svc);
			continue;
		}
		CWARN("Unexpectedly long timeout %p", svc);
	}

	psclist_for_each_entry_safe(rs, t, &svc->srv_free_rs_list,
	    rs_list_entry) {
		psclist_del(&rs->rs_list_entry, &svc->srv_free_rs_list);
		PSCRPC_OBD_FREE(rs, svc->srv_max_reply_size);
	}

	pfl_poolmaster_destroy(&svc->srv_poolmaster);
	psc_mutex_destroy(&svc->srv_mutex);
	psc_waitq_destroy(&svc->srv_waitq);
	psc_waitq_destroy(&svc->srv_free_rs_waitq);
	PSCRPC_OBD_FREE(svc, sizeof(*svc));
	return 0;
}

int
pscrpc_peer_qlen_cmp(const void *a, const void *b)
{
	const struct pscrpc_peer_qlen *qa = a, *qb = b;

	return (memcmp(&qa->pql_id, &qb->pql_id, sizeof(qa->pql_id)));
}

struct pscrpc_service *
pscrpc_init_svc(int nbufs, int bufsize, int max_req_size,
    int max_reply_size, int req_portal, int rep_portal, char *name,
    svc_handler_t handler, int flags)
{
	struct pscrpc_service *svc;
	int rc;

	psclog_debug("bufsize %d max_req_size %d", bufsize,
	    max_req_size);

	LASSERT(nbufs > 0);
	LASSERT(bufsize >= max_req_size);

	PSCRPC_OBD_ALLOC(svc, sizeof(*svc));
	if (svc == NULL)
		return (NULL);

	/* First initialise enough for early teardown */

	svc->srv_name = name;
	psc_mutex_init(&svc->srv_mutex);
	INIT_PSCLIST_HEAD(&svc->srv_threads);
	psc_waitq_init(&svc->srv_waitq, "rpc-svc");

	svc->srv_nbuf_per_group = test_req_buffer_pressure ? 1 : nbufs;
	svc->srv_max_req_size = max_req_size;
	svc->srv_buf_size = bufsize;
	svc->srv_rep_portal = rep_portal;
	svc->srv_req_portal = req_portal;
	//svc->srv_watchdog_timeout = watchdog_timeout;
	svc->srv_handler = handler;
	//svc->srv_request_history_print_fn = svcreq_printfn;
	svc->srv_request_seq = 1;		/* valid seq #s start at 1 */
	svc->srv_request_max_cull_seq = 0;

	rc = LNetSetLazyPortal(svc->srv_req_portal);
	LASSERT(rc == 0);

	INIT_PSC_LISTENTRY(&svc->srv_lentry);
	INIT_PSCLIST_HEAD(&svc->srv_request_queue);
	INIT_PSCLIST_HEAD(&svc->srv_request_history);

	INIT_PSCLIST_HEAD(&svc->srv_idle_rqbds);
	INIT_PSCLIST_HEAD(&svc->srv_active_rqbds);
	INIT_PSCLIST_HEAD(&svc->srv_history_rqbds);

	//ATOMIC_INIT(&svc->srv_outstanding_replies);
	INIT_PSCLIST_HEAD(&svc->srv_active_replies);
	INIT_PSCLIST_HEAD(&svc->srv_reply_queue);

	INIT_PSCLIST_HEAD(&svc->srv_free_rs_list);

	psc_waitq_init(&svc->srv_free_rs_waitq, "reply-state");

	spinlock(&pscrpc_all_services_lock);
	psclist_add(&svc->srv_lentry, &pscrpc_all_services);
	freelock(&pscrpc_all_services_lock);

	psc_poolmaster_init(&svc->srv_poolmaster,
	    struct pscrpc_request_buffer_desc, rqbd_lentry, PPMF_AUTO,
	    64, 64, 0, NULL, "rqbd-%s", svc->srv_name);
	svc->srv_pool = psc_poolmaster_getmgr(
	    &svc->srv_poolmaster);

	/* Now allocate the request buffers */
	rc = pscrpc_grow_req_bufs(svc);
	/* We shouldn't be under memory pressure at startup, so
	 * fail if we can't post all our buffers at this time. */
	if (rc != 0)
		GOTO(failed, NULL);

	/* Now allocate pool of reply buffers */
	/* Increase max reply size to next power of two */
	svc->srv_max_reply_size = 1;
	while (svc->srv_max_reply_size < max_reply_size)
		svc->srv_max_reply_size <<= 1;

	if (flags & PSCRPC_SVCF_COUNT_PEER_QLENS) {
		svc->srv_count_peer_qlens = 1;
#define QLENTABSZ 511
		psc_hashtbl_init(&svc->srv_peer_qlentab, 0,
		    struct pscrpc_peer_qlen, pql_id, pql_hentry,
		    QLENTABSZ, pscrpc_peer_qlen_cmp, "qlen-%s",
		    svc->srv_name);
	}

	CDEBUG(D_NET, "%s: Started, listening on portal %d",
	       svc->srv_name, svc->srv_req_portal);

	return (svc);

 failed:
	pscrpc_unregister_service(svc);
	PSCRPC_OBD_FREE(svc, sizeof(*svc));
	return NULL;
}

int
pscrpcsvh_addthr(struct pscrpc_svc_handle *svh)
{
	struct pscrpc_service *svc;
	struct pscrpc_thread *prt;
	struct psc_thread *thr;

	svc = svh->svh_service;
	SVC_LOCK(svc);
	thr = pscthr_init(svh->svh_type, pscrpcthr_main,
	    svh->svh_thrsiz, "%sthr%02d", svh->svh_svc_name,
	    svh->svh_nthreads);
	thr->pscthr_flags |= PTF_RPC_SVC_THREAD;
	prt = thr->pscthr_private;
	prt->prt_alive = 1;
	prt->prt_svh = svh;
	INIT_PSC_LISTENTRY(&prt->prt_lentry);

	psclist_add(&prt->prt_lentry,
	    &svh->svh_service->srv_threads);
	svh->svh_nthreads++;
	SVC_ULOCK(svc);
	pscthr_setready(thr);
	return (0);
}

int
pscrpcsvh_delthr(struct pscrpc_svc_handle *svh)
{
	struct pscrpc_service *svc;
	struct pscrpc_thread *prt;
	int rc;

	rc = 0;
	svc = svh->svh_service;
	SVC_LOCK(svc);
	if (svc->srv_nthreads == 0)
		rc = -1;
	else {
		prt = psc_listhd_first_obj(&svc->srv_threads,
		    struct pscrpc_thread, prt_lentry);
		prt->prt_alive = 0;
	}
	psc_waitq_wakeall(&svc->srv_waitq);
	SVC_ULOCK(svc);
	return (rc);
}

/*
 * Create an RPC service.
 * @svh: an initialized service handle structure which holds the
 * service's relevant information.
 */
void
_pscrpc_svh_spawn(struct pscrpc_svc_handle *svh)
{
	int i, n;

	svh->svh_service = pscrpc_init_svc(svh->svh_nbufs,
	    svh->svh_bufsz, svh->svh_reqsz, svh->svh_repsz,
	    svh->svh_req_portal, svh->svh_rep_portal, svh->svh_svc_name,
	    svh->svh_handler, svh->svh_flags);

	psc_assert(svh->svh_service);

	/* Track the service handle */
	INIT_PSC_LISTENTRY(&svh->svh_lentry);
	psclist_add(&svh->svh_lentry, &pscrpc_svh_list);

	n = svh->svh_nthreads;
	svh->svh_nthreads = 0;
	for (i = 0; i < n; i++)
		pscrpcsvh_addthr(svh);
}

void
pscrpc_svh_destroy(struct pscrpc_svc_handle *svh)
{
	while (svh->svh_service->srv_nthreads)
		pscrpcsvh_delthr(svh);
	pscrpc_unregister_service(svh->svh_service);
}

#ifdef PFL_CTL

/*
 * Respond to a "GETRPCSVC" control inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
int
psc_ctlrep_getrpcsvc(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_rpcsvc *pcrs = m;
	struct pscrpc_service *s;
	int rc;

	rc = 1;

	spinlock(&pscrpc_all_services_lock);
	psclist_for_each_entry(s, &pscrpc_all_services, srv_lentry) {
		SVC_LOCK(s);
		strlcpy(pcrs->pcrs_name, s->srv_name,
		    sizeof(pcrs->pcrs_name));
		pcrs->pcrs_rqptl = s->srv_req_portal;
		pcrs->pcrs_rpptl = s->srv_rep_portal;
		pcrs->pcrs_rqsz = s->srv_max_req_size;;
		pcrs->pcrs_rpsz = s->srv_max_reply_size;
		pcrs->pcrs_bufsz = s->srv_buf_size;
		pcrs->pcrs_nbufs = s->srv_nbufs;
		pcrs->pcrs_nque = s->srv_n_queued_reqs;
		pcrs->pcrs_nact = s->srv_n_active_reqs;
		pcrs->pcrs_nthr = s->srv_nthreads;
		pcrs->pcrs_nrep = atomic_read(&s->srv_outstanding_replies);
		pcrs->pcrs_nrqbd = s->srv_nrqbd_receiving;
		pcrs->pcrs_nwq = psc_waitq_nwaiters(&s->srv_waitq);
		if (s->srv_count_peer_qlens)
			pcrs->pcrs_flags |= PSCRPC_SVCF_COUNT_PEER_QLENS;
		SVC_ULOCK(s);

		rc = psc_ctlmsg_sendv(fd, mh, pcrs, NULL);
		if (!rc)
			break;
	}
	freelock(&pscrpc_all_services_lock);
	return (rc);
}

void
pflrpc_register_ctlops(struct psc_ctlop *ops)
{
	struct psc_ctlop *op;

	op = &ops[PCMT_GETLNETIF];
	op->pc_op = psc_ctlrep_getlnetif;
	op->pc_siz = sizeof(struct psc_ctlmsg_lnetif);

	op = &ops[PCMT_GETRPCRQ];
	op->pc_op = psc_ctlrep_getrpcrq;
	op->pc_siz = sizeof(struct psc_ctlmsg_rpcrq);

	op = &ops[PCMT_GETRPCSVC];
	op->pc_op = psc_ctlrep_getrpcsvc;
	op->pc_siz = sizeof(struct psc_ctlmsg_rpcsvc);
} 

#endif
