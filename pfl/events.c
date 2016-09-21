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
#include "pfl/completion.h"
#include "pfl/export.h"
#include "pfl/log.h"
#include "pfl/pool.h"
#include "pfl/rpc.h"
#include "pfl/rpclog.h"
#include "pfl/service.h"
#include "pfl/types.h"
#include "pfl/waitq.h"

#include "../ulnds/socklnd/usocklnd.h"

int			 pfl_rpc_timeout = PSCRPC_TIMEOUT;
int			 pfl_rpc_max_retry = PSCRPC_MAX_RETRIES;

lnet_handle_eq_t	 pscrpc_eq_h;
struct psclist_head	 pscrpc_wait_callbacks;

struct psc_poolmaster	 pscrpc_export_poolmaster;
struct psc_poolmaster	 pscrpc_conn_poolmaster;
struct psc_poolmaster	 pscrpc_imp_poolmaster;
struct psc_poolmaster	 pscrpc_set_poolmaster;
struct psc_poolmaster	 pscrpc_rq_poolmaster;

struct psc_poolmgr	*pscrpc_export_pool;
struct psc_poolmgr	*pscrpc_conn_pool;
struct psc_poolmgr	*pscrpc_imp_pool;
struct psc_poolmgr	*pscrpc_set_pool;
struct psc_poolmgr	*pscrpc_rq_pool;

struct pfl_opstats_grad pfl_rpc_client_request_latencies;

int64_t pfl_rpc_client_request_latency_durations[] = {
	0,
	1,
	5,
	10,
	20,
	30,
	40,
	50,
};

struct pfl_opstats_grad pfl_rpc_service_reply_latencies;

int64_t pfl_rpc_service_reply_latency_durations[] = {
	0,
	1,
	5,
	10,
	20,
	30,
	40,
	50,
};

/*
 *  Client's outgoing request callback
 */
void
pscrpc_request_out_callback(lnet_event_t *ev)
{
	int silent;
	char buf[PSCRPC_NIDSTR_SIZE];
	struct pscrpc_cb_id   *cbid = ev->md.user_ptr;
	struct pscrpc_request *req = cbid->cbid_arg;

	LASSERT(ev->type == LNET_EVENT_SEND ||
		ev->type == LNET_EVENT_UNLINK);
	LASSERT(ev->unlinked);

	if (ev->type == LNET_EVENT_UNLINK || ev->status != 0) {
		/*
		 * Failed send: make it seem like the reply timed out, just
		 * like failing sends in rpcclient.c does currently...
		 */
		spinlock(&req->rq_lock);
		silent = req->rq_silent_timeout;
		req->rq_net_err = 1;
		req->rq_abort_reply = 1;
		req->rq_abort_reply = 1;
		freelock(&req->rq_lock);

		pscrpc_wake_client_req(req);
	}

	if (!silent)
		DEBUG_REQ(ev->status ? PLL_ERROR : PLL_DIAG, req, buf,
		    "type %d, status %d", ev->type, ev->status);

	/* these balance the references in ptl_send_rpc() */
	atomic_dec(&req->rq_import->imp_inflight);
	pscrpc_req_finished(req);
}

void
pscrpc_bump_peer_qlen(void *p, __unusedx void *arg)
{
	struct pscrpc_peer_qlen *pq = p;

	atomic_inc(&pq->pql_qlen);
}

/*
 * Server's incoming request callback
 */
