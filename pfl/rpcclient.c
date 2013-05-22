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
#include <stdlib.h>

#include "pfl/list.h"
#include "psc_rpc/export.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/pool.h"
#include "psc_util/waitq.h"

static uint64_t		pscrpc_last_xid = 0;
static psc_spinlock_t	pscrpc_last_xid_lock = SPINLOCK_INIT;

uint64_t
pscrpc_next_xid(void)
{
	uint64_t tmp;

	spinlock(&pscrpc_last_xid_lock);
	tmp = ++pscrpc_last_xid;
	freelock(&pscrpc_last_xid_lock);
	return tmp;
}

uint64_t
pscrpc_sample_next_xid(void)
{
	uint64_t tmp;

	spinlock(&pscrpc_last_xid_lock);
	tmp = pscrpc_last_xid + 1;
	freelock(&pscrpc_last_xid_lock);
	return tmp;
}

struct pscrpc_request *
pscrpc_request_addref(struct pscrpc_request *req)
{
	atomic_inc(&req->rq_refcount);
	return (req);
}

struct pscrpc_import *
pscrpc_new_import(void)
{
	struct pscrpc_import *imp;

	PSCRPC_OBD_ALLOC(imp, sizeof(*imp));
	if (imp == NULL)
		return NULL;

	PSCRPC_OBD_ALLOC(imp->imp_client, sizeof(*imp->imp_client));
	if (imp->imp_client == NULL) {
		PSCRPC_OBD_FREE(imp, sizeof(*imp));
		return (NULL);
	}

	//INIT_PSCLIST_HEAD(&imp->imp_replay_list);
	INIT_PSCLIST_HEAD(&imp->imp_sending_list);
	//INIT_PSCLIST_HEAD(&imp->imp_delayed_list);
	INIT_SPINLOCK(&imp->imp_lock);
	//imp->imp_last_success_conn = 0;
	imp->imp_state = PSCRPC_IMP_NEW;
	//imp->imp_obd = class_incref(obd);
	psc_waitq_init(&imp->imp_recovery_waitq);

	atomic_set(&imp->imp_refcount, 2);
	atomic_set(&imp->imp_inflight, 0);
	//atomic_set(&imp->imp_replay_inflight, 0);
	//INIT_PSCLIST_HEAD(&imp->imp_handle.h_link);
	//class_handle_hash(&imp->imp_handle, import_handle_addref);

	return imp;
}

struct pscrpc_import *
pscrpc_import_get(struct pscrpc_import *import)
{
	psc_assert(atomic_read(&import->imp_refcount) >= 0);
	psc_assert(atomic_read(&import->imp_refcount) < 0x5a5a5a);
	atomic_inc(&import->imp_refcount);
	psclog_info("import get %p refcount=%d", import,
	    atomic_read(&import->imp_refcount));
	return import;
}

void
pscrpc_import_put(struct pscrpc_import *import)
{
	psclog_dbg("import put %p refcount=%d", import,
	    atomic_read(&import->imp_refcount) - 1);

	psc_assert(atomic_read(&import->imp_refcount) > 0);
	psc_assert(atomic_read(&import->imp_refcount) < 0x5a5a5a);
	if (!atomic_dec_and_test(&import->imp_refcount))
		return;
	psclog_dbg("destroying import %p", import);

	/* XXX what if we fail to establish a connect for a new import */
	psc_assert(import->imp_connection);
	pscrpc_put_connection(import->imp_connection);
	psc_waitq_destroy(&import->imp_recovery_waitq);
	PSCRPC_OBD_FREE(import->imp_client, sizeof(*import->imp_client));
	PSCRPC_OBD_FREE(import, sizeof(*import));
}

__static int
pscrpc_interpret(struct pscrpc_request *rq)
{
	//psc_assert(rq->rq_phase == PSCRPC_RQ_PHASE_COMPLETE);
	if (rq->rq_interpret_reply)
		rq->rq_status = rq->rq_interpret_reply(rq, &rq->rq_async_args);

	rq->rq_interpret_reply = NULL;

	/* If request CB has been run then decrement the completion
	 *   so that set ops don't spin.
	 */
	if (rq->rq_compl)
		rq->rq_compl = NULL;

	return (rq->rq_status);
}

struct pscrpc_request *
pscrpc_prep_req_pool(struct pscrpc_import *imp, uint32_t version,
    int opcode, int count, int *lengths, char **bufs,
    __unusedx struct pscrpc_request_pool *pool)
{
	struct pscrpc_request *request = NULL;
	int rc;

	psc_assert((unsigned long)imp > 0x1000);
	psc_assert(imp != LP_POISON);
	psc_assert((unsigned long)imp->imp_client > 0x1000);
	psc_assert(imp->imp_client != LP_POISON);

	request = psc_pool_get(pscrpc_rq_pool);

	if (!request) {
		CERROR("request allocation out of memory\n");
		return (NULL);
	}
	memset(request, 0, sizeof(*request));

	rc = pscrpc_pack_request(request, count, lengths, bufs);
	if (rc) {
		psc_assert(!request->rq_pool);
		psc_pool_return(pscrpc_rq_pool, request);
		return (NULL);
	}

	psclog_info("request %p request->rq_reqmsg %p",
	    request, request->rq_reqmsg);

	request->rq_reqmsg->version |= version;

	if (imp->imp_server_timeout)
		request->rq_timeout = PSCRPC_OBD_TIMEOUT / 2;
	else
		request->rq_timeout = PSCRPC_OBD_TIMEOUT;

	request->rq_send_state = PSCRPC_IMP_NOOP;
	request->rq_type = PSCRPC_MSG_REQUEST;
	request->rq_import = pscrpc_import_get(imp);
	request->rq_export = NULL;

	request->rq_req_cbid.cbid_fn  = pscrpc_request_out_callback;
	request->rq_req_cbid.cbid_arg = request;

	request->rq_reply_cbid.cbid_fn  = pscrpc_reply_in_callback;
	request->rq_reply_cbid.cbid_arg = request;

	request->rq_phase = PSCRPC_RQ_PHASE_NEW;

	/* XXX FIXME bug 249 */
	request->rq_request_portal = imp->imp_client->cli_request_portal;
	request->rq_reply_portal = imp->imp_client->cli_reply_portal;

	INIT_SPINLOCK(&request->rq_lock);
	INIT_PSC_LISTENTRY(&request->rq_lentry);
	INIT_PSC_LISTENTRY(&request->rq_history_lentry);
	//INIT_PSCLIST_HEAD(&request->rq_replay_list);
	INIT_PSC_LISTENTRY(&request->rq_set_chain_lentry);
	psc_waitq_init(&request->rq_reply_waitq);
	request->rq_xid = pscrpc_next_xid();
	atomic_set(&request->rq_refcount, 1);

	request->rq_reqmsg->opc = opcode;
	request->rq_reqmsg->flags = 0;

	return (request);
}

