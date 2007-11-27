/* $Id: zestService.c 2114 2007-11-03 19:39:08Z pauln $ */

#include <stdio.h>

#include "zestAlloc.h"
#include "zestAtomic.h"
#include "zestExport.h"
#include "zestList.h"
#include "zestLock.h"
#include "zestRpc.h"
#include "zestRpcIO.h"
#include "zestRpcLog.h"
#include "zestRpcMDS.h"
#include "zestThread.h"
#include "zestThreadTable.h"
#include "zestWaitq.h"

struct zestion_thread *zestionRPCMDSThreads;
struct zestion_thread *zestionRPCIOThreads;

static int test_req_buffer_pressure = 0;

static int zestrpc_server_post_idle_rqbds (struct zestrpc_service *svc);

static ZLIST_HEAD (zestrpc_all_services);
static spinlock_t zestrpc_all_services_lock = SPIN_LOCK_UNLOCKED;

static char *
zestrpc_alloc_request_buffer (int size)
{
        char *ptr;

	ZOBD_ALLOC(ptr, size);
        return (ptr);
}

static void
zestrpc_free_request_buffer (char *ptr, int size)
{
	ZOBD_FREE(ptr, size);
}

/**
 * zestrpc_alloc_rqbd - create a new request buffer desc and malloc request buffer memory.  This call sets request_in_callback as the callback handler.
 * @svc: pointer to the service which owns this request buffer ptr.
 */
struct zestrpc_request_buffer_desc *
zestrpc_alloc_rqbd (struct zestrpc_service *svc)
{
        struct zestrpc_request_buffer_desc *rqbd;

        ZOBD_ALLOC(rqbd, sizeof (*rqbd));
        if (rqbd == NULL)
                return (NULL);

        rqbd->rqbd_service = svc;
        rqbd->rqbd_refcount = 0;
        rqbd->rqbd_cbid.cbid_fn  = zrequest_in_callback;
        rqbd->rqbd_cbid.cbid_arg = rqbd;
        INIT_ZLIST_HEAD(&rqbd->rqbd_reqs);
        rqbd->rqbd_buffer = zestrpc_alloc_request_buffer(svc->srv_buf_size);

        if (rqbd->rqbd_buffer == NULL) {
                ZOBD_FREE(rqbd, sizeof (*rqbd));
                return (NULL);
        }

        spin_lock(&svc->srv_lock);
        zlist_add(&rqbd->rqbd_list, &svc->srv_idle_rqbds);
        svc->srv_nbufs++;
        spin_unlock(&svc->srv_lock);

        return (rqbd);
}

void
zestrpc_free_rqbd (struct zestrpc_request_buffer_desc *rqbd)
{
        struct zestrpc_service *svc = rqbd->rqbd_service;

        LASSERT (rqbd->rqbd_refcount == 0);
        LASSERT (zlist_empty(&rqbd->rqbd_reqs));

        spin_lock(&svc->srv_lock);
        zlist_del(&rqbd->rqbd_list);
        svc->srv_nbufs--;
        spin_unlock(&svc->srv_lock);

        zestrpc_free_request_buffer (rqbd->rqbd_buffer, svc->srv_buf_size);
        ZOBD_FREE (rqbd, sizeof (*rqbd));
}

/**
 * zestrpc_server_post_idle_rqbds - iterate over the srv_idle_rqbds list and repost buffer desc's found there.  Calls zestrpc_register_rqbd() which does LNetMEAttach and LNetMDAttach.
 * @svc: pointer to the service
 */
int
zestrpc_server_post_idle_rqbds (struct zestrpc_service *svc)
{
        struct zestrpc_request_buffer_desc *rqbd;
        int                                rc;
        int                                posted = 0;

        for (;;) {
                spin_lock(&svc->srv_lock);

                if (zlist_empty (&svc->srv_idle_rqbds)) {
                        spin_unlock(&svc->srv_lock);
                        return (posted);
                }

                rqbd = zlist_entry(zlist_next(&svc->srv_idle_rqbds),
                                  struct zestrpc_request_buffer_desc,
                                  rqbd_list);
                zlist_del (&rqbd->rqbd_list);

                /* assume we will post successfully */
                svc->srv_nrqbd_receiving++;
                zlist_add (&rqbd->rqbd_list, &svc->srv_active_rqbds);

                spin_unlock(&svc->srv_lock);

                rc = zestrpc_register_rqbd(rqbd);
                if (rc != 0)
                        break;

                posted = 1;
        }

        spin_lock(&svc->srv_lock);

        svc->srv_nrqbd_receiving--;
        zlist_del(&rqbd->rqbd_list);
        zlist_add_tail(&rqbd->rqbd_list, &svc->srv_idle_rqbds);

        /* Don't complain if no request buffers are posted right now; LNET
         * won't drop requests because we set the portal lazy! */

        spin_unlock(&svc->srv_lock);

        return (-1);
}


