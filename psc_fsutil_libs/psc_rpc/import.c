/* $Id$ */

#define PSC_SUBSYS PSS_RPC

#include "psc_rpc/rpc.h"

static inline char *
pscrpc_import_state_name(enum psc_imp_state state)
{
        static char* import_state_names[] = {
                "<UNKNOWN>", "CLOSED",  "NEW", "DISCONN",
                "CONNECTING", "REPLAY", "REPLAY_LOCKS", "REPLAY_WAIT",
                "RECOVER", "FULL", "EVICTED",
        };

        LASSERT (state <= PSC_IMP_EVICTED);
        return import_state_names[state];
}

/* A CLOSED import should remain so. */
#define PSCIMPORT_SET_STATE_NOLOCK(imp, state)				\
	do {								\
		if (imp->imp_state != PSC_IMP_CLOSED) {		\
			psc_warnx("%p %s: changing import state from %s to %s", \
			       imp, libcfs_id2str(imp->imp_connection->c_peer), \
			       pscrpc_import_state_name(imp->imp_state), \
			       pscrpc_import_state_name(state));	\
			imp->imp_state = state;				\
		}							\
	} while(0)


int pscrpc_init_import(struct pscrpc_import *imp)
{
        spin_lock(&imp->imp_lock);

        imp->imp_generation++;
        imp->imp_state =  PSC_IMP_NEW;

        spin_unlock(&imp->imp_lock);

        return 0;
}

/* Returns true if import was FULL, false if import was already not
 * connected.
 * @imp - import to be disconnected
 * @conn_cnt - connection count (epoch) of the request that timed out
 *             and caused the disconnection.  In some cases, multiple
 *             inflight requests can fail to a single target (e.g. OST
 *             bulk requests) and if one has already caused a reconnection
 *             (increasing the import->conn_cnt) the older failure should
 *             not also cause a reconnection.  If zero it forces a reconnect.
 */
int pscrpc_set_import_discon(struct pscrpc_import *imp, u32 conn_cnt)
{
        int rc = 0;

        spin_lock(&imp->imp_lock);

	psc_warnx("inhere conn_cnt %u imp_conn_cnt %u, imp->imp_state = %d",
		  conn_cnt, imp->imp_conn_cnt, imp->imp_state);

        if (imp->imp_state == PSC_IMP_FULL &&
            (conn_cnt == 0 || conn_cnt == (u32)imp->imp_conn_cnt)) {

		psc_errorx("Connection to service via nid %s was "
			   "lost; in progress operations using this "
			   "service will %s.",
			   libcfs_nid2str(imp->imp_connection->c_peer.nid),
			   imp->imp_replayable ?
			   "wait for recovery to complete" : "fail");

                PSCIMPORT_SET_STATE_NOLOCK(imp, PSC_IMP_DISCON);
                spin_unlock(&imp->imp_lock);

                //if (obd_dump_on_timeout)
                //        libcfs_debug_dumplog();

#if PAULS_TODO
		/* inform the client to take action in response to the event */
                pscobd_import_event(imp->imp_obd, imp, IMP_EVENT_DISCON);
#endif
                rc = 1;
        } else {
                spin_unlock(&imp->imp_lock);
                psc_warnx("%s: import %p already %s (conn %u, was %u): %s",
			imp->imp_client->cli_name, imp,
			(imp->imp_state == PSC_IMP_FULL &&
			 (u32)imp->imp_conn_cnt > conn_cnt) ?
			"reconnected" : "not connected", imp->imp_conn_cnt,
			conn_cnt, pscrpc_import_state_name(imp->imp_state));
        }

        return rc;
}

/*
 * This acts as a barrier; all existing requests are rejected, and
 * no new requests will be accepted until the import is valid again.
 */
void pscrpc_deactivate_import(struct pscrpc_import *imp)
{
        ENTRY;

        spin_lock(&imp->imp_lock);
        psc_warnx("setting import %p INVALID", imp);
        imp->imp_invalid = 1;
        imp->imp_generation++;
        spin_unlock(&imp->imp_lock);

	psc_errorx("Here's where failover is supposed to happen!!!");
        pscrpc_abort_inflight(imp);
        //pscobd_import_event(imp->imp_obd, imp, IMP_EVENT_INACTIVE);
}

/*
 * This function will invalidate the import, if necessary, then block
 * for all the RPC completions, and finally notify the obd to
 * invalidate its state (ie cancel locks, clear pending requests,
 * etc).
 */
void pscrpc_invalidate_import(struct pscrpc_import *imp)
{
        struct l_wait_info lwi;
        int rc;

        if (!imp->imp_invalid)
                pscrpc_deactivate_import(imp);

        LASSERT(imp->imp_invalid);

        /* wait for all requests to error out and call completion callbacks */
        lwi = LWI_TIMEOUT_INTERVAL(MAX(ZOBD_TIMEOUT, 1), HZ, NULL, NULL);
        rc = psc_cli_wait_event(imp->imp_recovery_waitq,
				(atomic_read(&imp->imp_inflight) == 0),
				&lwi);

        if (rc)
                CERROR("rc = %d waiting for callback (%d != 0)\n",
                       rc, atomic_read(&imp->imp_inflight));

        //pscobd_import_event(imp->imp_obd, imp, IMP_EVENT_INVALIDATE);
}

void pscrpc_activate_import(struct pscrpc_import *imp)
{
        //struct obd_device *obd = imp->imp_obd;

        spin_lock(&imp->imp_lock);
        imp->imp_invalid = 0;
        spin_unlock(&imp->imp_lock);

        //pscobd_import_event(obd, imp, IMP_EVENT_ACTIVE);
}

void pscrpc_fail_import(struct pscrpc_import *imp, __u32 conn_cnt)
{
        ENTRY;

	if (imp->imp_state == PSC_IMP_NEW) {
		psc_info("Failing new import %p", imp);
		pscrpc_deactivate_import(imp);
		return;
	}

        if (pscrpc_set_import_discon(imp, conn_cnt)) {
                if (!imp->imp_replayable) {
                        psc_warnx("import for %s not replayable, "
			       "auto-deactivating",
			       libcfs_id2str(imp->imp_connection->c_peer));

                        pscrpc_deactivate_import(imp);
                }
		//CDEBUG(D_HA, "%s: waking up pinger\n",
                //       obd2cli_tgt(imp->imp_obd));

                spin_lock(&imp->imp_lock);
                imp->imp_force_verify = 1;
                spin_unlock(&imp->imp_lock);

                //ptlrpc_pinger_wake_up();
		if (imp->imp_failcb){
			if (-ENOSYS != (int)imp->imp_failcb){
				psc_trace("invoking client failover callback");
				if (0 != imp->imp_failcb(imp->imp_failcbarg)){
					psc_fatalx("imp->failcb() failed");
				} else {
					psc_notify("imp->failcb() succeeded!");
				}
			} else {
				psc_trace("NO failover callback registered");
			}
		} else {
			psc_fatalx("communication failure");
		}
        }
	EXIT;
}