void
pscrpc_req_setcompl(struct pscrpc_request *rq, struct psc_compl *pc)
{
	rq->rq_compl = pc;
	DEBUG_REQ(PLL_DIAG, rq, "rq_compl %p", pc);
}

struct pscrpc_request *
pscrpc_prep_req(struct pscrpc_import *imp, uint32_t version, int opcode,
    int count, int *lengths, char **bufs)
{
	return (pscrpc_prep_req_pool(imp, version, opcode, count,
	    lengths, bufs, NULL));
}

static __inline struct pscrpc_bulk_desc *
pscrpc_new_bulk(int npages, int type, int portal)
{
	struct pscrpc_bulk_desc *desc;

	PSCRPC_OBD_ALLOC(desc, offsetof(struct pscrpc_bulk_desc,
	    bd_iov[npages]));
	if (!desc)
		return NULL;

	INIT_SPINLOCK(&desc->bd_lock);
	psc_waitq_init(&desc->bd_waitq);
	desc->bd_max_iov = npages;
	desc->bd_iov_count = 0;
	desc->bd_md_h = LNET_INVALID_HANDLE;
	desc->bd_portal = portal;
	desc->bd_type = type;

	return desc;
}

struct pscrpc_bulk_desc *
pscrpc_prep_bulk_imp(struct pscrpc_request *req, int npages, int type,
    int portal)
{
	struct pscrpc_import    *imp = req->rq_import;
	struct pscrpc_bulk_desc *desc;

	psc_assert(type == BULK_PUT_SINK || type == BULK_GET_SOURCE);
	desc = pscrpc_new_bulk(npages, type, portal);
	if (desc == NULL)
		return (NULL);

	//desc->bd_import_generation = req->rq_import_generation;
	desc->bd_connection = imp->imp_connection;
	desc->bd_import = pscrpc_import_get(imp);
	desc->bd_req    = req;

	desc->bd_cbid.cbid_fn  = pscrpc_client_bulk_callback;
	desc->bd_cbid.cbid_arg = desc;

	/* This makes req own desc, and free it when she frees herself */
	psc_assert(req->rq_bulk == NULL);
	req->rq_bulk = desc;

	return desc;
}

struct pscrpc_bulk_desc *
pscrpc_prep_bulk_exp(struct pscrpc_request *req, int npages, int type,
    int portal)
{
	struct pscrpc_export   *exp = req->rq_export;
	struct pscrpc_bulk_desc *desc;

	psc_assert(type == BULK_PUT_SOURCE || type == BULK_GET_SINK);

	desc = pscrpc_new_bulk(npages, type, portal);
	if (desc == NULL)
		return (NULL);

	desc->bd_connection = req->rq_conn;
	desc->bd_export = pscrpc_export_get(exp);
	desc->bd_req = req;

	desc->bd_cbid.cbid_fn  = pscrpc_server_bulk_callback;
	desc->bd_cbid.cbid_arg = desc;

	/* NB we don't assign rq_bulk here; server-side requests are
	 * Re-Used, and the handler frees the bulk desc explicitly. */
	return desc;
}

void
pscrpc_set_init(struct pscrpc_request_set *set)
{
	INIT_PSCLIST_HEAD(&set->set_requests);
	INIT_PSC_LISTENTRY(&set->set_lentry);
	psc_waitq_init(&set->set_waitq);
	set->set_remaining = 0;
	INIT_SPINLOCK(&set->set_lock);
}

struct pscrpc_request_set *
pscrpc_prep_set(void)
{
	struct pscrpc_request_set *set;

	PSCRPC_OBD_ALLOC(set, sizeof *set);
	if (!set)
		return (NULL);
	pscrpc_set_init(set);
	return (set);
}

/* XXX rename this terribly named function */
void
pscrpc_set_lock(struct pscrpc_request_set *set)
{
	spinlock(&set->set_lock);
	while (set->set_flags & PSCRPC_SETF_CHECKING) {
		psc_waitq_wait(&set->set_waitq, &set->set_lock);
		spinlock(&set->set_lock);
	}
}

/*
 * lock so many callers can add things, the context that owns the set
 * is supposed to notice these and move them into the set proper.
 */
void
pscrpc_set_add_new_req(struct pscrpc_request_set *set,
		       struct pscrpc_request    *req)
{
	pscrpc_set_lock(set);
	/* The set takes over the caller's request reference */
	psclist_add_tail(&req->rq_set_chain_lentry, &set->set_requests);
	req->rq_set = set;
	set->set_remaining++;
	atomic_inc(&req->rq_import->imp_inflight);
	freelock(&set->set_lock);
}

void
pscrpc_set_remove_req(struct pscrpc_request_set *set,
    struct pscrpc_request *req)
{
	pscrpc_set_lock(set);
	psclist_del(&req->rq_set_chain_lentry, &set->set_requests);
	req->rq_set = NULL;
	psc_assert(set->set_remaining > 0);
	set->set_remaining--;
	psc_assert(atomic_read(&req->rq_import->imp_inflight) > 0);
	atomic_dec(&req->rq_import->imp_inflight);
	psc_waitq_wakeall(&set->set_waitq);
	freelock(&set->set_lock);
}