void
pscrpc_request_in_callback(lnet_event_t *ev)
{
	struct pscrpc_cb_id               *cbid = ev->md.user_ptr;
	struct pscrpc_request_buffer_desc *rqbd = cbid->cbid_arg;
	struct pscrpc_service             *svc = rqbd->rqbd_service;
	struct pscrpc_request             *req;

	LASSERT(ev->type == LNET_EVENT_PUT ||
		 ev->type == LNET_EVENT_UNLINK);
	LASSERT((char *)ev->md.start >= rqbd->rqbd_buffer);
	LASSERT((char *)ev->md.start + ev->offset + ev->mlength <=
		 rqbd->rqbd_buffer + svc->srv_buf_size);

	if (!ev->status)
		psclog_trace("event type %d, status %d, service %s",
		    ev->type, ev->status, svc->srv_name);
	else
		psclog_errorx("event type %d, status %d, service %s",
		    ev->type, ev->status, svc->srv_name);

	if (ev->unlinked) {
		/* If this is the last request message to fit in the
		 * request buffer we can use the request object embedded in
		 * rqbd.  Note that if we failed to allocate a request,
		 * we'd have to re-post the rqbd, which we can't do in this
		 * context. */
		req = &rqbd->rqbd_req;
		memset(req, 0, sizeof(*req));
	} else {
		LASSERT (ev->type == LNET_EVENT_PUT);
		if (ev->status != 0) {
			/* We moaned above already... */
			return;
		}
		req = psc_pool_get(pscrpc_rq_pool);
	}

	/* NB we ABSOLUTELY RELY on req being zeroed, so pointers are NULL,
	 * flags are reset and scalars are zero.  We only set the message
	 * size to non-zero if this was a successful receive. */
	INIT_PSC_LISTENTRY(&req->rq_global_lentry);
	INIT_PSC_LISTENTRY(&req->rq_history_lentry);
	INIT_PSC_LISTENTRY(&req->rq_lentry);
	req->rq_xid = ev->match_bits;
	req->rq_reqmsg = ev->md.start + ev->offset;
	if (ev->type == LNET_EVENT_PUT && ev->status == 0)
		req->rq_reqlen = ev->mlength;
	do_gettimeofday(&req->rq_arrival_time);
	req->rq_peer = ev->initiator;
	req->rq_self = ev->target.nid;
	req->rq_rqbd = rqbd;
	req->rq_phase = PSCRPC_RQ_PHASE_NEW;
#ifdef CRAY_XT3
	//req->rq_uid = ev->uid;
#endif

	pll_add(&pscrpc_requests, req);

	SVC_LOCK(svc);

	req->rq_history_seq = svc->srv_request_seq++;
	psclist_add_tail(&req->rq_history_lentry, &svc->srv_request_history);

	if (ev->unlinked) {
		svc->srv_nrqbd_receiving--;
		CDEBUG(D_RPCTRACE, "Buffer complete: %d buffers still posted (%s)",
		    svc->srv_nrqbd_receiving, svc->srv_name);

		if (!svc->srv_nrqbd_receiving)
			CERROR("Service %s, all request buffers are busy",
			    svc->srv_name);
#if 0
		/* Normally, don't complain about 0 buffers posted; LNET won't
		 * drop incoming reqs since we set the portal lazy */
		if (test_req_buffer_pressure &&
		    ev->type != LNET_EVENT_UNLINK &&
		    svc->srv_nrqbd_receiving == 0)
			CWARN("All %s request buffers busy",
			    svc->srv_name);
#endif
		/* req takes over the network's ref on rqbd */
	} else {
		/* req takes a ref on rqbd */
		rqbd->rqbd_refcount++;
	}

	psclist_add_tail(&req->rq_lentry, &svc->srv_request_queue);
	svc->srv_n_queued_reqs++;

	/* count the RPC request queue length for this peer if enabled */
	if (svc->srv_count_peer_qlens) {
		struct pscrpc_peer_qlen *pq;

		pq = psc_hashtbl_search_cmpcb(&svc->srv_peer_qlentab,
		    &req->rq_peer, pscrpc_bump_peer_qlen, NULL,
		    &req->rq_peer.nid);
		if (pq == NULL) {
			struct pscrpc_peer_qlen *tpq;
			struct psc_hashbkt *b;

			tpq = PSCALLOC(sizeof(*tpq));
			psc_hashent_init(&svc->srv_peer_qlentab, tpq);
			tpq->pql_id = req->rq_peer;
			atomic_set(&tpq->pql_qlen, 1);

			/*
			 * Search again in case it was created by
			 * another thread in the interim.
			 */
			b = psc_hashbkt_get(&svc->srv_peer_qlentab,
			    &req->rq_peer.nid);
			pq = psc_hashbkt_search_cmpcb(
			    &svc->srv_peer_qlentab, b, &req->rq_peer,
			    pscrpc_bump_peer_qlen, NULL,
			    &req->rq_peer.nid);
			if (pq == NULL) {
				psc_hashbkt_add_item(
				    &svc->srv_peer_qlentab, b, tpq);
				pq = tpq;
				tpq = NULL;
			}
			psc_hashbkt_put(&svc->srv_peer_qlentab, b);

			PSCFREE(tpq);
		}
		req->rq_peer_qlen = pq;
	}

	/* NB everything can disappear under us once the request
	 * has been queued and we unlock, so do the wake now... */
	psc_waitq_wakeall(&svc->srv_waitq);

	SVC_ULOCK(svc);
}

