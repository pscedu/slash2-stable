/* $Id: zestRpcClient.c 2143 2007-11-05 20:40:02Z yanovich $ */

#include "subsys.h"
#define ZSUBSYS ZS_RPC

#include "zestRpc.h"
#include "zestRpcLog.h"
#include "zestExport.h"
#include "zestAlloc.h"

static u64 zestrpc_last_xid = 0;
static spinlock_t zestrpc_last_xid_lock = SPIN_LOCK_UNLOCKED;

u64 zestrpc_next_xid(void)
{
        u64 tmp;
        spin_lock(&zestrpc_last_xid_lock);
        tmp = ++zestrpc_last_xid;
        spin_unlock(&zestrpc_last_xid_lock);
        return tmp;
}

u64 zestrpc_sample_next_xid(void)
{
        u64 tmp;
        spin_lock(&zestrpc_last_xid_lock);
        tmp = zestrpc_last_xid + 1;
        spin_unlock(&zestrpc_last_xid_lock);
        return tmp;
}

struct zestrpc_request *zestrpc_request_addref(struct zestrpc_request *req)
{
        ENTRY;
        atomic_inc(&req->rq_refcount);
        RETURN(req);
}

//struct zest_import *class_new_import(struct obd_device *obd)
struct zestrpc_import *new_import(void)
{
        struct zestrpc_import *imp;

        ZOBD_ALLOC(imp, sizeof(*imp));
        if (imp == NULL)
                return NULL;

        //INIT_ZLIST_HEAD(&imp->imp_replay_list);
        INIT_ZLIST_HEAD(&imp->imp_sending_list);
        //INIT_ZLIST_HEAD(&imp->imp_delayed_list);
        spin_lock_init(&imp->imp_lock);
        //imp->imp_last_success_conn = 0;
        imp->imp_state = ZEST_IMP_NEW;
        //imp->imp_obd = class_incref(obd);
        init_waitqueue_head(&imp->imp_recovery_waitq);

        atomic_set(&imp->imp_refcount, 2);
        atomic_set(&imp->imp_inflight, 0);
        //atomic_set(&imp->imp_replay_inflight, 0);
        INIT_ZLIST_HEAD(&imp->imp_conn_list);
        //INIT_ZLIST_HEAD(&imp->imp_handle.h_link);
        //class_handle_hash(&imp->imp_handle, import_handle_addref);

        return imp;
}

struct zestrpc_import *import_get(struct zestrpc_import *import)
{
        LASSERT(atomic_read(&import->imp_refcount) >= 0);
        LASSERT(atomic_read(&import->imp_refcount) < 0x5a5a5a);
        atomic_inc(&import->imp_refcount);
        zinfo("import %p refcount=%d", import,
	      atomic_read(&import->imp_refcount));
        return import;
}

void import_put(struct zestrpc_import *import)
{
        ENTRY;

        zinfo("import %p refcount=%d", import,
	      atomic_read(&import->imp_refcount) - 1);

        LASSERT(atomic_read(&import->imp_refcount) > 0);
        LASSERT(atomic_read(&import->imp_refcount) < 0x5a5a5a);
        if (!atomic_dec_and_test(&import->imp_refcount)) {
                EXIT;
                return;
        }
        zdbg("destroying import %p", import);

	zest_assert(import->imp_connection);
	zestrpc_put_connection(import->imp_connection);
 
	ZOBD_FREE(import, sizeof(*import));
        EXIT;
}


#if 0 //try to get away without uuid's
struct zestrpc_connection *zestrpc_uuid_to_connection(struct obd_uuid *uuid)
{
        struct zestrpc_connection *c;
        lnet_nid_t                self;
        lnet_process_id_t         peer;
        int                       err;
	
        err = zestrpc_uuid_to_peer(uuid, &peer, &self);
        if (err != 0) {
                CERROR("cannot find peer %s!\n", uuid->uuid);
                return NULL;
        }

        c = zestrpc_get_connection(peer, self, uuid);
        if (c) {
                memcpy(c->c_remote_uuid.uuid,
                       uuid->uuid, sizeof(c->c_remote_uuid.uuid));
        }

        CDEBUG(D_INFO, "%s -> %p\n", uuid->uuid, c);

        return c;
}
#endif

#if 0
static struct zestrpc_request *
zestrpc_prep_req_from_pool(struct zestrpc_request_pool *pool)
{
        struct zestrpc_request *request;
        struct zest_msg *reqmsg;

        if (!pool)
                return NULL;

        spin_lock(&pool->prp_lock);

        /* See if we have anything in a pool, and bail out if nothing,    
	 * in writeout path, where this matters, this is safe to do, because
	 * nothing is lost in this case, and when some in-flight requests      
	 * complete, this code will be called again. */
        if (unlikely(zlist_empty(&pool->prp_req_list))) {
                spin_unlock(&pool->prp_lock);
                return NULL;
        }
	
        request = zlist_entry(pool->prp_req_list.next, struct zestrpc_request,
                             rq_list);
        zlist_del(&request->rq_list);
        spin_unlock(&pool->prp_lock);

        LASSERT(request->rq_reqmsg);
        LASSERT(request->rq_pool);

        reqmsg = request->rq_reqmsg;
        memset(request, 0, sizeof(*request));
        request->rq_reqmsg = reqmsg;
        request->rq_pool = pool;
        request->rq_reqlen = pool->prp_rq_size;
        return request;
}
#endif

struct zestrpc_request *
zestrpc_prep_req_pool(struct zestrpc_import *imp, 
		     u32 version, int opcode, int count, 
		     int *lengths, char **bufs,
                     struct zestrpc_request_pool *pool)
{
        struct zestrpc_request *request = NULL;
        int rc;
        ENTRY;

        LASSERT((unsigned long)imp > 0x1000);
        LASSERT(imp != LP_POISON);
        LASSERT((unsigned long)imp->imp_client > 0x1000);
        LASSERT(imp->imp_client != LP_POISON);

        if (pool)
                //request = zestrpc_prep_req_from_pool(pool);
		zfatalx("Pools are not yet supported by ZestRpc");

        if (!request)
                ZOBD_ALLOC(request, sizeof(*request));

        if (!request) {
                CERROR("request allocation out of memory\n");
                RETURN(NULL);
        }

        rc = zest_pack_request(request, count, lengths, bufs);
        if (rc) {
                LASSERT(!request->rq_pool);
                ZOBD_FREE(request, sizeof(*request));
                RETURN(NULL);
        }