static void __zestrpc_server_free_request(struct zestrpc_request *req)
{
        struct zestrpc_request_buffer_desc *rqbd = req->rq_rqbd;

        zlist_del(&req->rq_list_entry);

        if (req->rq_reply_state != NULL) {
                zestrpc_rs_decref(req->rq_reply_state);
                req->rq_reply_state = NULL;
        }

        if (req != &rqbd->rqbd_req) {
                /* NB request buffers use an embedded
		 * req if the incoming req unlinked the
		 * MD; this isn't one of them! */
                ZOBD_FREE(req, sizeof(*req));
        }
}


static void
zestrpc_server_free_request(struct zestrpc_request *req)
{
        struct zestrpc_request_buffer_desc *rqbd = req->rq_rqbd;
        struct zestrpc_service             *svc = rqbd->rqbd_service;
        int                                refcount;
        struct zlist_head                  *tmp;
        struct zlist_head                  *nxt;

        spin_lock(&svc->srv_lock);

        svc->srv_n_active_reqs--;
        zlist_add(&req->rq_list_entry, &rqbd->rqbd_reqs);

        refcount = --(rqbd->rqbd_refcount);
        if (refcount == 0) {
                /* request buffer is now idle: add to history */
                zlist_del(&rqbd->rqbd_list);
                zlist_add_tail(&rqbd->rqbd_list, &svc->srv_history_rqbds);
                svc->srv_n_history_rqbds++;

                /* cull some history?
		 * I expect only about 1 or 2 rqbds need to be recycled here */
                while (svc->srv_n_history_rqbds > svc->srv_max_history_rqbds) {
                        rqbd = zlist_entry(zlist_next(&svc->srv_history_rqbds),
                                          struct zestrpc_request_buffer_desc,
                                          rqbd_list);

                        zlist_del(&rqbd->rqbd_list);
                        svc->srv_n_history_rqbds--;

                        /* remove rqbd's reqs from svc's req history while
			 * I've got the service lock */
                        zlist_for_each(tmp, &rqbd->rqbd_reqs) {
                                req = zlist_entry(tmp, struct zestrpc_request,
                                                 rq_list_entry);
                                /* Track the highest culled req seq */
                                if (req->rq_history_seq >
                                    svc->srv_request_max_cull_seq)
                                        svc->srv_request_max_cull_seq =
                                                req->rq_history_seq;
                                zlist_del(&req->rq_history_list);
                        }

                        spin_unlock(&svc->srv_lock);


                        zlist_for_each_safe(tmp, nxt, &rqbd->rqbd_reqs) {
                                req = zlist_entry(zlist_next(&rqbd->rqbd_reqs),
                                                 struct zestrpc_request,
                                                 rq_list_entry);
                                __zestrpc_server_free_request(req);
                        }

                        spin_lock(&svc->srv_lock);

                        /* schedule request buffer for re-use.
			 * NB I can only do this after I've disposed of their
			 * reqs; particularly the embedded req */
                        zlist_add_tail(&rqbd->rqbd_list, &svc->srv_idle_rqbds);
                }
        } else if (req->rq_reply_state && req->rq_reply_state->rs_prealloc) {
		/* If we are low on memory, we are not interested in
		   history */
                zlist_del(&req->rq_history_list);
                __zestrpc_server_free_request(req);
        }

        spin_unlock(&svc->srv_lock);

}

static int
zestrpc_server_handle_request(struct zestrpc_service *svc,
                             struct zestion_thread *thread)
{
        struct zestrpc_request *request;
        struct timeval         work_start;
        struct timeval         work_end;
        long                   timediff;
        int                    rc;
        ENTRY;

        LASSERT(svc);

        spin_lock(&svc->srv_lock);
        if (zlist_empty (&svc->srv_request_queue) ||
            (svc->srv_n_difficult_replies != 0 &&
             svc->srv_n_active_reqs >= (svc->srv_nthreads - 1))) {
                /* If all the other threads are handling requests, I must
		 * remain free to handle any 'difficult' reply that might
		 * block them */
                spin_unlock(&svc->srv_lock);
                RETURN(0);
        }

        request = zlist_entry (zlist_next(&svc->srv_request_queue),
                              struct zestrpc_request, rq_list_entry);
        zlist_del(&request->rq_list_entry);
        svc->srv_n_queued_reqs--;
        svc->srv_n_active_reqs++;

        spin_unlock(&svc->srv_lock);