/*
 * Client's bulk has been written/read
 */
void
pscrpc_client_bulk_callback(lnet_event_t *ev)
{
	struct pscrpc_cb_id     *cbid = ev->md.user_ptr;
	struct pscrpc_bulk_desc *desc = cbid->cbid_arg;

	LASSERT((desc->bd_type == BULK_PUT_SINK &&
		  ev->type == LNET_EVENT_PUT) ||
		 (desc->bd_type == BULK_GET_SOURCE &&
		  ev->type == LNET_EVENT_GET) ||
		 ev->type == LNET_EVENT_UNLINK);
	LASSERT(ev->unlinked);

	CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
	       "event type %d, status %d, desc %p x%"PRId64,
	       ev->type, ev->status, desc, desc->bd_req->rq_xid);

	spinlock(&desc->bd_lock);

	LASSERT(desc->bd_network_rw);
	desc->bd_network_rw = 0;

	if (ev->type != LNET_EVENT_UNLINK && ev->status == 0) {
		desc->bd_success = 1;
		desc->bd_nob_transferred = ev->mlength;
	}
	/*
	 * NB don't unlock till after wakeup; desc can disappear under us
	 * otherwise */
	pscrpc_wake_client_req(desc->bd_req);

	freelock(&desc->bd_lock);
}

void
pscrpc_reply_in_callback(lnet_event_t *ev)
{
	struct pscrpc_cb_id   *cbid = ev->md.user_ptr;
	struct pscrpc_request *req = cbid->cbid_arg;
	char buf[PSCRPC_NIDSTR_SIZE];

	LASSERT(ev->type == LNET_EVENT_PUT ||
		ev->type == LNET_EVENT_UNLINK);
	LASSERT(ev->unlinked);
	LASSERT(ev->md.start == req->rq_repmsg);
	LASSERT(ev->offset == 0);
	LASSERT(ev->mlength <= (uint32_t)req->rq_replen);

	DEBUG_REQ(ev->status ? PLL_ERROR : PLL_DIAG, req, buf,
		  "type=%d status=%d initiator=%s",
		  ev->type, ev->status, libcfs_id2str(ev->initiator));
	psclog_debug("event: type=%d status=%d offset=%d mlength=%d",
	    ev->type, ev->status, ev->offset, ev->mlength);

	if (!req->rq_peer.nid)
		req->rq_peer = ev->initiator;

	spinlock(&req->rq_lock);

#if 0 /* this fails in a recovery scenario */
	LASSERT(req->rq_receiving_reply);
#endif
	req->rq_receiving_reply = 0;

	if (ev->type == LNET_EVENT_PUT && ev->status == 0) {
		struct timespec ts, tmp;

		req->rq_replied = 1;
		req->rq_nob_received = ev->mlength;

		PFL_GETTIMESPEC(&ts);
		timespecsub(&ts, &req->rq_sent_ts, &tmp);
		/*
 		 * 09/21/2016: hit segfault during kernel 4.4 build:
 		 *
 		 * {tv_sec = -1, tv_nsec = 692443857}
 		 *
 		 * usocklnd_read_handler() --> usocklnd_read_msg() --> 
 		 * lnet_finalize() --> lnet_enq_event_locked() -->
 		 * pscrpc_master_callback() --> pscrpc_reply_in_callback().
 		 *
 		 */
		if (tmp.tv_sec >= 0)
			pfl_opstats_grad_incr(&pfl_rpc_client_request_latencies, 
			    tmp.tv_sec);
	}

	if (req->rq_compl)
		/*
		 * Notify upper layer that an RPC is ready to be
		 * finalized.
		 */
		psc_compl_one(req->rq_compl, 1);

	if (req->rq_waitq)
		psc_waitq_wakeall(req->rq_waitq);

	/* NB don't unlock till after wakeup; req can disappear under us
	 * since we don't have our own ref */
	pscrpc_wake_client_req(req);

	freelock(&req->rq_lock);
}