/**
 * expired_request - timeout handler used when sending a msg
 * @data: the request
 * Notes: Modified to allow requests to be retried without causing a
 * failure of the entire import.  When the request fails
 * 'imp_max_retries' times, pscrpc_expire_one_request() is called and
 * the import is failed.
 */
static int
expired_request(void *data)
{
	struct pscrpc_request *req = data;
	struct pscrpc_import *imp = req->rq_import;

	atomic_inc(&req->rq_retries);

	DEBUG_REQ(PLL_INFO, req, "request timeout");

	if (atomic_read(&req->rq_retries) >= imp->imp_max_retries)
		return (pscrpc_expire_one_request(req));

	spinlock(&req->rq_lock);
	req->rq_resend = 1;
	freelock(&req->rq_lock);

	return 0;
}

static void
interrupted_request(void *data)
{
	struct pscrpc_request *req = data;
	DEBUG_REQ(PLL_INFO, req, "request interrupted");
	spinlock(&req->rq_lock);
	req->rq_intr = 1;
	freelock(&req->rq_lock);
}

static int
pscrpc_send_new_req_locked(struct pscrpc_request *req)
{
	struct pscrpc_import     *imp;
	int rc;

	LOCK_ENSURE(&req->rq_lock);
	DEBUG_REQ(PLL_INFO, req, "about to send rpc");

	psc_assert(req->rq_phase == PSCRPC_RQ_PHASE_NEW);
	req->rq_phase = PSCRPC_RQ_PHASE_RPC;
	freelock(&req->rq_lock);

	imp = req->rq_import;
	spinlock(&imp->imp_lock);
	req->rq_import_generation = imp->imp_generation;
	psclist_add_tail(&req->rq_lentry, &imp->imp_sending_list);
	freelock(&imp->imp_lock);

	rc = pscrpc_send_rpc(req, 0);
	if (rc) {
		DEBUG_REQ(PLL_WARN, req,
			  "send failed (%d); expect timeout", rc);
		req->rq_net_err = 1;
		spinlock(&imp->imp_lock);
		psclist_del(&req->rq_lentry, &imp->imp_sending_list);
		freelock(&imp->imp_lock);

		return (rc);
	}
	return (0);
}

int
pscrpc_push_req(struct pscrpc_request *req)
{
	spinlock(&req->rq_lock);
	if (req->rq_phase == PSCRPC_RQ_PHASE_NEW)
		/* pscrpc_send_new_req_locked() frees the lock.
		 */
		return (pscrpc_send_new_req_locked(req));
	else {
		/* This is ok, it means that another thread has done
		 *   a pscrpc_check_set() which also pushes req's
		 *   which are PSCRPC_RQ_PHASE_NEW.
		 */
		freelock(&req->rq_lock);
		DEBUG_REQ(PLL_INFO, req, "req already inflight");
		return (0);
	}
}

static int
pscrpc_check_reply(struct pscrpc_request *req)
{
	int rc = 0;

	/* serialise with network callback */
	spinlock(&req->rq_lock);

	if (req->rq_replied) {
		DEBUG_REQ(PLL_INFO, req, "REPLIED:");
		GOTO(out, rc = 1);
	}

	if (req->rq_net_err && !req->rq_timedout) {
		DEBUG_REQ(PLL_ERROR, req, "NET_ERR: %d", req->rq_net_err);
		freelock(&req->rq_lock);
		rc = pscrpc_expire_one_request(req);
		spinlock(&req->rq_lock);
		GOTO(out, rc);
	}

	if (req->rq_err) {
		DEBUG_REQ(PLL_WARN, req, "ABORTED:");
		GOTO(out, rc = 1);
	}

	if (req->rq_resend) {
		DEBUG_REQ(PLL_WARN, req, "RESEND:");
		GOTO(out, rc = 1);
	}

	if (req->rq_restart) {
		DEBUG_REQ(PLL_WARN, req, "RESTART:");
		GOTO(out, rc = 1);
	}
 out:
	freelock(&req->rq_lock);
	DEBUG_REQ(rc ? PLL_INFO : PLL_DEBUG, req, "rc=%d", rc);
	return rc;
}

void
pscrpc_unregister_reply(struct pscrpc_request *request)
{
	struct l_wait_info lwi;
	struct psc_waitq *wq;
	int rc;

	psc_assert(!in_interrupt());             /* might sleep */

	DEBUG_REQ(PLL_INFO, request, "receving_reply=%d",
		  pscrpc_client_receiving_reply(request));

	if (!pscrpc_client_receiving_reply(request))
		return;

	LNetMDUnlink(request->rq_reply_md_h);

	/* We have to l_wait_event() whatever the result, to give liblustre
	 * a chance to run reply_in_callback() */
	if (request->rq_set)
		wq = &request->rq_set->set_waitq;
	else
		wq = &request->rq_reply_waitq;

	for (;;) {
		/* Network access will complete in finite time but the HUGE
		 * timeout lets us CWARN for visibility of sluggish NALs */
		lwi = LWI_TIMEOUT(10, NULL, NULL);
		rc = pscrpc_cli_wait_event(wq,
		    !pscrpc_client_receiving_reply(request), &lwi);
		if (rc == 0)
			return;

		psc_assert(rc == -ETIMEDOUT);
		DEBUG_REQ(PLL_ERROR, request, "Unexpectedly long timeout");
		//abort();
	}
}

static int
pscrpc_check_status(struct pscrpc_request *req)
{
	int err;

	err = req->rq_repmsg->status;
	if (req->rq_repmsg->type == PSCRPC_MSG_ERR) {
		DEBUG_REQ(PLL_ERROR, req, "type == PSCRPC_MSG_ERR, err == %d",
			  err);
		return (err < 0 ? err : -EINVAL);
	}

	if (err < 0) {
		DEBUG_REQ(PLL_INFO, req, "status is %d", err);

	} else if (err > 0) {
		/* XXX: translate this error from net to host */
		DEBUG_REQ(PLL_INFO, req, "status is %d", err);
	}

	return (err);
}