	zinfo("request %p request->rq_reqmsg %p", 
	      request, request->rq_reqmsg);

        request->rq_reqmsg->version |= version;

        if (imp->imp_server_timeout)
                request->rq_timeout = ZOBD_TIMEOUT / 2;
        else
                request->rq_timeout = ZOBD_TIMEOUT;

        request->rq_send_state = ZEST_IMP_FULL;
        request->rq_type = ZEST_RPC_MSG_REQUEST;
        request->rq_import = import_get(imp);
        request->rq_export = NULL;

        request->rq_req_cbid.cbid_fn  = zrequest_out_callback;
        request->rq_req_cbid.cbid_arg = request;

        request->rq_reply_cbid.cbid_fn  = zreply_in_callback;
        request->rq_reply_cbid.cbid_arg = request;

        request->rq_phase = ZRQ_PHASE_NEW;

        /* XXX FIXME bug 249 */
        request->rq_request_portal = imp->imp_client->cli_request_portal;
        request->rq_reply_portal = imp->imp_client->cli_reply_portal;

        spin_lock_init(&request->rq_lock);
        INIT_ZLIST_HEAD(&request->rq_list_entry);
        //INIT_ZLIST_HEAD(&request->rq_replay_list);
        INIT_ZLIST_HEAD(&request->rq_set_chain);
        init_waitqueue_head(&request->rq_reply_waitq);
        request->rq_xid = zestrpc_next_xid();
        atomic_set(&request->rq_refcount, 1);

        request->rq_reqmsg->opc = opcode;
        request->rq_reqmsg->flags = 0;

        RETURN(request);
}

struct zestrpc_request *
zestrpc_prep_req(struct zestrpc_import *imp, __u32 version, int opcode,
		 int count, int *lengths, char **bufs)
{
        return zestrpc_prep_req_pool(imp, version, opcode, count, lengths,
				     bufs, NULL);
}


static inline struct zestrpc_bulk_desc *
new_bulk(int npages, int type, int portal)
{
        struct zestrpc_bulk_desc *desc;

        ZOBD_ALLOC(desc, offsetof (struct zestrpc_bulk_desc, bd_iov[npages]));
        if (!desc)
                return NULL;

        spin_lock_init(&desc->bd_lock);
        init_waitqueue_head(&desc->bd_waitq);
        desc->bd_max_iov = npages;
        desc->bd_iov_count = 0;
        desc->bd_md_h = LNET_INVALID_HANDLE;
        desc->bd_portal = portal;
        desc->bd_type = type;

        return desc;
}

struct zestrpc_bulk_desc *
zestrpc_prep_bulk_imp (struct zestrpc_request *req, int npages, 
		      int type, int portal)
{
        struct zestrpc_import    *imp = req->rq_import;
        struct zestrpc_bulk_desc *desc;

        LASSERT(type == BULK_PUT_SINK || type == BULK_GET_SOURCE);
        desc = new_bulk(npages, type, portal);
        if (desc == NULL)
                RETURN(NULL);

        //desc->bd_import_generation = req->rq_import_generation;
	desc->bd_connection = imp->imp_connection;
        desc->bd_import = import_get(imp);
        desc->bd_req    = req;

        desc->bd_cbid.cbid_fn  = zclient_bulk_callback;
        desc->bd_cbid.cbid_arg = desc;

        /* This makes req own desc, and free it when she frees herself */
        req->rq_bulk = desc;

        return desc;
}

struct zestrpc_bulk_desc *
zestrpc_prep_bulk_exp (struct zestrpc_request *req,
		      int npages, int type, int portal)
{
        struct zestrpc_export   *exp = req->rq_export;
        struct zestrpc_bulk_desc *desc;

        LASSERT(type == BULK_PUT_SOURCE || type == BULK_GET_SINK);

        desc = new_bulk(npages, type, portal);
        if (desc == NULL)
                RETURN(NULL);

	desc->bd_connection = req->rq_conn;
        desc->bd_export = zclass_export_get(exp);
        desc->bd_req = req;

        desc->bd_cbid.cbid_fn  = zserver_bulk_callback;
        desc->bd_cbid.cbid_arg = desc;

        /* NB we don't assign rq_bulk here; server-side requests are 
         * Re-Used, and the handler frees the bulk desc explicitly. */
        return desc;
}

struct zestrpc_request_set * 
zestrpc_prep_set(void)
{
        struct zestrpc_request_set *set;

        ZOBD_ALLOC(set, sizeof *set);
        if (!set)
                RETURN(NULL);
        INIT_ZLIST_HEAD(&set->set_requests);
        init_waitqueue_head(&set->set_waitq);
        set->set_remaining = 0;
        spin_lock_init(&set->set_new_req_lock);
        INIT_ZLIST_HEAD(&set->set_new_requests);

        RETURN(set);
}

/* lock so many callers can add things, the context that owns the set 
 * is supposed to notice these and move them into the set proper. */
void 
zestrpc_set_add_new_req(struct zestrpc_request_set *set,
			struct zestrpc_request     *req)
{
        spin_lock(&set->set_new_req_lock);
        /* The set takes over the caller's request reference */
        //zlist_add_tail(&req->rq_set_chain, &set->set_new_requests);
        zlist_add_tail(&req->rq_set_chain, &set->set_requests);
        req->rq_set = set;
	set->set_remaining++;
        atomic_inc(&req->rq_import->imp_inflight);
        spin_unlock(&set->set_new_req_lock);
}

/**
 * expired_request - timeout handler used when sending a msg
 * @data: the request
 * Notes: Modified to allow requests to be retried without causing a failure of the entire import.  When the request fails 'imp_max_retries' times, zestrpc_expire_one_request() is called and the import is failed. 
 */
static int expired_request(void *data)
{
        struct zestrpc_request *req = data;
	struct zestrpc_import *imp = req->rq_import;
        ENTRY;

	atomic_inc(&req->rq_retries);	

	DEBUG_REQ(ZLL_INFO, req, "request timeout");

	if (atomic_read(&req->rq_retries) >= imp->imp_max_retries) { 
		RETURN(zestrpc_expire_one_request(req));		
	}

	spin_lock(&req->rq_lock);
        req->rq_resend = 1;
        spin_unlock(&req->rq_lock);
	
	return 0;
}

static void interrupted_request(void *data)
{
        struct zestrpc_request *req = data;
        DEBUG_REQ(ZLL_INFO, req, "request interrupted");
        spin_lock(&req->rq_lock);
        req->rq_intr = 1;
        spin_unlock(&req->rq_lock);
}