/*
 *  Server's outgoing reply callback
 */
void
pscrpc_reply_out_callback(lnet_event_t *ev)
{
	struct pscrpc_cb_id       *cbid = ev->md.user_ptr;
	struct pscrpc_reply_state *rs = cbid->cbid_arg;
	struct pscrpc_service     *svc = rs->rs_service;

	LASSERT(ev->type == LNET_EVENT_SEND ||
		ev->type == LNET_EVENT_ACK ||
		ev->type == LNET_EVENT_UNLINK);

	if (rs->rs_compl)
		psc_compl_one(rs->rs_compl, 1);

	if (!rs->rs_difficult) {
		/* 'Easy' replies have no further processing so I drop the
		 * net's ref on 'rs' */
		LASSERT(ev->unlinked);
		pscrpc_rs_decref(rs);
		atomic_dec(&svc->srv_outstanding_replies);
		return;
	}

	LASSERT(rs->rs_on_net);

	if (ev->unlinked) {
		/* Last network callback.  The net's ref on 'rs' stays put
		 * until pscrpc_server_handle_reply() is done with it */
		SVC_LOCK(svc);
		rs->rs_on_net = 0;
#if 0
		// not sure if we're going to need these
		//  pauln - 05082007
		pscrpc_schedule_difficult_reply(rs);
#endif
		SVC_ULOCK(svc);
	}
}

/*
 * Server's bulk completion callback
 */
void
pscrpc_server_bulk_callback(lnet_event_t *ev)
{
	struct pscrpc_cb_id     *cbid = ev->md.user_ptr;
	struct pscrpc_bulk_desc *desc = cbid->cbid_arg;

	LASSERT(ev->type == LNET_EVENT_SEND ||
		ev->type == LNET_EVENT_UNLINK ||
		(desc->bd_type == BULK_PUT_SOURCE &&
		 ev->type == LNET_EVENT_ACK) ||
		(desc->bd_type == BULK_GET_SINK &&
		 ev->type == LNET_EVENT_REPLY));

	CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
	       "event type %d, status %d, desc %p",
	       ev->type, ev->status, desc);

	spinlock(&desc->bd_lock);

	if ((ev->type == LNET_EVENT_ACK ||
	     ev->type == LNET_EVENT_REPLY) &&
	    ev->status == 0) {
		/* We heard back from the peer, so even if we get this
		 * before the SENT event (oh yes we can), we know we
		 * read/wrote the peer buffer and how much... */
		desc->bd_success = 1;
		desc->bd_nob_transferred = ev->mlength;
	}

	if (ev->unlinked) {
		/* This is the last callback no matter what... */
		desc->bd_network_rw = 0;
		psc_waitq_wakeall(&desc->bd_waitq);
	}

	freelock(&desc->bd_lock);
}

static void
pscrpc_drop_callback(lnet_event_t *ev)
{
	pscrpc_drop_conns(&ev->initiator);
}