static int
after_reply(struct pscrpc_request *req)
{
	//struct pscrpc_import *imp = req->rq_import;
	int rc;

	psc_assert(!req->rq_receiving_reply);

	/* NB Until this point, the whole of the incoming message,
	 * including buflens, status etc is in the sender's byte order. */

#if SWAB_PARANOIA
	/* Clear reply swab mask; this is a new reply in sender's byte order */
	req->rq_rep_swab_mask = 0;
#endif
	psc_assert(req->rq_nob_received <= req->rq_replen);
	rc = pscrpc_unpack_msg(req->rq_repmsg, req->rq_nob_received);
	if (rc) {
		DEBUG_REQ(PLL_ERROR, req, "unpack_rep failed: %d", rc);
		return (-EPROTO);
	}

	if (req->rq_repmsg->type != PSCRPC_MSG_REPLY &&
	    req->rq_repmsg->type != PSCRPC_MSG_ERR) {
		DEBUG_REQ(PLL_ERROR, req, "invalid packet received (type=%u)",
			  req->rq_repmsg->type);
		return (-EPROTO);
	}

	rc = pscrpc_check_status(req);

	/*
	 * Either we've been evicted, or the server has failed for
	 * some reason. Try to reconnect, and if that fails, punt to the
	 * upcall.
	 */
	if ((rc == -ENOTCONN) || (rc == -ENODEV)) {
		psclog_errorx("ENOTCONN or ENODEV");
		return (rc);
	}

	/* Store transno in reqmsg for replay. */
	req->rq_reqmsg->transno = req->rq_transno = req->rq_repmsg->transno;

	return (rc);
}

int
pscrpc_queue_wait(struct pscrpc_request *req)
{
	int rc = 0, brc, timeout = 0;
	struct pscrpc_import *imp = req->rq_import;
	struct l_wait_info lwi;

	psc_assert(req->rq_set == NULL);
	psc_assert(!req->rq_receiving_reply);
	atomic_inc(&imp->imp_inflight);

	psc_assert(imp);
	DEBUG_REQ(PLL_INFO, req, "sending..");

	/* Mark phase here for a little debug help */
	req->rq_phase = PSCRPC_RQ_PHASE_RPC;

 restart:
	if (req->rq_resend) {
		DEBUG_REQ(PLL_WARN, req, "resending (receiving_reply=%d)",
			  req->rq_receiving_reply);
		psc_assert(!req->rq_no_resend);

		if (req->rq_compl)
			/* Make sure reply_in_callback doesn't bump rq_compl.
			 */
			req->rq_abort_reply = 1;

		pscrpc_unregister_reply(req);
		pscrpc_msg_add_flags(req->rq_reqmsg, MSG_RESENT);

		if (req->rq_bulk) {
			pscrpc_unregister_bulk(req);

			/* bulk requests are supposed to be
			 * idempotent, so we are free to bump the xid
			 * here, which we need to do before
			 * registering the bulk again (bug 6371).
			 * print the old xid first for sanity.
			 */
			DEBUG_REQ(PLL_INFO, req, "bumping xid for bulk: ");
			req->rq_xid = pscrpc_next_xid();
		}
	}

	spinlock(&imp->imp_lock);
	req->rq_import_generation = imp->imp_generation;
	psclist_add_tail(&req->rq_lentry, &imp->imp_sending_list);
	freelock(&imp->imp_lock);

	psclog_info("request %p request->rq_reqmsg %p",
	    req, req->rq_reqmsg);

	rc = pscrpc_send_rpc(req, 0);
	if (rc) {
		DEBUG_REQ(PLL_INFO, req, "send failed (%d); recovering", rc);
		timeout = 1;
	} else {
		//timeout = MAX(req->rq_timeout * 100, 1);
		timeout = MAX(req->rq_timeout, 1);
		DEBUG_REQ(PLL_INFO, req,"sleeping for %d sec", timeout);
	}
	lwi = LWI_TIMEOUT_INTR(timeout, expired_request,
	    interrupted_request, req);

	rc = pscrpc_cli_wait_event(&req->rq_reply_waitq,
	    pscrpc_check_reply(req), &lwi);

	DEBUG_REQ(rc ? PLL_ERROR : PLL_INFO, req,
		  "completed (rc=%d), replied=%d", rc, req->rq_replied);

	spinlock(&imp->imp_lock);
	psclist_del(&req->rq_lentry, psc_lentry_hd(&req->rq_lentry));
	freelock(&imp->imp_lock);

	if (rc)
		GOTO(out, 0);

	if (req->rq_err)
		GOTO(out, rc = -EIO);

	/* Resend if we need to, unless we were interrupted. */
	if (req->rq_resend && !req->rq_intr) {
		/* ...unless we were specifically told otherwise. */
		if (req->rq_no_resend)
			GOTO(out, rc = -ETIMEDOUT);
		goto restart;
	}

	if (req->rq_intr) {
		/* Should only be interrupted if we timed out. */
		if (!req->rq_timedout)
			DEBUG_REQ(PLL_ERROR, req,
				  "rq_intr set but rq_timedout not");
		GOTO(out, rc = -EINTR);
	}

	if (req->rq_timedout) {
		/* non-recoverable timeout */
		GOTO(out, rc = -ETIMEDOUT);
	}

	if (!req->rq_replied) {
		/* How can this be? -eeb */
		DEBUG_REQ(PLL_ERROR, req, "!rq_replied: ");
#if 1 /* We have hit this point many times upon server failure */
		LBUG();
		GOTO(out, rc = req->rq_status);
#else /* so... just make it an error condition */
		GOTO(out, rc = -EIO);
#endif
	}

	rc = after_reply(req);
 out:
	/* If the reply was received normally, this just grabs the spinlock
	 * (ensuring the reply callback has returned), sees that
	 * req->rq_receiving_reply is clear and returns. */
	pscrpc_unregister_reply(req);

	if (req->rq_bulk) {
		struct pscrpc_msg *m;

		m = req->rq_repmsg;
		if (rc >= 0) {
			if (m && (m->flags & MSG_ABORT_BULK) &&
			    req->rq_bulk_abortable)
				goto skipbulk;

			/* success so far.  Note that anything going wrong
			 * with bulk now, is EXTREMELY strange, since the
			 * server must have believed that the bulk
			 * tranferred OK before she replied with success to
			 * me. */
			lwi = LWI_TIMEOUT(timeout, NULL, NULL);
			brc = pscrpc_cli_wait_event(&req->rq_reply_waitq,
			    !pscrpc_bulk_active(req->rq_bulk),
			    &lwi);
			psc_assert(brc == 0 || brc == -ETIMEDOUT);
			if (brc) {
				psc_assert(brc == -ETIMEDOUT);
				DEBUG_REQ(PLL_ERROR, req, "bulk timed out");
				rc = brc;
			} else if (!req->rq_bulk->bd_success) {
				DEBUG_REQ(PLL_ERROR, req,
					  "bulk transfer failed");
				rc = -EIO;
			}
		}
 skipbulk:
		//if (rc < 0)
			pscrpc_unregister_bulk(req);
	}

	psc_assert(!req->rq_receiving_reply);
	req->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;

	atomic_dec(&imp->imp_inflight);
	return (rc);
}