static int zestrpc_send_new_req(struct zestrpc_request *req)
{
        struct zestrpc_import     *imp;
        int rc;
        ENTRY;
	
        LASSERT(req->rq_phase == ZRQ_PHASE_NEW);
        req->rq_phase = ZRQ_PHASE_RPC;
	
        imp = req->rq_import;
        spin_lock(&imp->imp_lock);
	
        req->rq_import_generation = imp->imp_generation;
	
#if 0
        if (zestrpc_import_delay_req(imp, req, &rc)) {
                spin_lock (&req->rq_lock);
                req->rq_waiting = 1;
                spin_unlock (&req->rq_lock);
		
                DEBUG_REQ(D_HA, req, "req from PID %d waiting for recovery: "
			  "(%s != %s)",
			  req->rq_reqmsg->status,
			zestrpc_import_state_name(req->rq_send_state),
			  zestrpc_import_state_name(imp->imp_state));
		LASSERT(zlist_empty (&req->rq_list_entry));
		
                zlist_add_tail(&req->rq_list_entry, &imp->imp_delayed_list);
                spin_unlock(&imp->imp_lock);
                RETURN(0);
        }

        if (rc != 0) {
                spin_unlock(&imp->imp_lock);
                req->rq_status = rc;
                req->rq_phase = ZRQ_PHASE_INTERPRET;
                RETURN(rc);
        }
#endif
	
        /* XXX this is the same as zestrpc_queue_wait */
        LASSERT(zlist_empty(&req->rq_list_entry));
        zlist_add_tail(&req->rq_list_entry, &imp->imp_sending_list);
	spin_unlock(&imp->imp_lock);
	
	// we don't have a 'current' struct - paul
        //req->rq_reqmsg->status = current->pid;
	zinfo("Sending RPC pname:cluuid:pid:xid:nid:opc"
	      " %d:"LPU64":%s:%d\n", 
	      req->rq_reqmsg->status,
	      req->rq_xid,
	      libcfs_nid2str(imp->imp_connection->c_peer.nid),
	      req->rq_reqmsg->opc);
	
	rc = zest_send_rpc(req, 0);
	if (rc) {
		DEBUG_REQ(ZLL_WARN, req, 
			  "send failed (%d); expect timeout", rc);
		req->rq_net_err = 1;
		RETURN(rc);
	}
	RETURN(0);
}

int zestrpc_push_req(struct zestrpc_request *req) {
	return (zestrpc_send_new_req(req));
}


static int zestrpc_check_reply(struct zestrpc_request *req)
{
	int rc = 0;
	ENTRY;

	/* serialise with network callback */
	spin_lock(&req->rq_lock);
	
	if (req->rq_replied) {
		DEBUG_REQ(ZLL_INFO, req, "REPLIED:");
		GOTO(out, rc = 1);
	}
	
	if (req->rq_net_err && !req->rq_timedout) {
		DEBUG_REQ(ZLL_ERROR, req, "NET_ERR: %d", req->rq_net_err);
		spin_unlock(&req->rq_lock);
		rc = zestrpc_expire_one_request(req);
		spin_lock(&req->rq_lock);
		GOTO(out, rc);
	}
	
	if (req->rq_err) {
		DEBUG_REQ(ZLL_WARN, req, "ABORTED:");
		GOTO(out, rc = 1);
	}
	
	if (req->rq_resend) {
		DEBUG_REQ(ZLL_WARN, req, "RESEND:");
		GOTO(out, rc = 1);
	}
	
	if (req->rq_restart) {
		DEBUG_REQ(ZLL_WARN, req, "RESTART:");
		GOTO(out, rc = 1);
	}
	EXIT;
 out:
	spin_unlock(&req->rq_lock);
	DEBUG_REQ(ZLL_INFO, req, "rc = %d for", rc);
	return rc;
}

void zestrpc_unregister_reply (struct zestrpc_request *request)
{
        int                rc;
        wait_queue_head_t *wq;
        struct l_wait_info lwi;

        LASSERT(!in_interrupt ());             /* might sleep */

        if (!zestrpc_client_receiving_reply(request))
                return;

        LNetMDUnlink (request->rq_reply_md_h);

        /* We have to l_wait_event() whatever the result, to give liblustre     
	 * a chance to run reply_in_callback() */
	
        if (request->rq_set != NULL)
                wq = &request->rq_set->set_waitq;
        else
                wq = &request->rq_reply_waitq;
	
        for (;;) {
                /* Network access will complete in finite time but the HUGE   
		 * timeout lets us CWARN for visibility of sluggish NALs */
                lwi = LWI_TIMEOUT(300, NULL, NULL);
                rc = zcli_wait_event (*wq, 
				      !zestrpc_client_receiving_reply(request),
				      &lwi);
                if (rc == 0)
                        return;

                LASSERT (rc == -ETIMEDOUT);
                DEBUG_REQ(ZLL_WARN, request, "Unexpectedly long timeout");
        }
}


static int zestrpc_check_status(struct zestrpc_request *req)
{
        int err;
        ENTRY;

        err = req->rq_repmsg->status;
        if (req->rq_repmsg->type == ZEST_RPC_MSG_ERR) {
                DEBUG_REQ(ZLL_ERROR, req, "type == ZPTL_RPC_MSG_ERR, err == %d",
                          err);
                RETURN(err < 0 ? err : -EINVAL);
        }

        if (err < 0) {
                DEBUG_REQ(ZLL_INFO, req, "status is %d", err);
        } else if (err > 0) {
                /* XXX: translate this error from net to host */
                DEBUG_REQ(ZLL_INFO, req, "status is %d", err);
        }

        RETURN(err);
}