        do_gettimeofday(&work_start);
        timediff = cfs_timeval_sub(&work_start,
				   &request->rq_arrival_time, NULL);

#if WETRACKSTATSSOMEDAY
        if (svc->srv_stats != NULL) {
                lprocfs_counter_add(svc->srv_stats, ZESTRPC_REQWAIT_CNTR,
                                    timediff);
                lprocfs_counter_add(svc->srv_stats, ZESTRPC_REQQDEPTH_CNTR,
                                    svc->srv_n_queued_reqs);
                lprocfs_counter_add(svc->srv_stats, ZESTRPC_REQACTIVE_CNTR,
                                    svc->srv_n_active_reqs);
        }
#endif
#if SWAB_PARANOIA
        /* Clear request swab mask; this is a new request */
        request->rq_req_swab_mask = 0;
#endif
        rc = zest_unpack_msg (request->rq_reqmsg, request->rq_reqlen);
        if (rc != 0) {
                CERROR ("error unpacking request: ptl %d from %s"
                        " xid "LPU64"\n", svc->srv_req_portal,
                        libcfs_id2str(request->rq_peer), request->rq_xid);
                goto out;
        }

        rc = -EINVAL;
        if (request->rq_reqmsg->type != ZEST_RPC_MSG_REQUEST) {
                CERROR("wrong packet type received (type=%u) from %s\n",
                       request->rq_reqmsg->type,
                       libcfs_id2str(request->rq_peer));
                goto out;
        }

        zinfo("got req "LPD64, request->rq_xid);

        request->rq_svc_thread = thread;

	request->rq_conn = zestrpc_get_connection(request->rq_peer,
						  request->rq_self, NULL);
	if (request->rq_conn == NULL) {
                CERROR("null connection struct :(\n");
                return -ENOTCONN;
        }
	/*
	 * Here's a hack to trick lustre rpc into thinking there's a real export
	 */
	if (request->rq_conn->c_exp == NULL) {
		struct zestrpc_export *exp;

		DEBUG_REQ(ZLL_WARN, request, "null export");

		ZOBD_ALLOC(exp, sizeof(*exp));
		if (exp == NULL)
			zfatal("Couldn't allocate export");
		/*
		 * init and associate the connection and export structs
		 *  see zclass_new_export() for more detail
		 */
		LOCK_INIT(&exp->exp_lock);
		atomic_set(&exp->exp_refcount, 2);
		atomic_set(&exp->exp_rpc_count, 0);
		exp->exp_connection = request->rq_conn;
		request->rq_conn->c_exp = exp;
	}
	zclass_export_rpc_get(request->rq_conn->c_exp);
	request->rq_export = request->rq_conn->c_exp;

        /* Discard requests queued for longer than my timeout.  If the
	 * client's timeout is similar to mine, she'll be timing out this
	 * REQ anyway (bug 1502) */
        if (timediff / 1000000 > ZOBD_TIMEOUT) {
                CERROR("Dropping timed-out opc %d request from %s"
                       ": %ld seconds old\n", request->rq_reqmsg->opc,
                       libcfs_id2str(request->rq_peer),
                       timediff / 1000000);
                goto put_rpc_export;
        }

        request->rq_phase = ZRQ_PHASE_INTERPRET;

        zinfo("Handling RPC peer+ref:pid:xid:nid:opc "
	      "%s+%d:%d:"LPU64":%d",
	      libcfs_id2str(request->rq_conn->c_peer),
	      atomic_read(&request->rq_export->exp_refcount),
	      request->rq_reqmsg->status,
	      request->rq_xid,
	      request->rq_reqmsg->opc);

        rc = svc->srv_handler(request);

        request->rq_phase = ZRQ_PHASE_COMPLETE;

        zinfo("Handled RPC peer+ref:pid:xid:nid:opc "
	      "%s+%d:%d:"LPU64":%d",
	      libcfs_id2str(request->rq_conn->c_peer),
	      atomic_read(&request->rq_export->exp_refcount),
	      request->rq_reqmsg->status,
	      request->rq_xid,
	      request->rq_reqmsg->opc);

 put_rpc_export:
	zclass_export_rpc_put(request->rq_export);
	request->rq_export = NULL;

 out:
	if (request->rq_export != NULL)
                zclass_export_put(request->rq_export);

        do_gettimeofday(&work_end);

        timediff = cfs_timeval_sub(&work_end, &work_start, NULL);

        if (timediff / 1000000 > ZOBD_TIMEOUT)
                CERROR("request "LPU64" opc %u from %s processed in %lds "
                       "trans "LPU64" rc %d/%d\n",
                       request->rq_xid, request->rq_reqmsg->opc,
                       libcfs_id2str(request->rq_peer),
                       cfs_timeval_sub(&work_end, &request->rq_arrival_time,
                                       NULL) / 1000000,
                       request->rq_repmsg ? request->rq_repmsg->transno :
                       request->rq_transno, request->rq_status,
                       request->rq_repmsg ?
		          (int)request->rq_repmsg->status : -999);
        else
                CDEBUG(D_NET, "request "LPU64" opc %u from %s processed in "
                       "%ldus (%ldus total) trans "LPU64" rc %d/%d\n",
                       request->rq_xid, request->rq_reqmsg->opc,
                       libcfs_id2str(request->rq_peer), timediff,
                       cfs_timeval_sub(&work_end, &request->rq_arrival_time,
                                       NULL),
                       request->rq_transno, request->rq_status,
                       request->rq_repmsg ?
		          (int)request->rq_repmsg->status : -999);

#if 0 // Zest has no proc counter
        if (svc->srv_stats != NULL) {
                int opc = opcode_offset(request->rq_reqmsg->opc);
                if (opc > 0) {
                        LASSERT(opc < LUSTRE_MAX_OPCODES);
                        lprocfs_counter_add(svc->srv_stats,
                                            opc + ZESTRPC_LAST_CNTR,
                                            timediff);
                }
        }
#endif