/**
 * pscrpc_check_set - Send unsent RPCs in @set.  If check_allsent,
 *     returns TRUE if all are sent otherwise return true if at least one
 *     has completed.
 * @set:  the set in question
 * @check_allsent: return true only if all requests are finished; set to '1'
 *                 for original Lustre behavior.
 * NOTES: check_allsent was added to support single, non-blocking sends
 *        implemented within the context of an rpcset.
 */
int
pscrpc_check_set(struct pscrpc_request_set *set, int check_allsent)
{
	struct pscrpc_request *req;
	int ncompleted         = 0;

	pscrpc_set_lock(set);
	psc_assert((set->set_flags & PSCRPC_SETF_CHECKING) == 0);
	set->set_flags |= PSCRPC_SETF_CHECKING;
	freelock(&set->set_lock);

	if (set->set_remaining == 0) {
		spinlock(&set->set_lock);
		psc_assert(set->set_flags & PSCRPC_SETF_CHECKING);
		set->set_flags &= ~PSCRPC_SETF_CHECKING;
		psc_waitq_wakeall(&set->set_waitq);
		freelock(&set->set_lock);
		return (1);
	}

	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry) {
		struct pscrpc_import *imp = req->rq_import;
		int rc = 0;

		DEBUG_REQ(PLL_DEBUG, req, "reqset=%p", set);

		if (req->rq_phase == PSCRPC_RQ_PHASE_NEW &&
		    pscrpc_push_req(req)) {
			DEBUG_REQ(PLL_WARN, req, "PSCRPC_RQ_PHASE_NEW");
		}

		if (!(req->rq_phase == PSCRPC_RQ_PHASE_RPC ||
		      req->rq_phase == PSCRPC_RQ_PHASE_BULK ||
		      req->rq_phase == PSCRPC_RQ_PHASE_INTERPRET ||
		      req->rq_phase == PSCRPC_RQ_PHASE_COMPLETE)) {
			DEBUG_REQ(PLL_ERROR, req, "bad phase %x",
				  req->rq_phase);
			LBUG();
		}

		if (req->rq_phase == PSCRPC_RQ_PHASE_COMPLETE) {
			ncompleted++;
			continue;
		}

		if (req->rq_phase == PSCRPC_RQ_PHASE_INTERPRET)
			GOTO(interpret, req->rq_status);

		if (req->rq_net_err && !req->rq_timedout) {
			pscrpc_expire_one_request(req);
			if (!req->rq_err)
				req->rq_err = 1;
		}

 handle_error:
		if (req->rq_err) {
			/* Catch errored out RPC's here.
			 */
			pscrpc_unregister_reply(req);
			if (req->rq_bulk)
				pscrpc_abort_bulk(req->rq_bulk);

			if (req->rq_status == 0)
				req->rq_status = -EIO;
			req->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;

			spinlock(&imp->imp_lock);
			psclist_del(&req->rq_lentry,
			    psc_lentry_hd(&req->rq_lentry));
			freelock(&imp->imp_lock);

			GOTO(interpret, req->rq_status);
		}

		/* pscrpc_queue_wait->l_wait_event guarantees that rq_intr
		 * will only be set after rq_timedout, but the oig waiting
		 * path sets rq_intr irrespective of whether pscrpcd has
		 * seen a timeout.  our policy is to only interpret
		 * interrupted rpcs after they have timed out */
		if (req->rq_intr && req->rq_timedout) {
			/* NB could be on delayed list */
			pscrpc_unregister_reply(req);
			req->rq_status = -EINTR;
			req->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;

			spinlock(&imp->imp_lock);
			psclist_del(&req->rq_lentry,
			    psc_lentry_hd(&req->rq_lentry));
			freelock(&imp->imp_lock);

			GOTO(interpret, req->rq_status);
		}

		if (req->rq_phase == PSCRPC_RQ_PHASE_RPC) {
			int status = 0;
			/* If using a non-blocking set, then check for expired
			 *   requests here.
			 */
			if (!check_allsent &&
			    !pscrpc_client_replied(req) &&
			    pscrpc_is_expired(req)) {
				/* rc == 0 means we're resending the request!
				 *   otherwise, this request is getting nuked.
				 */
				status = expired_request(req);
				DEBUG_REQ(PLL_WARN, req, "expired (resend=%d)",
					  !status);

				if (status) {
					psc_assert(req->rq_status);
					psc_assert(req->rq_err);
					psc_assert(req->rq_timedout);
					goto handle_error;
				}
			}
			if (req->rq_resend) {
				if (req->rq_no_resend) {
					if (!req->rq_err)
						req->rq_err = 1;
					req->rq_status = -ENOTCONN;
					goto handle_error;
				}

				if (req->rq_compl)
					/* Make sure reply_in_callback doesn't
					 *   bump rq_compl.
					 */
					req->rq_abort_reply = 1;

				pscrpc_unregister_reply(req);
				/* Move to the tail of the list.
				 */
				spinlock(&imp->imp_lock);
				psclist_del(&req->rq_lentry,
					    psc_lentry_hd(&req->rq_lentry));
				psclist_add_tail(&req->rq_lentry,
						 &imp->imp_sending_list);
				freelock(&imp->imp_lock);

				pscrpc_msg_add_flags(req->rq_reqmsg,
				     MSG_RESENT);
				if (req->rq_bulk) {
					uint64_t old_xid = req->rq_xid;

					pscrpc_unregister_bulk(req);

					/* ensure previous bulk fails */
					req->rq_xid = pscrpc_next_xid();
					DEBUG_REQ(PLL_NOTICE, req,
						  "resend bulk: old x%"PRIu64
						  " new x%"PRIu64,
						  old_xid, req->rq_xid);
				}

				rc = pscrpc_send_rpc(req, 0);
				if (rc) {
					DEBUG_REQ(PLL_ERROR, req,
						  "send failed (%d)", rc);
					req->rq_net_err = 1;
				}
			}

			if (pscrpc_client_receiving_reply(req) ||
			    !pscrpc_client_replied(req))
				/* Req is not finished.
				 */
				continue;

			spinlock(&imp->imp_lock);
			psclist_del(&req->rq_lentry,
				    psc_lentry_hd(&req->rq_lentry));
			freelock(&imp->imp_lock);

			req->rq_status = after_reply(req);

			/* If there is no bulk associated with this request,
			 * then we're done and should let the interpreter
			 * process the reply.  Similarly if the RPC returned
			 * an error, and therefore the bulk will never arrive
			 */
			if (req->rq_bulk == NULL || req->rq_status) {
				req->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;
				GOTO(interpret, req->rq_status);
			}

			req->rq_phase = PSCRPC_RQ_PHASE_BULK;
		}

		if (req->rq_repmsg &&
		    (req->rq_repmsg->flags & MSG_ABORT_BULK) &&
		    req->rq_bulk_abortable) {
			req->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;
			GOTO(interpret, req->rq_status);
		}

		psc_assert(req->rq_phase == PSCRPC_RQ_PHASE_BULK);
		if (pscrpc_bulk_active(req->rq_bulk))
			continue;

		if (!req->rq_bulk->bd_success) {
			/* The RPC reply arrived OK, but the bulk screwed
			 * up!  Dead weird since the server told us the RPC
			 * was good after getting the REPLY for her GET or
			 * the ACK for her PUT. */
			DEBUG_REQ(PLL_ERROR, req, "bulk transfer failed");
			LBUG();
		}

		req->rq_phase = PSCRPC_RQ_PHASE_INTERPRET;

 interpret:
		psc_assert(req->rq_phase == PSCRPC_RQ_PHASE_INTERPRET);
		psc_assert(!req->rq_receiving_reply);

		pscrpc_unregister_reply(req);
		if (req->rq_bulk)
			pscrpc_unregister_bulk(req);

		req->rq_phase = PSCRPC_RQ_PHASE_COMPLETE;
		ncompleted++;

		pscrpc_interpret(req);

		set->set_remaining--;

		DEBUG_REQ(PLL_DEBUG, req, "set(%p) rem=(%d) ",
			  set, set->set_remaining);

		atomic_dec(&imp->imp_inflight);
		psc_waitq_wakeall(&imp->imp_recovery_waitq);
	}

	spinlock(&set->set_lock);
	psc_assert(set->set_flags & PSCRPC_SETF_CHECKING);
	set->set_flags &= ~PSCRPC_SETF_CHECKING;
	psc_waitq_wakeall(&set->set_waitq);
	freelock(&set->set_lock);

	/* If we hit an error, we want to recover promptly. */
	if (check_allsent)
		/* old behavior */
		return (set->set_remaining == 0);
	/* new (single non-blocking req) behavior */
	return (ncompleted);
}