/**
 * pscrpc_master_callback - client and server master callback.
 * In server context, pscrpc_master_callback is called within lnet via
 * the callback handler in LNetEQPoll.  The client is single threaded
 * so there is only one blocking routing for all lnet activities
 * (pscrpc_check_events).
 *
 * In client context, pscrpc_check_events is responsible for calling
 * pscrpc_master_callback().
 * @ev: the event passed up from lnet
 */
static void
pscrpc_master_callback(lnet_event_t *ev)
{
	struct pscrpc_cb_id *cbid;
	void (*callback)(lnet_event_t *);

	if (ev->type == LNET_EVENT_DROP) {
		pscrpc_drop_callback(ev);
		return;
	}

	cbid = ev->md.user_ptr;
	callback = cbid->cbid_fn;
	/* Honestly, it's best to find out early. */
	LASSERT(cbid->cbid_arg != LP_POISON);
	LASSERT(callback == pscrpc_request_out_callback ||
		 callback == pscrpc_reply_in_callback    ||
		 callback == pscrpc_client_bulk_callback ||
		 callback == pscrpc_request_in_callback  ||
		 callback == pscrpc_reply_out_callback   ||
		 callback == pscrpc_server_bulk_callback);

	callback(ev);
}

#if 0
/**
 * pscrpc_register_wait_callback - put a client's callback handler onto the main callback list.
 *
 * NOTES: Client context only
 */
void *
pscrpc_register_wait_callback(int (*fn)(void *arg), void *arg)
{
	struct pscrpc_wait_callback *llwc = NULL;

	llwc = PSCALLOC(sizeof(*llwc));

	llwc->llwc_fn = fn;
	llwc->llwc_arg = arg;
	psclist_add_tail(&llwc->llwc_list, &pscrpc_wait_callbacks);

	return (llwc);
}

/**
 * pscrpc_deregister_wait_callback - remove a client calback handler
 *
 * NOTES: Client context only
 */
void
pscrpc_deregister_wait_callback(void *opaque)
{
	struct pscrpc_wait_callback *llwc = opaque;

	psclist_del(&llwc->llwc_list);
	PSCRPC_OBD_FREE(llwc, sizeof(*llwc));
}
#endif

/**
 * pscrpc_check_events - check event queue by calling LNetEQPoll, if an
 * event is found call pscrpc_master_callback().  This call implies
 * single threaded operation.
 * @timeout: number of seconds to block (0 is forever i think)
 * NOTES: Client context only
 */
int
pscrpc_check_events(int timeout)
{
	lnet_event_t ev;
	int         rc;
	int         i;

	psclog_trace("timeo_ms %d", timeout * 1000);
	rc = LNetEQPoll(&pscrpc_eq_h, 1, timeout * 1000, &ev, &i);
	if (rc == 0)
		return (0);

	LASSERT(rc == -EOVERFLOW || rc == 1);

	/* liblustre: no asynch callback so we can't affort to miss any
	 * events... */
	if (rc == -EOVERFLOW) {
		CERROR("Dropped an event!!!");
		abort();
	}

	pscrpc_master_callback(&ev);
	return (1);
}

/**
 * pscrpc_wait_event - called from the macro pscrpc_cli_wait_event(),
 *	calls pscrpc_check_events().
 * See pscrpc_cli_wait_event() for more detail.
 * @timeout: number of seconds to block (0 is forever i think)
 * Returns: 0 if nothing found, 1 if something found, -ETIMEDOUT on timeout.
 * NOTES: Client context only
 */
int
pscrpc_wait_event(int timeout)
{
#if CLIENT_IS_A_SERVER_TOO
	struct pscrpc_wait_callback *llwc;
	extern struct psclist_head   pscrpc_wait_callbacks;
#endif
	int                          found_something = 0;

	/* single threaded recursion check... */
	//liblustre_waiting = 1;

	for (;;) {
		/* Deal with all pending events */
		while (pscrpc_check_events(0))
			found_something = 1;

#if CLIENT_IS_A_SERVER_TOO
		/* Give all registered callbacks a bite at the cherry */
		psclist_for_each_entry(llwc, &pscrpc_wait_callbacks, llwc_lentry) {
			if (llwc->llwc_fn(llwc->llwc_arg))
				found_something = 1;
		}
#endif

		if (found_something || timeout == 0)
			break;

		/* Nothing so far, but I'm allowed to block... */
		found_something = pscrpc_check_events(timeout);
		if (!found_something)           /* still nothing */
			return -ETIMEDOUT;
	}

	//liblustre_waiting = 0;

	return found_something;
}