        zestrpc_server_free_request(request);

        RETURN(1);
}

static int
zestrpc_server_handle_reply (struct zestrpc_service *svc)
{
	LASSERT(svc != NULL);
#if 0
        struct zestrpc_reply_state *rs;
        struct obd_export         *exp;
        struct obd_device         *obd;
        int                        nlocks;
        int                        been_handled;
        ENTRY;

        spin_lock(&svc->srv_lock);
        if (zlist_empty (&svc->srv_reply_queue)) {
                spin_unlock(&svc->srv_lock);
                RETURN(0);
        }

        rs = zlist_entry (svc->srv_reply_queue.next,
                         struct zestrpc_reply_state, rs_list_entry);

        exp = rs->rs_export;
        obd = exp->exp_obd;

        LASSERT (rs->rs_difficult);
        LASSERT (rs->rs_scheduled);

        zlist_del(&rs->rs_list_entry);

        /* Disengage from notifiers carefully (lock order - irqrestore below!)*/
        spin_unlock(&svc->srv_lock);

        spin_lock (&obd->obd_uncommitted_replies_lock);
        /* Noop if removed already */
        zlist_del_init (&rs->rs_obd_list);
        spin_unlock (&obd->obd_uncommitted_replies_lock);

        spin_lock (&exp->exp_lock);
        /* Noop if removed already */
        zlist_del_init (&rs->rs_exp_list);
        spin_unlock (&exp->exp_lock);

        spin_lock(&svc->srv_lock);

        been_handled = rs->rs_handled;
        rs->rs_handled = 1;

        nlocks = rs->rs_nlocks;                 /* atomic "steal", but */
        rs->rs_nlocks = 0;                      /* locks still on rs_locks! */

        if (nlocks == 0 && !been_handled) {
                /* If we see this, we should already have seen the warning
		 * in mds_steal_ack_locks()  */
                CWARN("All locks stolen from rs %p x"LPD64".t"LPD64
                      " o%d NID %s\n",
                      rs,
                      rs->rs_xid, rs->rs_transno,
                      rs->rs_msg.opc,
                      libcfs_nid2str(exp->exp_connection->c_peer.nid));
        }

        if ((!been_handled && rs->rs_on_net) ||
            nlocks > 0) {
                spin_unlock(&svc->srv_lock);

                if (!been_handled && rs->rs_on_net) {
                        LNetMDUnlink(rs->rs_md_h);
                        /* Ignore return code; we're racing with
			 * completion... */
                }

                while (nlocks-- > 0)
                        ldlm_lock_decref(&rs->rs_locks[nlocks],
                                         rs->rs_modes[nlocks]);

                spin_lock(&svc->srv_lock);
        }

        rs->rs_scheduled = 0;

        if (!rs->rs_on_net) {
                /* Off the net */
                svc->srv_n_difficult_replies--;
                spin_unlock(&svc->srv_lock);

                zclass_export_put (exp);
                rs->rs_export = NULL;
                zestrpc_rs_decref (rs);
                atomic_dec (&svc->srv_outstanding_replies);
                RETURN(1);
        }

        /* still on the net; callback will schedule */
        spin_unlock(&svc->srv_lock);
#endif
        RETURN(1);
}

int
target_send_reply_msg (struct zestrpc_request *req, int rc, int fail_id)
{
#if PAULS_TODO
        if (ZOBD_FAIL_CHECK(fail_id | ZOBD_FAIL_ONCE)) {
                obd_fail_loc |= ZOBD_FAIL_ONCE | ZOBD_FAILED;
                DEBUG_REQ(ZLL_ERROR, req, "dropping reply");
                return (-ECOMM);
        }
#endif
	if (fail_id) {
                DEBUG_REQ(ZLL_ERROR, req, "dropping reply");
                return (-ECOMM);
	}

        if (rc) {
                DEBUG_REQ(ZLL_ERROR, req, "processing error (%d)", rc);
                req->rq_status = rc;
                return (zestrpc_error(req));
        } else {
                DEBUG_REQ(ZLL_INFO, req, "sending reply");
        }

        return (zestrpc_send_reply(req, 1));
}