void
pscrpc_set_destroy(struct pscrpc_request_set *set)
{
	struct pscrpc_request *req, *next;
	unsigned expected_phase;
	int               n = 0;

	/* Requests on the set should either all be completed, or all be new */
	expected_phase = (set->set_remaining == 0) ?
		PSCRPC_RQ_PHASE_COMPLETE : PSCRPC_RQ_PHASE_NEW;
	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry) {
		psc_assert(req->rq_phase == expected_phase);
		n++;
	}

	psc_assert(set->set_remaining == 0 || set->set_remaining == n);

	psclist_for_each_entry_safe(req, next, &set->set_requests,
	    rq_set_chain_lentry) {
		psclist_del(&req->rq_set_chain_lentry,
		    &set->set_requests);

		psc_assert(req->rq_phase == expected_phase);

		if (req->rq_phase == PSCRPC_RQ_PHASE_NEW) {
			req->rq_status = -EBADR;
			/* higher level (i.e. LOV) failed;
			 * let the sub reqs clean up */
			pscrpc_interpret(req);
			set->set_remaining--;
		}

		req->rq_set = NULL;
		pscrpc_req_finished(req);
	}

	psc_assert(set->set_remaining == 0 &&
		   psc_waitq_nwaiters(&set->set_waitq) == 0);

	psc_assert(!psc_waitq_nwaiters(&set->set_waitq));
	psc_waitq_destroy(&set->set_waitq);

	PSCRPC_OBD_FREE(set, sizeof(*set));
}