static int after_reply(struct zestrpc_request *req)
{
        //struct zestrpc_import *imp = req->rq_import;
        int rc;
        ENTRY;

        LASSERT(!req->rq_receiving_reply);

        /* NB Until this point, the whole of the incoming message,
	 * including buflens, status etc is in the sender's byte order. */

#if SWAB_PARANOIA
        /* Clear reply swab mask; this is a new reply in sender's byte order */
        req->rq_rep_swab_mask = 0;
#endif
        LASSERT (req->rq_nob_received <= req->rq_replen);
        rc = zest_unpack_msg(req->rq_repmsg, req->rq_nob_received);
        if (rc) {
                DEBUG_REQ(ZLL_ERROR, req, "unpack_rep failed: %d", rc);
                RETURN(-EPROTO);
        }

        if (req->rq_repmsg->type != ZEST_RPC_MSG_REPLY &&
            req->rq_repmsg->type != ZEST_RPC_MSG_ERR) {
                DEBUG_REQ(ZLL_ERROR, req, "invalid packet received (type=%u)",
                          req->rq_repmsg->type);
                RETURN(-EPROTO);
        }

        rc = zestrpc_check_status(req);

        /* Either we've been evicted, or the server has failed for
	 * some reason. Try to reconnect, and if that fails, punt to the 
	 * upcall. */
        if ((rc == -ENOTCONN) || (rc == -ENODEV)) {
		zerrorx("ENOTCONN or ENODEV");
#if 0
                if (req->rq_send_state != ZEST_IMP_FULL ||
                    imp->imp_obd->obd_no_recov || imp->imp_dlm_fake) {
                        RETURN(-ENOTCONN);
                }

                zestrpc_request_handle_notconn(req);
#endif
                RETURN(rc);
        }

        /* Store transno in reqmsg for replay. */
        req->rq_reqmsg->transno = req->rq_transno = req->rq_repmsg->transno;

#if 0
        if (req->rq_import->imp_replayable) {
                spin_lock(&imp->imp_lock);
                if (req->rq_transno != 0)
                        zestrpc_retain_replayable_request(req, imp);
                else if (req->rq_commit_cb != NULL) {
                        spin_unlock(&imp->imp_lock);
                        req->rq_commit_cb(req);
                        spin_lock(&imp->imp_lock);
                }

                /* Replay-enabled imports return commit-status information. */
                if (req->rq_repmsg->last_committed)
                        imp->imp_peer_committed_transno =
                                req->rq_repmsg->last_committed;
                zestrpc_free_committed(imp);
                spin_unlock(&imp->imp_lock);
        }
#endif
        RETURN(rc);
}

int zestrpc_queue_wait(struct zestrpc_request *req)
{
        int rc = 0;
        int brc;
        struct l_wait_info lwi;
        struct zestrpc_import *imp = req->rq_import;
        int timeout = 0;
        ENTRY;
	
        LASSERT(req->rq_set == NULL);
        LASSERT(!req->rq_receiving_reply);
        atomic_inc(&imp->imp_inflight);

        LASSERT(imp != NULL);
        zinfo("Sending RPC cookie:pid:xid:nid:opc "
	      LPX64":%d:"LPU64":%s:%d", 
	      req->rq_reqmsg->handle.cookie, 
	      imp->imp_connection->c_peer.pid, req->rq_xid,
	      libcfs_nid2str(imp->imp_connection->c_peer.nid),
	      req->rq_reqmsg->opc);
	
        /* Mark phase here for a little debug help */
        req->rq_phase = ZRQ_PHASE_RPC;

        spin_lock(&imp->imp_lock);
        req->rq_import_generation = imp->imp_generation;
 restart:
	/*
#if 0
        if (zestrpc_import_delay_req(imp, req, &rc)) {
                zlist_del(&req->rq_list_entry);

                zlist_add_tail(&req->rq_list_entry, &imp->imp_delayed_list);
                spin_unlock(&imp->imp_lock);

                DEBUG_REQ(ZLL_WARN, req, "\"%s\" waiting for recovery: (%s != %s)",
                          current->comm,
                          zestrpc_import_state_name(req->rq_send_state),
                          zestrpc_import_state_name(imp->imp_state));
                lwi = LWI_INTR(interrupted_request, req);
                rc = l_wait_event(req->rq_reply_waitq,
                                  (req->rq_send_state == imp->imp_state ||
                                   req->rq_err || req->rq_intr),
                                  &lwi);
                DEBUG_REQ(ZLL_WARN, req, "\"%s\" awake: (%s == %s or %d/%d == 1)",
                          current->comm,
                          zestrpc_import_state_name(imp->imp_state),
                          zestrpc_import_state_name(req->rq_send_state),
                          req->rq_err, req->rq_intr);

                spin_lock(&imp->imp_lock);
                zlist_del(&req->rq_list_entry);

                if (req->rq_err) {
                        rc = -EIO;
                }
                else if (req->rq_intr) {
                        rc = -EINTR;
                }
                else if (req->rq_no_resend) {
                        spin_unlock(&imp->imp_lock);
                        GOTO(out, rc = -ETIMEDOUT);
                }
                else {
                        GOTO(restart, rc);
                }
        }
#endif
	*/
        if (rc != 0) {
                zlist_del(&req->rq_list_entry);
                spin_unlock(&imp->imp_lock);
                req->rq_status = rc; // XXX this ok?    
                GOTO(out, rc);
        }

        if (req->rq_resend) {
                zest_msg_add_flags(req->rq_reqmsg, MSG_RESENT);

                if (req->rq_bulk != NULL) {
                        zestrpc_unregister_bulk (req);

                        /* bulk requests are supposed to be 
                         * idempotent, so we are free to bump the xid
                         * here, which we need to do before
                         * registering the bulk again (bug 6371).   
                         * print the old xid first for sanity.       
			 */
                        DEBUG_REQ(ZLL_INFO, req, "bumping xid for bulk: ");
                        req->rq_xid = zestrpc_next_xid();
                }

                DEBUG_REQ(ZLL_WARN, req, "resending: ");
        }
        /* XXX this is the same as zestrpc_set_wait */
        LASSERT(zlist_empty(&req->rq_list_entry));
        zlist_add_tail(&req->rq_list_entry, &imp->imp_sending_list);
        spin_unlock(&imp->imp_lock);

	zinfo("request %p request->rq_reqmsg %p", 
	      req, req->rq_reqmsg);

	rc = zest_send_rpc(req, 0);
        if (rc) {
                DEBUG_REQ(ZLL_INFO, req, "send failed (%d); recovering", rc);
                timeout = 1;
        } else {
                //timeout = MAX(req->rq_timeout * HZ, 1);
                timeout = MAX(req->rq_timeout, 1);
                DEBUG_REQ(ZLL_INFO, req, 
			  "-- sleeping for %d jiffies", timeout);
        }
        lwi = LWI_TIMEOUT_INTR(timeout, expired_request, 
			       interrupted_request, req);

        zcli_wait_event(req->rq_reply_waitq, zestrpc_check_reply(req), &lwi);
	DEBUG_REQ(ZLL_INFO, req, "-- done sleeping");
	
	zinfo("Completed RPC status:err:xid:nid:opc %d:%d:"LPX64":%s:%d",
	      req->rq_reqmsg->status, req->rq_err, req->rq_xid,
	      libcfs_nid2str(imp->imp_connection->c_peer.nid),
	      req->rq_reqmsg->opc);
	
        spin_lock(&imp->imp_lock);
        zlist_del_init(&req->rq_list_entry);
        spin_unlock(&imp->imp_lock);

        /* If the reply was received normally, this just grabs the spinlock
         * (ensuring the reply callback has returned), sees that          
	 * req->rq_receiving_reply is clear and returns. */
        zestrpc_unregister_reply (req);

        if (req->rq_err)
                GOTO(out, rc = -EIO);

        /* Resend if we need to, unless we were interrupted. */
        if (req->rq_resend && !req->rq_intr) {
                /* ...unless we were specifically told otherwise. */
                if (req->rq_no_resend)
                        GOTO(out, rc = -ETIMEDOUT);
                spin_lock(&imp->imp_lock);
                goto restart;
        }

        if (req->rq_intr) {
                /* Should only be interrupted if we timed out. */
                if (!req->rq_timedout)
			DEBUG_REQ(ZLL_ERROR, req, 
				  "rq_intr set but rq_timedout not");
                GOTO(out, rc = -EINTR);
        }

        if (req->rq_timedout) {                 
		/* non-recoverable timeout */
                GOTO(out, rc = -ETIMEDOUT);
        }

        if (!req->rq_replied) {
                /* How can this be? -eeb */
                DEBUG_REQ(ZLL_ERROR, req, "!rq_replied: ");
                LBUG();
                GOTO(out, rc = req->rq_status);
        }

        rc = after_reply (req);
        /* NB may return +ve success rc */
        if (req->rq_resend) {
                spin_lock(&imp->imp_lock);
                goto restart;
        }

 out:
        if (req->rq_bulk != NULL) {
                if (rc >= 0) {
                        /* success so far.  Note that anything going wrong
			 * with bulk now, is EXTREMELY strange, since the  
			 * server must have believed that the bulk  
			 * tranferred OK before she replied with success to 
			 * me. */
                        lwi = LWI_TIMEOUT(timeout, NULL, NULL);
                        brc = zcli_wait_event(req->rq_reply_waitq,
					      !zestrpc_bulk_active(req->rq_bulk),
					      &lwi);
                        LASSERT(brc == 0 || brc == -ETIMEDOUT);
                        if (brc != 0) {
                                LASSERT(brc == -ETIMEDOUT);
                                DEBUG_REQ(ZLL_ERROR, req, "bulk timed out");
                                rc = brc;
                        } else if (!req->rq_bulk->bd_success) {
                                DEBUG_REQ(ZLL_ERROR, req, 
					  "bulk transfer failed");
                                rc = -EIO;
                        }
                }
                if (rc < 0)
                        zestrpc_unregister_bulk (req);
        }

        LASSERT(!req->rq_receiving_reply);
        req->rq_phase = ZRQ_PHASE_INTERPRET;

        atomic_dec(&imp->imp_inflight);
        wake_up(&imp->imp_recovery_waitq);
        RETURN(rc);
}

