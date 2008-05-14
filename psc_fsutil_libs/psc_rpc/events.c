/* $Id$ */

#define PSC_SUBSYS PSS_RPC

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/waitq.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"
#include "psc_types.h"

lnet_handle_eq_t pscrpc_eq_h;
struct psclist_head pscrpc_wait_callbacks;

/*
 *  Client's outgoing request callback
 */
void request_out_callback(lnet_event_t *ev)
{
        struct pscrpc_cb_id   *cbid = ev->md.user_ptr;
        struct pscrpc_request *req = cbid->cbid_arg;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_SEND ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT (ev->unlinked);

        DEBUG_REQ((ev->status == 0) ? PLL_INFO : PLL_ERROR, req,
                  "type %d, status %d", ev->type, ev->status);

        if (ev->type == LNET_EVENT_UNLINK || ev->status != 0) {

                /* Failed send: make it seem like the reply timed out, just
		 * like failing sends in client.c does currently...  */
                spinlock(&req->rq_lock);
                req->rq_net_err = 1;
                freelock(&req->rq_lock);

                pscrpc_wake_client_req(req);
        }

        /* these balance the references in ptl_send_rpc() */
        atomic_dec(&req->rq_import->imp_inflight);
        pscrpc_req_finished(req);

        EXIT;
}

/*
 * Server's incoming request callback
 */
void request_in_callback(lnet_event_t *ev)
{
        struct pscrpc_cb_id               *cbid = ev->md.user_ptr;
        struct pscrpc_request_buffer_desc *rqbd = cbid->cbid_arg;
        struct pscrpc_service             *service = rqbd->rqbd_service;
        struct pscrpc_request             *req;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_PUT ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT ((char *)ev->md.start >= rqbd->rqbd_buffer);
        LASSERT ((char *)ev->md.start + ev->offset + ev->mlength <=
                 rqbd->rqbd_buffer + service->srv_buf_size);

	if (!ev->status)
		psc_trace("event type %d, status %d, service %s",
		       ev->type, ev->status, service->srv_name);
	else
		psc_errorx("event type %d, status %d, service %s",
			ev->type, ev->status, service->srv_name);

        if (ev->unlinked) {
                /* If this is the last request message to fit in the
		 * request buffer we can use the request object embedded in
		 * rqbd.  Note that if we failed to allocate a request,
		 * we'd have to re-post the rqbd, which we can't do in this
		 * context. */
                req = &rqbd->rqbd_req;
                memset(req, 0, sizeof (*req));
        } else {
                LASSERT (ev->type == LNET_EVENT_PUT);
                if (ev->status != 0) {
                        /* We moaned above already... */
			EXIT;
                        return;
                }
#if 0
                ZOBD_ALLOC_GFP(req, sizeof(*req), GFP_ATOMIC);
#endif
		req = TRY_PSCALLOC(sizeof(*req));
                if (req == NULL) {
                        CERROR("Can't allocate incoming request descriptor: "
			       "Dropping %s RPC from %s\n",
                               service->srv_name,
                               libcfs_id2str(ev->initiator));
			EXIT;
                        return;
                }
        }

        /* NB we ABSOLUTELY RELY on req being zeroed, so pointers are NULL,
         * flags are reset and scalars are zero.  We only set the message
         * size to non-zero if this was a successful receive. */
        req->rq_xid = ev->match_bits;
        req->rq_reqmsg = ev->md.start + ev->offset;
        if (ev->type == LNET_EVENT_PUT && ev->status == 0)
                req->rq_reqlen = ev->mlength;
        do_gettimeofday(&req->rq_arrival_time);
        req->rq_peer = ev->initiator;
        req->rq_self = ev->target.nid;
        req->rq_rqbd = rqbd;
        req->rq_phase = ZRQ_PHASE_NEW;
#ifdef CRAY_XT3
        //req->rq_uid = ev->uid;
#endif

        spinlock(&service->srv_lock);

        req->rq_history_seq = service->srv_request_seq++;
        psclist_xadd_tail(&req->rq_history_list, &service->srv_request_history);

        if (ev->unlinked) {
                service->srv_nrqbd_receiving--;
		//               CDEBUG(D_RPCTRACE,"Buffer complete: %d buffers still posted\n",
                CERROR("Buffer complete: %d buffers still posted (%s)\n",
                       service->srv_nrqbd_receiving, service->srv_name);

		if (!service->srv_nrqbd_receiving)
			CERROR("Service %s, all request buffer are busy",
			      service->srv_name);
#if 0
                /* Normally, don't complain about 0 buffers posted; LNET won't
		 * drop incoming reqs since we set the portal lazy */
                if (test_req_buffer_pressure &&
                    ev->type != LNET_EVENT_UNLINK &&
                    service->srv_nrqbd_receiving == 0)
                        CWARN("All %s request buffers busy\n",
                              service->srv_name);
#endif
                /* req takes over the network's ref on rqbd */
        } else {
                /* req takes a ref on rqbd */
                rqbd->rqbd_refcount++;
        }

        psclist_xadd_tail(&req->rq_list_entry, &service->srv_request_queue);
        service->srv_n_queued_reqs++;

        /* NB everything can disappear under us once the request
	 * has been queued and we unlock, so do the wake now... */
	psc_waitq_wakeup(&service->srv_waitq);
        //wake_up(&service->srv_waitq);

        freelock(&service->srv_lock);
        EXIT;
}