lnet_pid_t
pscrpc_get_pid(void)
{
	return (LUSTRE_SRV_LNET_PID);
}

int
pscrpc_ni_init(int type, int nmsgs)
{
	int               rc;
	lnet_process_id_t my_id;

	psc_poolmaster_init(&pscrpc_export_poolmaster,
	    struct pscrpc_export, exp_lentry, PPMF_AUTO, 64, 64, 0,
	    NULL, "rpcexp");
	pscrpc_export_pool = psc_poolmaster_getmgr(&pscrpc_export_poolmaster);

	psc_poolmaster_init(&pscrpc_conn_poolmaster,
	    struct pscrpc_connection, c_hentry.phe_lentry, PPMF_AUTO, 64, 64, 0,
	    NULL, "rpcconn");
	pscrpc_conn_pool = psc_poolmaster_getmgr(&pscrpc_conn_poolmaster);

	psc_poolmaster_init(&pscrpc_set_poolmaster,
	    struct pscrpc_request_set, set_lentry, PPMF_AUTO, 64, 64, 0,
	    NULL, "rpcset");
	pscrpc_set_pool = psc_poolmaster_getmgr(&pscrpc_set_poolmaster);

	psc_poolmaster_init(&pscrpc_imp_poolmaster,
	    struct pscrpc_import, imp_lentry, PPMF_AUTO, 64, 64, 0,
	    NULL, "rpcimp");
	pscrpc_imp_pool = psc_poolmaster_getmgr(&pscrpc_imp_poolmaster);

	psc_poolmaster_init(&pscrpc_rq_poolmaster,
	    struct pscrpc_request, rq_lentry, PPMF_AUTO, 64, 64, 0,
	    NULL, "rpcrq");
	pscrpc_rq_pool = psc_poolmaster_getmgr(&pscrpc_rq_poolmaster);

	pscrpc_conns_init();

	rc = LNetInit(nmsgs);
	if (rc)
		psc_fatalx("failed to initialize LNET (%d)", rc);

	/*
	 * CAVEAT EMPTOR: how we process portals events is _radically_
	 * different depending on...
	 */
	if (type == PSCNET_SERVER) {
		/*
		 * Kernel portals calls our master callback when events are added to
		 * the event queue.  In fact lustre never pulls events off this queue,
		 * so it's only sized for some debug history.
		 */
		lnet_server_mode();
		psclog_info("Requesting PID %u", PSCRPC_SVR_PID);
		rc = LNetNIInit(PSCRPC_SVR_PID);
		if (rc)
			errx(1, "failed LNetNIInit() (rc=%d)", rc);

		rc = LNetEQAlloc(1024, pscrpc_master_callback, &pscrpc_eq_h);
		psclog_info("%#"PRIx64" pscrpc_eq_h cookie value",
		    pscrpc_eq_h.cookie);

	} else if (type == PSCNET_MTCLIENT) {
		rc = LNetNIInit(pscrpc_get_pid());
		if (rc)
			errx(1, "failed LNetNIInit() (rc=%d)", rc);

		rc = LNetEQAlloc(1024, pscrpc_master_callback, &pscrpc_eq_h);
		psclog_info("%#"PRIx64" pscrpc_eq_h cookie value",
		    pscrpc_eq_h.cookie);
	} else {
		/*
		 * liblustre calls the master callback when it removes events from the
		 * event queue.  The event queue has to be big enough not to drop
		 * anything.
		 */
		rc = LNetNIInit(pscrpc_get_pid());
		if (rc)
			errx(1, "failed LNetNIInit() (rc=%d)", rc);
		rc = LNetEQAlloc(10240, 0, &pscrpc_eq_h);
	}

	psc_assert(rc == 0);

	if (LNetGetId(1, &my_id))
		psc_fatalx("LNetGetId() failed");

	psclog_debug("nidpid is (%"PSCPRIxLNID",0x%x)", my_id.nid, my_id.pid);

	if (rc == 0)
		return 0;

	CERROR("Failed to allocate event queue: %d", rc);
	LNetNIFini();

	return (-ENOMEM);
}