int
zestrpc_grow_req_bufs(struct zestrpc_service *svc)
{
        struct zestrpc_request_buffer_desc *rqbd;
        int                                i;

        CDEBUG(D_RPCTRACE, "%s: allocate %d new %d-byte reqbufs (%d/%d left)\n",
               svc->srv_name, svc->srv_nbuf_per_group, svc->srv_buf_size,
               svc->srv_nrqbd_receiving, svc->srv_nbufs);
        for (i = 0; i < svc->srv_nbuf_per_group; i++) {
                rqbd = zestrpc_alloc_rqbd(svc);

                if (rqbd == NULL) {
                        CERROR ("%s: Can't allocate request buffer\n",
                                svc->srv_name);
                        return (-ENOMEM);
                }

                if (zestrpc_server_post_idle_rqbds(svc) < 0)
                        return (-EAGAIN);
        }

        return (0);
}

static void
zestrpc_check_rqbd_pool(struct zestrpc_service *svc)
{
        int avail = svc->srv_nrqbd_receiving;
        int low_water = test_req_buffer_pressure ? 0 :
		svc->srv_nbuf_per_group/2;

	//ENTRY;
        /* NB I'm not locking; just looking. */

        /* CAVEAT EMPTOR: We might be allocating buffers here because we've
	 * allowed the request history to grow out of control.  We could put a
	 * sanity check on that here and cull some history if we need the
	 * space. */

        if (avail <= low_water)
                zestrpc_grow_req_bufs(svc);

        //lprocfs_counter_add(svc->srv_stats, ZESTRPC_REQBUF_AVAIL_CNTR, avail);
	//EXIT;
}

static int
zestrpc_retry_rqbds(void *arg)
{
        struct zestrpc_service *svc = (struct zestrpc_service *)arg;

        svc->srv_rqbd_timeout = 0;
        return (-ETIMEDOUT);
}


#define zestrpc_main zestrpc_main
static void * zestrpc_main(void *arg)
{
        struct zestion_thread     *thread = arg;
        struct zestrpc_service    *svc;
        struct zestrpc_reply_state *rs;
	int *run;
        //struct lc_watchdog        *watchdog;

        int rc = 0;
        ENTRY;

	zdbg("thread %p zthr_type is %d", thread,
	     thread->zthr_type);
	switch (thread->zthr_type) {
	case ZTHRT_RPCMDS:
		svc = thread->zthr_rpcmdsthr.zrm_svc;
		break;
	case ZTHRT_RPCIO:
		svc = thread->zthr_rpciothr.zri_svc;
		break;
	default:
		zfatal("unknown thread type; type=%d", thread->zthr_type);
	}

	zest_assert(svc != NULL);

        if (svc->srv_init != NULL) {
                rc = svc->srv_init(thread);
                if (rc)
                        goto out;
        }
	/* Alloc reply state structure for this one */
	ZOBD_ALLOC(rs, svc->srv_max_reply_size);
	INIT_ZLIST_ENTRY(&rs->rs_list_entry);

	spin_lock(&svc->srv_lock);
	svc->srv_nthreads++;
	zlist_add(&rs->rs_list_entry, &svc->srv_free_rs_list);
	spin_unlock(&svc->srv_lock);
	wake_up(&svc->srv_free_rs_waitq);

	CDEBUG(D_NET, "service thread %zu started\n", thread->zthr_id);

	run = (svc == zestRpcMdsSvc ? &thread->zthr_rpcmdsthr.zrm_run :
	    &thread->zthr_rpciothr.zri_run);

	/* XXX maintain a list of all managed devices: insert here */
	while (*run  ||
	       svc->srv_n_difficult_replies != 0) {

		/* Don't exit while there are replies to be handled */
		struct l_wait_info lwi = LWI_TIMEOUT(svc->srv_rqbd_timeout,
						     zestrpc_retry_rqbds, svc);

		//lc_watchdog_disable(watchdog);
		//l_wait_event_exclusive (svc->srv_waitq,
		/*
		zdbg("*run %d, svc->srv_n_difficult_replies %d, "
		       "zlist_empty(&svc->srv_idle_rqbds) %d,  svc->srv_rqbd_timeout %d "
		       "zlist_empty (&svc->srv_reply_queue) %d, zlist_empty(&svc->srv_request_queue) %d "
		       "svc->srv_n_active_reqs %d svc->srv_nthreads %d"
		       "COND 1=%d, COND 2=%d, COND 3=%d",

		       *run, svc->srv_n_difficult_replies,
		       zlist_empty(&svc->srv_idle_rqbds), svc->srv_rqbd_timeout,
		       zlist_empty (&svc->srv_reply_queue), zlist_empty(&svc->srv_request_queue),
		       svc->srv_n_active_reqs, svc->srv_nthreads,
		       (*run != 0 && svc->srv_n_difficult_replies == 0),
		       (!zlist_empty(&svc->srv_idle_rqbds) && svc->srv_rqbd_timeout == 0),
		       (!zlist_empty (&svc->srv_request_queue) &&
			(svc->srv_n_difficult_replies == 0 ||
			 svc->srv_n_active_reqs < (svc->srv_nthreads - 1)))
		       );
		*/
		zsvr_wait_event(&svc->srv_waitq,
				(!*run &&
				 svc->srv_n_difficult_replies == 0) ||
				(!zlist_empty(&svc->srv_idle_rqbds) &&
				 svc->srv_rqbd_timeout == 0) ||
				!zlist_empty (&svc->srv_reply_queue) ||
				(!zlist_empty (&svc->srv_request_queue) &&
				 (svc->srv_n_difficult_replies == 0 ||
				  svc->srv_n_active_reqs <
				  (svc->srv_nthreads - 1))),
				&lwi, NULL);
		//				&lwi, &svc->srv_lock);

		//lc_watchdog_touch(watchdog);

		zestrpc_check_rqbd_pool(svc);

		/*
		 * this has to be mod'ed to support the io threads
		 *  put'ing replies onto this list after they've sync'ed
		 *  the blocks to disk.. paul
		 */
		//if (!zlist_empty (&svc->srv_reply_queue))
		//	zestrpc_server_handle_reply (svc);

		/* only handle requests if there are no difficult replies
		 * outstanding, or I'm not the last thread handling
		 * requests */
		if (!zlist_empty (&svc->srv_request_queue) &&
		    (svc->srv_n_difficult_replies == 0 ||
		     svc->srv_n_active_reqs < (svc->srv_nthreads - 1)))
			zestrpc_server_handle_request(svc, thread);

		if (!zlist_empty(&svc->srv_idle_rqbds) &&
		    zestrpc_server_post_idle_rqbds(svc) < 0) {
			/* I just failed to repost request buffers.  Wait
			 * for a timeout (unless something else happens)
			 * before I try again */
			svc->srv_rqbd_timeout = HZ/10;
			CDEBUG(D_RPCTRACE,"Posted buffers: %d\n",
			       svc->srv_nrqbd_receiving);
		}
	}

	//lc_watchdog_delete(watchdog);

	// out_srv_init:
	/*
	 * deconstruct service specific state created by zestrpc_start_thread()
	 */
	if (svc->srv_done != NULL)
		svc->srv_done(thread);

 out:
	CDEBUG(D_NET, "service thread %zu exiting: rc %d\n", thread->zthr_id, rc);

	spin_lock(&svc->srv_lock);
	svc->srv_nthreads--;                    /* must know immediately */
#if 0 //ptlrpc
	thread->t_id = rc;
	thread->t_flags = SVC_STOPPED;

        wake_up(&thread->t_ctl_waitq);
#endif
        spin_unlock(&svc->srv_lock);

	thread->zthr_rc = rc;
	return NULL;
}