/** 
 * zestrpc_check_set - send unsent RPCs in @set.  If check_allsent, 
 *     returns TRUE if all are sent otherwise return true if at least one 
 *     has completed.
 * @set:  the set in question
 * @check_allsent: return true only if all requests are finished, set to '1'
 *                 for original lustre behavior.
 * NOTES: check_allsent was added to support single, non-blocking sends 
 *        implemented within the context of an rpcset.
 */
int zestrpc_check_set(struct zestrpc_request_set *set, int check_allsent)
{
        struct zlist_head *tmp;
        int force_timer_recalc = 0;
	int ncompleted         = 0;
        ENTRY;

        if (set->set_remaining == 0)
                RETURN(1);

        zlist_for_each(tmp, &set->set_requests) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_set_chain);
                struct zestrpc_import *imp = req->rq_import;
                int rc = 0;

		DEBUG_REQ(ZLL_WARN, req, "debug");

                if (req->rq_phase == ZRQ_PHASE_NEW &&
                    zestrpc_send_new_req(req)) {
			DEBUG_REQ(ZLL_WARN, req, "ZRQ_PHASE_NEW");
                        force_timer_recalc = 1;
                }

                if (!(req->rq_phase == ZRQ_PHASE_RPC ||
                      req->rq_phase == ZRQ_PHASE_BULK ||
                      req->rq_phase == ZRQ_PHASE_INTERPRET ||
                      req->rq_phase == ZRQ_PHASE_COMPLETE)) {
                        DEBUG_REQ(ZLL_ERROR, req, "bad phase %x", 
				  req->rq_phase);
                        LBUG();
                }

                if (req->rq_phase == ZRQ_PHASE_COMPLETE) {
			ncompleted++;
                        continue;
		}

                if (req->rq_phase == ZRQ_PHASE_INTERPRET)
                        GOTO(interpret, req->rq_status);

                if (req->rq_net_err && !req->rq_timedout)
                        zestrpc_expire_one_request(req);

                if (req->rq_err) {
                        zestrpc_unregister_reply(req);
                        if (req->rq_status == 0)
                                req->rq_status = -EIO;
                        req->rq_phase = ZRQ_PHASE_INTERPRET;

                        spin_lock(&imp->imp_lock);
                        zlist_del(&req->rq_list_entry);
                        spin_unlock(&imp->imp_lock);

                        GOTO(interpret, req->rq_status);
                }

                /* zestrpc_queue_wait->l_wait_event guarantees that rq_intr 
		 * will only be set after rq_timedout, but the oig waiting 
		 * path sets rq_intr irrespective of whether zestrpcd has   
		 * seen a timeout.  our policy is to only interpret 
		 * interrupted rpcs after they have timed out */
                if (req->rq_intr && (req->rq_timedout || req->rq_waiting)) {
                        /* NB could be on delayed list */
                        zestrpc_unregister_reply(req);
                        req->rq_status = -EINTR;
                        req->rq_phase = ZRQ_PHASE_INTERPRET;

                        spin_lock(&imp->imp_lock);
                        zlist_del(&req->rq_list_entry);
                        spin_unlock(&imp->imp_lock);

                        GOTO(interpret, req->rq_status);
                }

                if (req->rq_phase == ZRQ_PHASE_RPC) {
                        if (req->rq_timedout||req->rq_waiting||req->rq_resend) {
                                int status;
                                zestrpc_unregister_reply(req);

                                spin_lock(&imp->imp_lock);

#if 0
                                if (zestrpc_import_delay_req(imp, req, &status)){
                                        spin_unlock(&imp->imp_lock);
                                        continue;
                                }
#endif
                                zlist_del(&req->rq_list_entry);
                                if (status != 0) {
                                        req->rq_status = status;
                                        req->rq_phase = ZRQ_PHASE_INTERPRET;
                                        spin_unlock(&imp->imp_lock);
                                        GOTO(interpret, req->rq_status);
                                }
                                if (req->rq_no_resend) {
                                        req->rq_status = -ENOTCONN;
                                        req->rq_phase = ZRQ_PHASE_INTERPRET;
                                        spin_unlock(&imp->imp_lock);
                                        GOTO(interpret, req->rq_status);
                                }
                                zlist_add_tail(&req->rq_list_entry,
                                              &imp->imp_sending_list);

                                spin_unlock(&imp->imp_lock);

                                req->rq_waiting = 0;
                                if (req->rq_resend) {
                                        zest_msg_add_flags(req->rq_reqmsg,
                                                             MSG_RESENT);
                                        if (req->rq_bulk) {
                                                u64 old_xid = req->rq_xid;

                                                zestrpc_unregister_bulk (req);

                                                /* ensure previous bulk fails */
                                                req->rq_xid = zestrpc_next_xid();
						CDEBUG(D_HA, "resend bulk "
                                                       "old x%"ZLPU64
                                                       " new x%"ZLPU64"\n",
                                                       old_xid, req->rq_xid);
                                        }
                                }

                                rc = zest_send_rpc(req, 0);
                                if (rc) {
                                        DEBUG_REQ(ZLL_ERROR, req, 
						  "send failed (%d)", rc);
                                        force_timer_recalc = 1;
                                        req->rq_net_err = 1;
                                }
                                /* need to reset the timeout */
                                force_timer_recalc = 1;
                        }

                        /* Still waiting for a reply? */
                        if (zestrpc_client_receiving_reply(req))
                                continue;

                        /* Did we actually receive a reply? */
                        if (!zestrpc_client_replied(req))
                                continue;

                        spin_lock(&imp->imp_lock);
                        zlist_del_init(&req->rq_list_entry);
                        spin_unlock(&imp->imp_lock);

                        req->rq_status = after_reply(req);
                        if (req->rq_resend) {
                                /* Add this req to the delayed list so    
				   it can be errored if the import is 
				   evicted after recovery. */
                                spin_lock(&req->rq_lock);
                                zlist_add_tail(&req->rq_list_entry,
                                              &imp->imp_delayed_list);
                                spin_unlock(&req->rq_lock);
				continue;
                        }

                        /* If there is no bulk associated with this request, 
			 * then we're done and should let the interpreter   
			 * process the reply.  Similarly if the RPC returned  
			 * an error, and therefore the bulk will never arrive
			 */
                        if (req->rq_bulk == NULL || req->rq_status != 0) {
                                req->rq_phase = ZRQ_PHASE_INTERPRET;
                                GOTO(interpret, req->rq_status);
                        }

                        req->rq_phase = ZRQ_PHASE_BULK;
                }

                LASSERT(req->rq_phase == ZRQ_PHASE_BULK);
                if (zestrpc_bulk_active(req->rq_bulk))
                        continue;

                if (!req->rq_bulk->bd_success) {
                        /* The RPC reply arrived OK, but the bulk screwed            
                         * up!  Dead wierd since the server told us the RPC          
                         * was good after getting the REPLY for her GET or           
                         * the ACK for her PUT. */
                        DEBUG_REQ(ZLL_ERROR, req, "bulk transfer failed");
                        LBUG();
                }

                req->rq_phase = ZRQ_PHASE_INTERPRET;

        interpret:
                LASSERT(req->rq_phase == ZRQ_PHASE_INTERPRET);
                LASSERT(!req->rq_receiving_reply);

                zestrpc_unregister_reply(req);
                if (req->rq_bulk != NULL)
                        zestrpc_unregister_bulk (req);

                req->rq_phase = ZRQ_PHASE_COMPLETE;
		ncompleted++;

                if (req->rq_interpret_reply != NULL) {
                        int (*interpreter)(struct zestrpc_request *,void *,int) = 
				req->rq_interpret_reply;
                        req->rq_status = interpreter(req, &req->rq_async_args,
                                                     req->rq_status);
                }

                zdbg("Completed RPC pname:cluuid:pid:xid:nid:"
		       "opc %d:"LPU64":%s:%d",
		       req->rq_reqmsg->status,
                       req->rq_xid,
                       libcfs_nid2str(imp->imp_connection->c_peer.nid),
                       req->rq_reqmsg->opc);

                set->set_remaining--;

                atomic_dec(&imp->imp_inflight);
                wake_up(&imp->imp_recovery_waitq);
        }

        /* If we hit an error, we want to recover promptly. */
	if (check_allsent)
		/* old behavior */
		RETURN(set->set_remaining == 0 || force_timer_recalc);
	else
		/* new (single non-blocking req) behavior */
		RETURN(ncompleted || force_timer_recalc);
}