/*
 * Client's bulk has been written/read
 */
void client_bulk_callback (lnet_event_t *ev)
{
        struct pscrpc_cb_id     *cbid = ev->md.user_ptr;
        struct pscrpc_bulk_desc *desc = cbid->cbid_arg;
        ENTRY;

        LASSERT ((desc->bd_type == BULK_PUT_SINK &&
                  ev->type == LNET_EVENT_PUT) ||
                 (desc->bd_type == BULK_GET_SOURCE &&
                  ev->type == LNET_EVENT_GET) ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT (ev->unlinked);

        CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
               "event type %d, status %d, desc %p\n",
               ev->type, ev->status, desc);

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
        EXIT;
}


void reply_in_callback(lnet_event_t *ev)
{
        struct pscrpc_cb_id   *cbid = ev->md.user_ptr;
        struct pscrpc_request *req = cbid->cbid_arg;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_PUT ||
                 ev->type == LNET_EVENT_UNLINK);
        LASSERT (ev->unlinked);
        LASSERT (ev->md.start == req->rq_repmsg);
        LASSERT (ev->offset == 0);
        LASSERT (ev->mlength <= (u32)req->rq_replen);

        DEBUG_REQ((ev->status == 0) ? PLL_INFO : PLL_ERROR, req,
                  "type %d, status %d initiator ;%s;",
		  ev->type, ev->status, libcfs_id2str(ev->initiator));

	if (!req->rq_peer.nid)
		req->rq_peer = ev->initiator;

        spinlock(&req->rq_lock);

        LASSERT (req->rq_receiving_reply);
        req->rq_receiving_reply = 0;

        if (ev->type == LNET_EVENT_PUT && ev->status == 0) {
                req->rq_replied = 1;
                req->rq_nob_received = ev->mlength;
        }

        /* NB don't unlock till after wakeup; req can disappear under us
         * since we don't have our own ref */
        pscrpc_wake_client_req(req);

        freelock(&req->rq_lock);
        EXIT;
}

/*
 *  Server's outgoing reply callback
 */
void reply_out_callback(lnet_event_t *ev)
{
        struct pscrpc_cb_id       *cbid = ev->md.user_ptr;
        struct pscrpc_reply_state *rs = cbid->cbid_arg;
        struct pscrpc_service     *svc = rs->rs_service;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_SEND ||
                 ev->type == LNET_EVENT_ACK ||
                 ev->type == LNET_EVENT_UNLINK);

        if (!rs->rs_difficult) {
                /* 'Easy' replies have no further processing so I drop the
		 * net's ref on 'rs' */
                LASSERT (ev->unlinked);
                pscrpc_rs_decref(rs);
                atomic_dec (&svc->srv_outstanding_replies);
		EXIT;
		return;
        }

        LASSERT (rs->rs_on_net);

        if (ev->unlinked) {
                /* Last network callback.  The net's ref on 'rs' stays put
		 * until pscrpc_server_handle_reply() is done with it */
                spinlock(&svc->srv_lock);
                rs->rs_on_net = 0;
#if 0
		// not sure if we're going to need these
		//  pauln - 05082007
                pscrpc_schedule_difficult_reply (rs);
#endif
                freelock(&svc->srv_lock);
        }

        EXIT;
}