#if 0
static void zestrpc_stop_thread(struct zestrpc_service *svc,
                               struct zestrpc_thread *thread)
{
        struct l_wait_info lwi = { 0 };

        spin_lock(&svc->srv_lock);
        thread->t_flags = SVC_STOPPING;
        spin_unlock(&svc->srv_lock);

        wake_up_all(&svc->srv_waitq);
        l_wait_event(thread->t_ctl_waitq, (thread->t_flags & SVC_STOPPED),
                     &lwi);

        spin_lock(&svc->srv_lock);
        zlist_del(&thread->t_link);
        spin_unlock(&svc->srv_lock);

        ZOBD_FREE(thread, sizeof(*thread));
}

void zestrpc_stop_all_threads(struct zestrpc_service *svc)
{
        struct zestrpc_thread *thread;

        spin_lock(&svc->srv_lock);
        while (!zlist_empty(&svc->srv_threads)) {
                thread = zlist_entry(svc->srv_threads.next,
                                    struct zestrpc_thread, t_link);

                spin_unlock(&svc->srv_lock);
                zestrpc_stop_thread(svc, thread);
                spin_lock(&svc->srv_lock);
        }

        spin_unlock(&svc->srv_lock);
}
#endif

int zestrpc_unregister_service(struct zestrpc_service *service)
{
        int                   rc;
        struct l_wait_info    lwi;
        struct zlist_head     *tmp;
        struct zestrpc_reply_state *rs, *t;

        //zestrpc_stop_all_threads(service);
        LASSERT(zlist_empty(&service->srv_threads));

        spin_lock (&zestrpc_all_services_lock);
        zlist_del(&service->srv_list_entry);
        spin_unlock (&zestrpc_all_services_lock);

        /* All history will be culled when the next request buffer is
	 * freed */
        service->srv_max_history_rqbds = 0;

        CDEBUG(D_NET, "%s: tearing down\n", service->srv_name);

        rc = LNetClearLazyPortal(service->srv_req_portal);
        LASSERT (rc == 0);

        /* Unlink all the request buffers.  This forces a 'final' event with
	 * its 'unlink' flag set for each posted rqbd */
        zlist_for_each(tmp, &service->srv_active_rqbds) {
                struct zestrpc_request_buffer_desc *rqbd =
                        zlist_entry(tmp, struct zestrpc_request_buffer_desc,
                                   rqbd_list);

                rc = LNetMDUnlink(rqbd->rqbd_md_h);
                LASSERT (rc == 0 || rc == -ENOENT);
        }

        /* Wait for the network to release any buffers it's currently
         * filling */
        for (;;) {
                spin_lock(&service->srv_lock);
                rc = service->srv_nrqbd_receiving;
                spin_unlock(&service->srv_lock);

                if (rc == 0)
                        break;

                /* Network access will complete in finite time but the HUGE
		 * timeout lets us CWARN for visibility of sluggish NALs */
                lwi = LWI_TIMEOUT(300 * HZ, NULL, NULL);
                rc = zsvr_wait_event(&service->srv_waitq,
				     service->srv_nrqbd_receiving == 0,
				     &lwi, &service->srv_lock);
                if (rc == -ETIMEDOUT)
                        CWARN("Service %s waiting for request buffers\n",
                              service->srv_name);
        }

        /* schedule all outstanding replies to terminate them */
        spin_lock(&service->srv_lock);
        while (!zlist_empty(&service->srv_active_replies)) {
                rs = zlist_entry(zlist_next(&service->srv_active_replies),
                                   struct zestrpc_reply_state, rs_list_entry);
		CWARN("Active reply found?? %p", rs);
                //zestrpc_schedule_difficult_reply(rs);
        }
        spin_unlock(&service->srv_lock);

        /* purge the request queue.  NB No new replies (rqbds all unlinked)
	 * and no service threads, so I'm the only thread noodling the
	 * request queue now */
        while (!zlist_empty(&service->srv_request_queue)) {
                struct zestrpc_request *req =
                        zlist_entry(zlist_next(&service->srv_request_queue),
                                   struct zestrpc_request,
                                   rq_list_entry);

                zlist_del(&req->rq_list_entry);
                service->srv_n_queued_reqs--;
                service->srv_n_active_reqs++;

                zestrpc_server_free_request(req);
        }
        LASSERT(service->srv_n_queued_reqs == 0);
        LASSERT(service->srv_n_active_reqs == 0);
        LASSERT(service->srv_n_history_rqbds == 0);
        LASSERT(zlist_empty(&service->srv_active_rqbds));

        /* Now free all the request buffers since nothing references them
	 * any more... */
        while (!zlist_empty(&service->srv_idle_rqbds)) {
                struct zestrpc_request_buffer_desc *rqbd =
                        zlist_entry(zlist_next(&service->srv_idle_rqbds),
                                   struct zestrpc_request_buffer_desc,
                                   rqbd_list);

                zestrpc_free_rqbd(rqbd);
        }

        /* wait for all outstanding replies to complete (they were
	 * scheduled having been flagged to abort above) */
        while (atomic_read(&service->srv_outstanding_replies) != 0) {
                lwi = LWI_TIMEOUT(10 * HZ, NULL, NULL);

                rc = zsvr_wait_event(&service->srv_waitq,
                                  !zlist_empty(&service->srv_reply_queue),
				  &lwi,
				  &service->srv_lock);

                LASSERT(rc == 0 || rc == -ETIMEDOUT);

                if (rc == 0) {
                        zestrpc_server_handle_reply(service);
                        continue;
                }
                CWARN("Unexpectedly long timeout %p\n", service);
        }

        //list_for_each_entry_safe(rs, t, &service->srv_free_rs_list,
        zlist_for_each_entry_safe(rs, t, &service->srv_free_rs_list,
				 rs_list_entry) {
                zlist_del(&rs->rs_list_entry);
                ZOBD_FREE(rs, service->srv_max_reply_size);
        }

        ZOBD_FREE(service, sizeof(*service));
        return 0;
}