void zestrpc_set_destroy(struct zestrpc_request_set *set)
{
        struct zlist_head *tmp;
        struct zlist_head *next;
        unsigned expected_phase;
        int               n = 0;
        ENTRY;

        /* Requests on the set should either all be completed, or all be new */
        expected_phase = (set->set_remaining == 0) ?
		ZRQ_PHASE_COMPLETE : ZRQ_PHASE_NEW;
        zlist_for_each (tmp, &set->set_requests) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_set_chain);

                LASSERT(req->rq_phase == expected_phase);
                n++;
        }

        LASSERT(set->set_remaining == 0 || set->set_remaining == n);

        zlist_for_each_safe(tmp, next, &set->set_requests) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_set_chain);
                zlist_del_init(&req->rq_set_chain);

                LASSERT(req->rq_phase == expected_phase);

                if (req->rq_phase == ZRQ_PHASE_NEW) {

                        if (req->rq_interpret_reply != NULL) {
                                int (*interpreter)(struct zestrpc_request *,
                                                   void *, int) =
                                        req->rq_interpret_reply;

                                /* higher level (i.e. LOV) failed;                                  
                                 * let the sub reqs clean up */
                                req->rq_status = -EBADR;
                                interpreter(req, &req->rq_async_args,
                                            req->rq_status);
                        }
                        set->set_remaining--;
                }

                req->rq_set = NULL;
                zestrpc_req_finished (req);
        }

        LASSERT(set->set_remaining == 0);

        ZOBD_FREE(set, sizeof(*set));
        EXIT;
}