int
pscrpc_expire_one_request(struct pscrpc_request *req)
{
	struct pscrpc_import *imp = req->rq_import;

	DEBUG_REQ(imp->imp_igntimeout ? PLL_WARN : PLL_ERROR, req,
	    "timeout (sent at %"PSCPRI_TIMET", %"PSCPRI_TIMET"s ago)",
	    req->rq_sent, CURRENT_SECONDS - req->rq_sent);

	psc_assert(req->rq_send_state == PSCRPC_IMP_NOOP);

	spinlock(&req->rq_lock);
	/* Error out the request here so that the upper layers may
	 *    retry.
	 */
	req->rq_err = req->rq_timedout = 1;
	req->rq_status = -ETIMEDOUT;
	freelock(&req->rq_lock);

	pscrpc_unregister_reply(req);

	if (req->rq_bulk)
		pscrpc_unregister_bulk(req);

	if (imp == NULL)
		DEBUG_REQ(PLL_WARN, req, "NULL import: already cleaned up?");

	else if (!imp->imp_igntimeout)
		pscrpc_fail_import(imp, req->rq_reqmsg->conn_cnt);


	return (1);
}

int
pscrpc_expired_set(void *data)
{
	struct pscrpc_request_set *set = data;
	struct pscrpc_request *req;
	time_t now = CURRENT_SECONDS;

	psc_assert(set);

	/* A timeout expired; see which reqs it applies to... */
	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry) {
		/* request in-flight? */
		if (!((req->rq_phase == PSCRPC_RQ_PHASE_RPC && !req->rq_waiting &&
		       !req->rq_resend) ||
		      (req->rq_phase == PSCRPC_RQ_PHASE_BULK)))
			continue;

		if (req->rq_timedout ||           /* already dealt with */
		    req->rq_sent + req->rq_timeout > now) /* not expired */
			continue;

		/* deal with this guy */
		pscrpc_expire_one_request(req);
	}

	/* When waiting for a whole set, we always to break out of the
	 * sleep so we can recalculate the timeout, or enable interrupts
	 * iff everyone's timed out.
	 */
	return (1);
}

int
pscrpc_set_next_timeout(struct pscrpc_request_set *set)
{
	time_t now = CURRENT_SECONDS, deadline;
	struct pscrpc_request *req;
	int timeout = 0;

	//SIGNAL_MASK_ASSERT(); /* XXX BUG 1511 */

	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry) {
		/* request in-flight? */
		if (!((req->rq_phase == PSCRPC_RQ_PHASE_RPC && !req->rq_waiting) ||
		      (req->rq_phase == PSCRPC_RQ_PHASE_BULK)))
			continue;

		if (req->rq_timedout)   /* already timed out */
			continue;

		deadline = req->rq_sent + req->rq_timeout;
		if (deadline <= now)    /* actually expired already */
			timeout = 1;    /* ASAP */
		else if (timeout == 0 || timeout > deadline - now)
			timeout = deadline - now;
	}
	return (timeout);
}

void
pscrpc_mark_interrupted(struct pscrpc_request *req)
{
	spinlock(&req->rq_lock);
	req->rq_intr = 1;
	freelock(&req->rq_lock);
}

void
pscrpc_interrupted_set(void *data)
{
	struct pscrpc_request_set *set = data;
	struct pscrpc_request *req;

	psc_assert(set);
	CERROR("INTERRUPTED SET %p\n", set);

	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry) {
		if (req->rq_phase != PSCRPC_RQ_PHASE_RPC)
			continue;

		pscrpc_mark_interrupted(req);
	}
}

#ifdef PSCRPC_IMPORT_DELAY
static int
pscrpc_import_delay_req(struct pscrpc_import *imp,
    struct pscrpc_request *req, int *status)
{
	int delay = 0;

	psc_assert(status);
	*status = 0;

	if (imp->imp_state == PSCRPC_IMP_NEW) {
		DEBUG_REQ(PLL_ERROR, req, "Uninitialized import.");
		*status = -EIO;
		LBUG();
	} else if (imp->imp_state == PSCRPC_IMP_CLOSED) {
		DEBUG_REQ(PLL_ERROR, req, "IMP_CLOSED ");
		*status = -EIO;
	} else if (req->rq_send_state == PSCRPC_IMP_CONNECTING &&
		   imp->imp_state == PSCRPC_IMP_CONNECTING) {
		/* allow CONNECT even if import is invalid */ ;
	} else if (imp->imp_invalid) {
		/* If the import has been invalidated (such as by an OST
		 * failure), the request must fail with -EIO. */
		if (!imp->imp_deactive)
			DEBUG_REQ(PLL_ERROR, req, "IMP_INVALID");
		*status = -EIO;
	} else if (req->rq_import_generation != imp->imp_generation) {
		DEBUG_REQ(PLL_ERROR, req, "req wrong generation:");
		*status = -EIO;
	} else if (req->rq_send_state != imp->imp_state) {
		//if (imp->imp_obd->obd_no_recov || imp->imp_dlm_fake ||
		if (req->rq_no_delay)
			*status = -EWOULDBLOCK;
		else
			delay = 1;
	}

	return (delay);
}
#endif

int
pscrpc_set_wait(struct pscrpc_request_set *set)
{
	struct pscrpc_request *req;
	struct l_wait_info     lwi;
	int                    rc=0, timeout;

	if (psc_listhd_empty(&set->set_requests))
		return (0);

	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry)
		if (req->rq_phase == PSCRPC_RQ_PHASE_NEW)
			pscrpc_push_req(req);

	do {
		timeout = pscrpc_set_next_timeout(set);
		/* wait until all complete, interrupted, or an in-flight
		 * req times out
		 */
		CDEBUG(D_NET, "set %p going to sleep for %d seconds\n",
		       set, timeout);
		lwi = LWI_TIMEOUT_INTR(timeout ? timeout : 1,
		       pscrpc_expired_set, pscrpc_interrupted_set, set);

		rc = pscrpc_cli_wait_event(&set->set_waitq,
		    pscrpc_check_set(set, 1), &lwi);

		psc_assert(rc == 0 || rc == -EINTR || rc == -ETIMEDOUT);

		/*
		 * -EINTR => all requests have been flagged rq_intr so next
		 * check completes.
		 * -ETIMEDOUT => someone timed out.  When all reqs have
		 * timed out, signals are enabled allowing completion with
		 * EINTR.
		 * I don't really care if we go once more round the loop in
		 * the error cases -eeb.
		 */
	} while (rc || set->set_remaining);

	psc_assert(set->set_remaining == 0);

	rc = 0;
	psclist_for_each_entry(req, &set->set_requests, rq_set_chain_lentry) {
		DEBUG_REQ(PLL_DEBUG, req, "set wait loop");

		if (req->rq_import->imp_failed) {
			psclog_errorx("failed import detected!");
			rc = -ECONNABORTED;
			continue;
		}

		psc_assert(req->rq_phase == PSCRPC_RQ_PHASE_COMPLETE);

		if (req->rq_status) {
			rc = -(abs(req->rq_status));
			DEBUG_REQ(PLL_ERROR, req, "error rq_status=%d set=%p",
				  rc, set);
		}
	}

	if (rc)
		psclog_errorx("set %p failed, rc=%d", set, rc);

	if (set->set_interpret) {
		rc = set->set_interpret(set, set->set_arg, rc);
		if (rc)
			psclog_errorx("set interpreter failed rc=%d", rc);
	}

	return (rc);
}