struct zestrpc_service *
zestrpc_init_svc(int nbufs, int bufsize, int max_req_size, int max_reply_size,
		 int req_portal, int rep_portal, char *name, int num_threads,
		 svc_handler_t handler)
{
        int                    rc;
        struct zestrpc_service *service;
        ENTRY;

	zinfo("bufsize %d  max_req_size %d", bufsize, max_req_size);

        LASSERT (nbufs > 0);
        LASSERT (bufsize >= max_req_size);

        ZOBD_ALLOC(service, sizeof(*service));
        if (service == NULL)
                RETURN(NULL);

        /* First initialise enough for early teardown */

        service->srv_name = name;
        spin_lock_init(&service->srv_lock);
        INIT_ZLIST_HEAD(&service->srv_threads);
        init_waitqueue_head(&service->srv_waitq);

        service->srv_nbuf_per_group = test_req_buffer_pressure ? 1 : nbufs;
        service->srv_max_req_size = max_req_size;
        service->srv_buf_size = bufsize;
        service->srv_rep_portal = rep_portal;
        service->srv_req_portal = req_portal;
        //service->srv_watchdog_timeout = watchdog_timeout;
        service->srv_handler = handler;
        //service->srv_request_history_print_fn = svcreq_printfn;
        service->srv_request_seq = 1;           /* valid seq #s start at 1 */
        service->srv_request_max_cull_seq = 0;
        service->srv_num_threads = num_threads;

        rc = LNetSetLazyPortal(service->srv_req_portal);
        LASSERT (rc == 0);

	INIT_ZLIST_ENTRY(&service->srv_list_entry);
	INIT_ZLIST_HEAD(&service->srv_request_queue);
	INIT_ZLIST_HEAD(&service->srv_request_history);

	INIT_ZLIST_HEAD(&service->srv_idle_rqbds);
	INIT_ZLIST_HEAD(&service->srv_active_rqbds);
	INIT_ZLIST_HEAD(&service->srv_history_rqbds);

	//ATOMIC_INIT(&service->srv_outstanding_replies);
	INIT_ZLIST_HEAD(&service->srv_active_replies);
	INIT_ZLIST_HEAD(&service->srv_reply_queue);

	INIT_ZLIST_HEAD(&service->srv_free_rs_list);

	zwaitq_init(&service->srv_free_rs_waitq);
	zwaitq_init(&service->srv_waitq);

	INIT_ZLIST_HEAD(&service->srv_threads);

	spin_lock_init(&service->srv_lock);

	service->srv_name = name;

        spin_lock (&zestrpc_all_services_lock);
        zlist_add (&service->srv_list_entry, &zestrpc_all_services);
        spin_unlock (&zestrpc_all_services_lock);

        /* Now allocate the request buffers */
        rc = zestrpc_grow_req_bufs(service);
        /* We shouldn't be under memory pressure at startup, so
         * fail if we can't post all our buffers at this time. */
        if (rc != 0)
                GOTO(failed, NULL);

        /* Now allocate pool of reply buffers */
        /* Increase max reply size to next power of two */
        service->srv_max_reply_size = 1;
        while (service->srv_max_reply_size < max_reply_size)
                service->srv_max_reply_size <<= 1;


        CDEBUG(D_NET, "%s: Started, zlistening on portal %d\n",
               service->srv_name, service->srv_req_portal);

        RETURN(service);
 failed:
        zestrpc_unregister_service(service);
        return NULL;
}