int zestrpc_expire_one_request(struct zestrpc_request *req)
{
        struct zestrpc_import *imp = req->rq_import;
        ENTRY;

        DEBUG_REQ(ZLL_ERROR, req, 
		  "timeout (sent at %lu, %lus ago)", 
		  (long)req->rq_sent, CURRENT_SECONDS - req->rq_sent);
#if 0
        /* Leave this out normally: it creates too much console noise and we
	 * only need it when we're debugging networks */
        if (imp != NULL)
                LNetCtl(IOC_LIBCFS_DEBUG_PEER, &imp->imp_connection->c_peer);
#endif
        spin_lock(&req->rq_lock);
        req->rq_timedout = 1;
        spin_unlock(&req->rq_lock);

        zestrpc_unregister_reply (req);

        //if (obd_dump_on_timeout)
        //        libcfs_debug_dumplog();

        if (req->rq_bulk != NULL)
                zestrpc_unregister_bulk (req);
	
        if (imp == NULL) {
                DEBUG_REQ(ZLL_WARN, req, "NULL import: already cleaned up?");
                RETURN(1);
        }

        /* If this request is for recovery or other primordial tasks,     
	 * then error it out here. */
        //if (req->rq_send_state != ZEST_IMP_FULL ||
        //    imp->imp_obd->obd_no_recov) {

	if (req->rq_send_state != ZEST_IMP_FULL) { 
                spin_lock(&req->rq_lock);
                req->rq_status = -ETIMEDOUT;
                req->rq_err = 1;
                spin_unlock(&req->rq_lock);
                RETURN(1);
        }

        zestrpc_fail_import(imp, req->rq_reqmsg->conn_cnt);
	
        RETURN(0);
}

int zestrpc_expired_set(void *data)
{
        struct zestrpc_request_set *set = data;
        struct zlist_head          *tmp;
        time_t                     now = CURRENT_SECONDS;
        ENTRY;

        LASSERT(set != NULL);

        /* A timeout expired; see which reqs it applies to... */
        zlist_for_each (tmp, &set->set_requests) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_set_chain);

                /* request in-flight? */
                if (!((req->rq_phase == ZRQ_PHASE_RPC && !req->rq_waiting &&
                       !req->rq_resend) ||
                      (req->rq_phase == ZRQ_PHASE_BULK)))
                        continue;

                if (req->rq_timedout ||           /* already dealt with */
                    req->rq_sent + req->rq_timeout > now) /* not expired */
                        continue;

                /* deal with this guy */
                zestrpc_expire_one_request (req);
        }

        /* When waiting for a whole set, we always to break out of the    
	 * sleep so we can recalculate the timeout, or enable interrupts     
	 * iff everyone's timed out.                        
	 */
        RETURN(1);
}


int zestrpc_set_next_timeout(struct zestrpc_request_set *set)
{
        struct zlist_head      *tmp;
        time_t                 now = CURRENT_SECONDS;
        time_t                 deadline;
        int                    timeout = 0;
        struct zestrpc_request *req;
        ENTRY;

        //SIGNAL_MASK_ASSERT(); /* XXX BUG 1511 */

        zlist_for_each(tmp, &set->set_requests) {
                req = zlist_entry(tmp, struct zestrpc_request, rq_set_chain);

                /* request in-flight? */
                if (!((req->rq_phase == ZRQ_PHASE_RPC && !req->rq_waiting) ||
                      (req->rq_phase == ZRQ_PHASE_BULK)))
                        continue;

                if (req->rq_timedout)   /* already timed out */
                        continue;

                deadline = req->rq_sent + req->rq_timeout;
                if (deadline <= now)    /* actually expired already */
                        timeout = 1;    /* ASAP */
                else if (timeout == 0 || timeout > deadline - now)
                        timeout = deadline - now;
        }
        RETURN(timeout);
}

 void zestrpc_mark_interrupted(struct zestrpc_request *req)
{
        spin_lock(&req->rq_lock);
        req->rq_intr = 1;
        spin_unlock(&req->rq_lock);
}

void zestrpc_interrupted_set(void *data)
{
        struct zestrpc_request_set *set = data;
        struct zlist_head *tmp;

        LASSERT(set != NULL);
        CERROR("INTERRUPTED SET %p\n", set);

        zlist_for_each(tmp, &set->set_requests) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_set_chain);

                if (req->rq_phase != ZRQ_PHASE_RPC)
                        continue;

                zestrpc_mark_interrupted(req);
	}
}

#if WEDONTNEEDTHISRIGTHNOW
static int zestrpc_import_delay_req(struct zestrpc_import *imp,
                                   struct zestrpc_request *req, int *status)
{
        int delay = 0;
        ENTRY;

        LASSERT (status != NULL);
        *status = 0;

        if (imp->imp_state == ZEST_IMP_NEW) {
                DEBUG_REQ(ZLL_ERROR, req, "Uninitialized import.");
                *status = -EIO;
                LBUG();
        } else if (imp->imp_state == ZEST_IMP_CLOSED) {
                DEBUG_REQ(ZLL_ERROR, req, "IMP_CLOSED ");
                *status = -EIO;
        } else if (req->rq_send_state == ZEST_IMP_CONNECTING &&
		   imp->imp_state == ZEST_IMP_CONNECTING) {
                /* allow CONNECT even if import is invalid */ ;
        } else if (imp->imp_invalid) {
                /* If the import has been invalidated (such as by an OST         
		 * failure), the request must fail with -EIO. */
                if (!imp->imp_deactive)
                        DEBUG_REQ(ZLL_ERROR, req, "IMP_INVALID");
                *status = -EIO;
        } else if (req->rq_import_generation != imp->imp_generation) {
                DEBUG_REQ(ZLL_ERROR, req, "req wrong generation:");
                *status = -EIO;
        } else if (req->rq_send_state != imp->imp_state) {
                //if (imp->imp_obd->obd_no_recov || imp->imp_dlm_fake ||
                if (req->rq_no_delay)                    
                        *status = -EWOULDBLOCK;
                else
                        delay = 1;
        }
	
        RETURN(delay);
}
#endif 
 