void
pscrpc_ni_fini(void)
{
	struct psc_waitq    waitq;
	struct l_wait_info  lwi;
	int                 rc;
	int                 retries;

	/* Wait for the event queue to become idle since there may still be
	 * messages in flight with pending events (i.e. the fire-and-forget
	 * messages == client requests and "non-difficult" server
	 * replies */

	for (retries = 0;; retries++) {
		rc = LNetEQFree(pscrpc_eq_h);
		switch (rc) {
		default:
			LBUG();

		case 0:
			LNetNIFini();
			return;

		case -EBUSY:
			if (retries != 0)
				CWARN("Event queue still busy");

			/* Wait for a bit */
			psc_waitq_init(&waitq, "rpc events");
			lwi = LWI_TIMEOUT(2, NULL, NULL);
			(void)pscrpc_svr_wait_event(&waitq, 0, &lwi, NULL);
			break;
		}
	}
	/* NOTREACHED */
}

const char *
pflrpc_log_get_peer_addr(struct psc_thread *thr)
{
	struct pscrpc_thread *prt;
	const char *s;

	if (!(thr->pscthr_flags & PTF_RPC_SVC_THREAD))
		return ("");

	prt = pscrpcthr(thr);
	if (prt->prt_peer_addrbuf[0] == '\0' &&
	    prt->prt_peer_addr != LNET_NID_ANY)
		pscrpc_nid2str(prt->prt_peer_addr,
		    prt->prt_peer_addrbuf);
	s = prt->prt_peer_addrbuf;
	return (s);
}

void
pscrpc_init_portals(int type, int nmsgs)
{
	int rc;

	if (getenv("LNET_NETWORKS") == NULL &&
	    getenv("LNET_IP2NETS") == NULL)
		psc_fatalx("please export LNET_NETWORKS or LNET_IP2NETS");

	rc = pscrpc_ni_init(type, nmsgs);
	if (rc)
		psc_fatalx("network initialization: %s", strerror(-rc));

	pflog_get_peer_addr = pflrpc_log_get_peer_addr;

	pfl_opstats_grad_init(&pfl_rpc_client_request_latencies,
	    OPSTF_BASE10,
	    pfl_rpc_client_request_latency_durations,
	    nitems(pfl_rpc_client_request_latency_durations),
	    "rpc-request-latency:%ss");

	pfl_opstats_grad_init(&pfl_rpc_service_reply_latencies,
	    OPSTF_BASE10,
	    pfl_rpc_service_reply_latency_durations,
	    nitems(pfl_rpc_service_reply_latency_durations),
	    "rpc-reply-latency:%ss");
}

void
pscrpc_exit_portals(void)
{
	pfl_opstats_grad_destroy(&pfl_rpc_client_request_latencies);
	pfl_opstats_grad_destroy(&pfl_rpc_service_reply_latencies);

	pflog_get_peer_addr = NULL;

	pscrpc_conns_destroy();
	pscrpc_ni_fini();
	pfl_poolmaster_destroy(&pscrpc_export_poolmaster);
	pfl_poolmaster_destroy(&pscrpc_conn_poolmaster);
	pfl_poolmaster_destroy(&pscrpc_set_poolmaster);
	pfl_poolmaster_destroy(&pscrpc_imp_poolmaster);
	pfl_poolmaster_destroy(&pscrpc_rq_poolmaster);
}