#define ZRPCMDSSVCNAME "zrpc_mds"
#define ZRPCIOSVCNAME  "zrpc_io"

void
zrpcthr_spawn(int type)
{
	struct zestion_thread **zthreads, *zthr;
	struct zestion_rpciothr *zri;
	struct zestrpc_service *zsvc;
	int    i, nthreads;

	zdbg("type=%d", type);

	zest_assert(type == ZTHRT_RPCIO ||
		    type == ZTHRT_RPCMDS);

	if (type == ZTHRT_RPCIO) {
		zthreads = &zestionRPCIOThreads;
		nthreads = NUM_RPCIO_THREADS;
		zestRpcIoSvc = zestrpc_init_svc(RPCIO_NBUFS,
						RPCIO_BUFSIZE,
						RPCIO_MAXREQSIZE,
						RPCIO_MAXREPSIZE,
						RPCIO_REQUEST_PORTAL,
						RPCIO_REPLY_PORTAL,
						RPCIO_SVCNAME,
						nthreads,
						zrpc_io_handler);
		zest_assert(zestRpcIoSvc);
		zsvc = zestRpcIoSvc;
	} else {
		zthreads = &zestionRPCMDSThreads;
		nthreads = NUM_RPCMDS_THREADS;
		zestRpcMdsSvc = zestrpc_init_svc(RPCMDS_NBUFS,
						 RPCMDS_BUFSIZE,
						 RPCMDS_MAXREQSIZE,
						 RPCMDS_MAXREPSIZE,
						 RPCMDS_REQUEST_PORTAL,
						 RPCMDS_REPLY_PORTAL,
						 RPCMDS_SVCNAME,
						 nthreads,
						 zrpc_mds_handler);
		zest_assert(zestRpcMdsSvc);
		zsvc = zestRpcMdsSvc;
	}

	*zthreads = ZALLOC(sizeof(**zthreads) * nthreads);

	for (i=0, zthr = *zthreads; i < nthreads; i++, zthr++) {
		switch (type) {
		case ZTHRT_RPCMDS:
			zthr->zthr_rpcmdsthr.zrm_svc = zsvc;
			zthr->zthr_rpcmdsthr.zrm_run = 1;
			break;
		case ZTHRT_RPCIO:
			zri = &zthr->zthr_rpciothr;
			zri->zri_svc = zsvc;
			zri->zri_run = 1;

			iostats_init(&zri->zri_st_netperf,
			    "rpcio-%02d", i);
			break;
		}

		zthr_init(zthr, type, zestrpc_main, i);
	}
}