int zestrpc_set_wait(struct zestrpc_request_set *set)
{
        struct zlist_head      *tmp;
        struct zestrpc_request *req;
        struct l_wait_info     lwi;
        int                    rc, timeout;
        ENTRY;

        if (zlist_empty(&set->set_requests))
                RETURN(0);

        zlist_for_each(tmp, &set->set_requests) {
                req = zlist_entry(tmp, struct zestrpc_request, rq_set_chain);
                if (req->rq_phase == ZRQ_PHASE_NEW)
                        (void)zestrpc_send_new_req(req);
        }

        do {
                timeout = zestrpc_set_next_timeout(set);

                /* wait until all complete, interrupted, or an in-flight    
		 * req times out */
                CDEBUG(D_NET, "set %p going to sleep for %d seconds\n",
                       set, timeout);
                lwi = LWI_TIMEOUT_INTR((timeout ? timeout : 1) * HZ,
                                       zestrpc_expired_set,
                                       zestrpc_interrupted_set, set);

                rc = zcli_wait_event(set->set_waitq,
				     zestrpc_check_set(set, 1), &lwi);

                LASSERT(rc == 0 || rc == -EINTR || rc == -ETIMEDOUT);

                /* -EINTR => all requests have been flagged rq_intr so next
                 * check completes.                             
		 * -ETIMEOUTD => someone timed out.  When all reqs have     
		 * timed out, signals are enabled allowing completion with    
		 * EINTR.                                     
		 * I don't really care if we go once more round the loop in    
		 * the error cases -eeb. */
        } while (rc != 0 || set->set_remaining != 0);
	
        LASSERT(set->set_remaining == 0);
	
        rc = 0;
        zlist_for_each(tmp, &set->set_requests) {
                req = zlist_entry(tmp, struct zestrpc_request, rq_set_chain);
		
                LASSERT(req->rq_phase == ZRQ_PHASE_COMPLETE);
                if (req->rq_status != 0)
                        rc = req->rq_status;
        }
	
        if (set->set_interpret != NULL) {
		set_interpreter_func interpreter = set->set_interpret;
                rc = interpreter (set, set->set_arg, rc);
        }
	
        RETURN(rc);
}

#if 0
void zestrpc_free_committed(struct zestrpc_import *imp)
{
        struct zlist_head *tmp, *saved;
        struct zestrpc_request *req;
        struct zestrpc_request *last_req = NULL; /* temporary fire escape */
        ENTRY;

        LASSERT(imp != NULL);

        LASSERT_SPIN_LOCKED(&imp->imp_lock);

        if (imp->imp_peer_committed_transno == imp->imp_last_transno_checked &&
            imp->imp_generation == imp->imp_last_generation_checked) {
                CDEBUG(D_HA, "%s: skip recheck for last_committed "LPU64"\n",
                       imp->imp_obd->obd_name, imp->imp_peer_committed_transno);
                return;
        }

        CDEBUG(D_HA, "%s: committing for last_committed "LPU64" gen %d\n",
               imp->imp_obd->obd_name, imp->imp_peer_committed_transno,
               imp->imp_generation);
        imp->imp_last_transno_checked = imp->imp_peer_committed_transno;
        imp->imp_last_generation_checked = imp->imp_generation;

        zlist_for_each_safe(tmp, saved, &imp->imp_replay_list) {
                req = zlist_entry(tmp, struct zestrpc_request, rq_replay_list);

                /* XXX ok to remove when 1357 resolved - rread 05/29/03  */
                LASSERT(req != last_req);
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

                DEBUG_REQ(D_HA, req, "committing (last_committed "LPU64")",
                          imp->imp_peer_committed_transno);
	free_req:
                if (req->rq_commit_cb != NULL)
                        req->rq_commit_cb(req);
                zlist_del_init(&req->rq_replay_list);
                __zestrpc_req_finished(req, 1);
        }

        EXIT;
        return;
}
#endif


void zestrpc_abort_inflight(struct zestrpc_import *imp)
{
	 struct zlist_head *tmp, *n;
	 ENTRY;

	 /* Make sure that no new requests get processed for this import.     
	  * zestrpc_{queue,set}_wait must (and does) hold imp_lock while testing  
	  * this flag and then putting requests on sending_list or delayed_list.
	  */
	 spin_lock(&imp->imp_lock);

	 /* XXX locking?  Maybe we should remove each request with the list  
	  * locked?  Also, how do we know if the requests on the list are    
	  * being freed at this time?  
	  */
	 zlist_for_each_safe(tmp, n, &imp->imp_sending_list) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_list_entry);

                DEBUG_REQ(ZLL_WARN, req, "inflight");

                spin_lock (&req->rq_lock);
                if (req->rq_import_generation < imp->imp_generation) {
                        req->rq_err = 1;
                        zestrpc_wake_client_req(req);
                }
                spin_unlock (&req->rq_lock);
	 }

#if 0
	 zlist_for_each_safe(tmp, n, &imp->imp_delayed_list) {
                struct zestrpc_request *req =
                        zlist_entry(tmp, struct zestrpc_request, rq_list_entry);

                DEBUG_REQ(ZLL_WARN, req, "aborting waiting req");

                spin_lock (&req->rq_lock);
                if (req->rq_import_generation < imp->imp_generation) {
                        req->rq_err = 1;
                        zestrpc_wake_client_req(req);
                }
                spin_unlock (&req->rq_lock);
	 }

	 /* Last chance to free reqs left on the replay list, but we  
	  * will still leak reqs that haven't committed.  */
	 if (imp->imp_replayable)
		 zestrpc_free_committed(imp);
#endif

	 spin_unlock(&imp->imp_lock);

	 EXIT;
}

void zestrpc_resend_req(struct zestrpc_request *req)
{
        DEBUG_REQ(ZLL_WARN, req, "going to resend");
        req->rq_reqmsg->handle.cookie = 0;
        req->rq_status = -EAGAIN;

        spin_lock(&req->rq_lock);
        req->rq_resend = 1;
        req->rq_net_err = 0;
        req->rq_timedout = 0;
        if (req->rq_bulk) {
                u64 old_xid = req->rq_xid;

                /* ensure previous bulk fails */
                req->rq_xid = zestrpc_next_xid();
                zwarnx("resend bulk old x%"ZLPU64" new x%"ZLPU64,
                       old_xid, req->rq_xid);
        }
        zestrpc_wake_client_req(req);
        spin_unlock(&req->rq_lock);
}