/**
 * pscrpc_set_finalize - call set_wait or check_set depending on blocking
 *	mode.  If the set has completed then free it.
 * @set: the set.
 * @block: call set_wait right away or otherwise only if check_set reports
 *	the set as being done.
 * @destroy: call destroy if the set has completed.
 * Notes: return 0 when the set has been completed, otherwise return 1
 */
int
pscrpc_set_finalize(struct pscrpc_request_set *set, int block, int destroy)
{
	int rc;

	if (block) {
 set_wait:
		rc = pscrpc_set_wait(set);
		if (rc) {
			errno = -rc;
			psclog_errorx("pscrpc_set_wait() failed (set=%p): %s",
			    set, strerror(-rc));
		}
		if (destroy)
			pscrpc_set_destroy(set);
	} else {
		rc = pscrpc_check_set(set, 1);
		psclog_debug("pscrpc_check_set() returned %d set=%p", rc, set);
		if (rc == 1)
			goto set_wait;
		else
			rc = 1;
	}
	return (rc);
}

#if 0
void
pscrpc_free_committed(struct pscrpc_import *imp)
{
	struct psclist_head *tmp, *saved;
	struct pscrpc_request *req;
	struct pscrpc_request *last_req = NULL; /* temporary fire escape */

	psc_assert(imp);

	LOCK_ENSURE(&imp->imp_lock);

	if (imp->imp_peer_committed_transno == imp->imp_last_transno_checked &&
	    imp->imp_generation == imp->imp_last_generation_checked) {
		CDEBUG(D_HA, "%s: skip recheck for last_committed %"PRIu64"\n",
		       imp->imp_obd->obd_name, imp->imp_peer_committed_transno);
		return;
	}

	CDEBUG(D_HA, "%s: committing for last_committed %"PRIu64" gen %d\n",
	       imp->imp_obd->obd_name, imp->imp_peer_committed_transno,
	       imp->imp_generation);
	imp->imp_last_transno_checked = imp->imp_peer_committed_transno;
	imp->imp_last_generation_checked = imp->imp_generation;

	psclist_for_each_safe(tmp, saved, &imp->imp_replay_list) {
		req = psc_lentry_obj(tmp, struct pscrpc_request, rq_replay_list);

		/* XXX ok to remove when 1357 resolved - rread 05/29/03  */
		psc_assert(req != last_req);
		last_req = req;

		if (req->rq_import_generation < imp->imp_generation) {
			DEBUG_REQ(D_HA, req, "freeing request with old gen");
			GOTO(free_req, 0);
		}

		if (req->rq_replay) {
			DEBUG_REQ(D_HA, req, "keeping (FL_REPLAY)");
			continue;
		}

		/* not yet committed */
		if (req->rq_transno > imp->imp_peer_committed_transno) {
			DEBUG_REQ(D_HA, req, "stopping search");
			break;
		}

		DEBUG_REQ(D_HA, req, "committing (last_committed %"PRIu64")",
			  imp->imp_peer_committed_transno);
 free_req:
		if (req->rq_commit_cb)
			req->rq_commit_cb(req);
		psclist_del_init(&req->rq_replay_list);
		_pscrpc_req_finished(req, 1);
	}
}
#endif

void
pscrpc_abort_inflight(struct pscrpc_import *imp)
{
	struct pscrpc_request *req, *next;

	 /* Make sure that no new requests get processed for this import.
	  * pscrpc_{queue,set}_wait must (and does) hold imp_lock while testing
	  * this flag and then putting requests on sending_list or
	  * delayed_list.
	  */
	 spinlock(&imp->imp_lock);

	 /* XXX locking?  Maybe we should remove each request with the list
	  * locked?  Also, how do we know if the requests on the list are
	  * being freed at this time?
	  */
	 psclist_for_each_entry_safe(req, next, &imp->imp_sending_list,
	     rq_lentry) {
		 DEBUG_REQ(PLL_WARN, req, "aborted");

		 spinlock(&req->rq_lock);
		 if (req->rq_import_generation < imp->imp_generation) {
			 req->rq_err = 1;
			 pscrpc_wake_client_req(req);
		 }
		 //req->rq_abort_reply = 1;
		 if (req->rq_bulk)
			 req->rq_bulk->bd_abort = 1;

		 freelock(&req->rq_lock);
	 }

	 freelock(&imp->imp_lock);
}

void
pscrpc_resend_req(struct pscrpc_request *req)
{
	DEBUG_REQ(PLL_WARN, req, "going to resend");
	req->rq_reqmsg->handle.cookie = 0;
	req->rq_status = -EAGAIN;

	spinlock(&req->rq_lock);
	req->rq_resend = 1;
	req->rq_net_err = 0;
	req->rq_timedout = 0;
	if (req->rq_bulk) {
		uint64_t old_xid = req->rq_xid;

		/* ensure previous bulk fails */
		req->rq_xid = pscrpc_next_xid();
		psclog_warnx("resend bulk old x%"PRIu64" new x%"PRIu64,
		       old_xid, req->rq_xid);
	}
	pscrpc_wake_client_req(req);
	freelock(&req->rq_lock);
}