/*
 * Server's bulk completion callback
 */
void server_bulk_callback (lnet_event_t *ev)
{
        struct pscrpc_cb_id     *cbid = ev->md.user_ptr;
        struct pscrpc_bulk_desc *desc = cbid->cbid_arg;
        ENTRY;

        LASSERT (ev->type == LNET_EVENT_SEND ||
                 ev->type == LNET_EVENT_UNLINK ||
                 (desc->bd_type == BULK_PUT_SOURCE &&
                  ev->type == LNET_EVENT_ACK) ||
                 (desc->bd_type == BULK_GET_SINK &&
                  ev->type == LNET_EVENT_REPLY));

        CDEBUG((ev->status == 0) ? D_NET : D_ERROR,
               "event type %d, status %d, desc %p\n",
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
                psc_waitq_wakeup(&desc->bd_waitq);
        }

        freelock(&desc->bd_lock);
        EXIT;
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
static void pscrpc_master_callback(lnet_event_t *ev)
{
        struct pscrpc_cb_id *cbid = ev->md.user_ptr;
        void (*callback)(lnet_event_t *ev) = cbid->cbid_fn;

        /* Honestly, it's best to find out early. */
        LASSERT (cbid->cbid_arg != LP_POISON);
        LASSERT (callback == request_out_callback ||
                 callback == reply_in_callback    ||
                 callback == client_bulk_callback ||
                 callback == request_in_callback  ||
                 callback == reply_out_callback   ||
                 callback == server_bulk_callback);

        callback (ev);
}

#if 0
/**
 * pscrpc_register_wait_callback - put a client's callback handler onto the main callback list.
 *
 * NOTES: Client context only
 */
void *
pscrpc_register_wait_callback (int (*fn)(void *arg), void *arg)
{
        struct pscrpc_wait_callback *llwc = NULL;

        llwc = PSCALLOC(sizeof(*llwc));

        llwc->llwc_fn = fn;
        llwc->llwc_arg = arg;
        psclist_xadd_tail(&llwc->llwc_list, &pscrpc_wait_callbacks);

        return (llwc);
}

/**
 * pscrpc_deregister_wait_callback - remove a client calback handler
 *
 * NOTES: Client context only
 */
void
pscrpc_deregister_wait_callback (void *opaque)
{
        struct pscrpc_wait_callback *llwc = opaque;

        psclist_del(&llwc->llwc_list);
        ZOBD_FREE(llwc, sizeof(*llwc));
}
#endif

/**
 * pscrpc_check_events - check event queue by calling LNetEQPoll, if an event is found call pscrpc_master_callback().  This call implies single threaded operation.
 * @timeout: number of seconds to block (0 is forever i think)
 * NOTES: Client context only
 */
int
pscrpc_check_events (int timeout)
{
        lnet_event_t ev;
        int         rc;
        int         i;
        ENTRY;

	psc_trace("timeo_ms %d", timeout * 1000);
        rc = LNetEQPoll(&pscrpc_eq_h, 1, timeout * 1000, &ev, &i);
        if (rc == 0)
                RETURN(0);

        LASSERT (rc == -EOVERFLOW || rc == 1);

        /* liblustre: no asynch callback so we can't affort to miss any
         * events... */
        if (rc == -EOVERFLOW) {
                CERROR ("Dropped an event!!!\n");
                abort();
        }

        pscrpc_master_callback (&ev);
        RETURN(1);
}

/**
 * pscrpc_wait_event - called from the macro psc_cli_wait_event() (pscRpc.h), calls pscrpc_check_events().
 * See psc_cli_wait_event for more detail.
 * @timeout: number of seconds to block (0 is forever i think)
 * NOTES: Client context only
 */
int
pscrpc_wait_event (int timeout)
{
#if CLIENT_IS_A_SERVER_TOO
        struct psclist_head             *tmp;
        struct pscrpc_wait_callback *llwc;
	extern struct psclist_head       pscrpc_wait_callbacks;
#endif
        int                           found_something = 0;

        /* single threaded recursion check... */
        //liblustre_waiting = 1;

        for (;;) {
                /* Deal with all pending events */
                while (pscrpc_check_events(0))
                        found_something = 1;

#if CLIENT_IS_A_SERVER_TOO
                /* Give all registered callbacks a bite at the cherry */
                psclist_for_each(tmp, &pscrpc_wait_callbacks) {
                        llwc = psclist_entry(tmp, struct pscrpc_wait_callback,
                                          llwc_list);

                        if (llwc->llwc_fn(llwc->llwc_arg))
                                found_something = 1;
                }
#endif

                if (found_something || timeout == 0)
                        break;

                /* Nothing so far, but I'm allowed to block... */
                found_something = pscrpc_check_events(timeout);
                if (!found_something){           /* still nothing */
			psc_warnx("pscrpc_check_events returned NOTHING.  recommend breaking here!!!");
			return -ETIMEDOUT;
                        break;                  /* I timed out */
		}
        }

        //liblustre_waiting = 0;

        return found_something;
}

lnet_pid_t psc_get_pid(void)
{
        lnet_pid_t        pid;

#ifndef  __KERNEL__
        pid = getpid();
#else
        pid = LUSTRE_SRV_LNET_PID;
#endif
        return pid;
}


int pscrpc_ni_init(int type)
{
        int               rc;
	lnet_process_id_t my_id;

	if ((rc = LNetInit()))
                psc_fatalx("failed to initialize LNET (%d)", rc);

        /* CAVEAT EMPTOR: how we process portals events is _radically_
	 * different depending on... */
	if (type == PSC_SERVER) {
		/* kernel portals calls our master callback when events are added to
		 * the event queue.  In fact lustre never pulls events off this queue,
		 * so it's only sized for some debug history. */
		CERROR("Requesting PID %u\n", PSC_SVR_PID);
		if ((rc = LNetNIInit(PSC_SVR_PID)))
			psc_fatalx("failed LNetNIInit() (%d)", rc);

		rc = LNetEQAlloc(1024, pscrpc_master_callback, &pscrpc_eq_h);
		CERROR("%#"_P_LU64"x pscrpc_eq_h cookie value", pscrpc_eq_h.cookie);
	} else {
		/* liblustre calls the master callback when it removes events from the
		 * event queue.  The event queue has to be big enough not to drop
		 * anything */
		if ((rc = LNetNIInit(psc_get_pid())))
			psc_fatalx("failed LNetNIInit() (%d)", rc);

		rc = LNetEQAlloc(10240, LNET_EQ_HANDLER_NONE, &pscrpc_eq_h);
	}

	psc_assert(rc == 0);

	if (LNetGetId(1, &my_id))
		psc_fatalx("LNetGetId() failed");

        psc_notify("nidpid is (0x%#"_P_LU64"x,0x%x)", my_id.nid, my_id.pid);

        if (rc == 0)
                return 0;

        CERROR ("Failed to allocate event queue: %d\n", rc);
        LNetNIFini();

        return (-ENOMEM);
}


void pscrpc_ni_fini(void)
{
        wait_queue_head_t   waitq;
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
                                CWARN("Event queue still busy\n");

                        /* Wait for a bit */
                        init_waitqueue_head(&waitq);
                        lwi = LWI_TIMEOUT(2, NULL, NULL);
                        psc_svr_wait_event(&waitq, 0, &lwi, NULL);
                        break;
                }
        }
        /* notreached */
}


int pscrpc_init_portals(int type)
{
        int    rc = pscrpc_ni_init(type);

	//libcfs_debug_init(1048576);

        if (rc != 0) {
                CERROR("network initialisation failed\n");
                return -EIO;
        }
#if 0 //ndef __KERNEL__
        liblustre_services_callback =
                liblustre_register_wait_callback(&liblustre_check_services, NULL);
#endif
        return 0;
}

void pscrpc_exit_portals(void)
{
#if 0 //ndef __KERNEL__
        liblustre_deregister_wait_callback(liblustre_services_callback);
#endif
        pscrpc_ni_fini();
}
