/* $Id: zestImport.c 2137 2007-11-05 18:41:27Z yanovich $ */

#include "subsys.h"
#define ZSUBSYS ZS_RPC

#include "zestRpc.h"

static inline char *
zestrpc_import_state_name(enum zest_imp_state state)
{
        static char* import_state_names[] = {
                "<UNKNOWN>", "CLOSED",  "NEW", "DISCONN",
                "CONNECTING", "REPLAY", "REPLAY_LOCKS", "REPLAY_WAIT",
                "RECOVER", "FULL", "EVICTED",
        };

        LASSERT (state <= ZEST_IMP_EVICTED);
        return import_state_names[state];
}

/* A CLOSED import should remain so. */
#define ZIMPORT_SET_STATE_NOLOCK(imp, state)				\
	do {								\
		if (imp->imp_state != ZEST_IMP_CLOSED) {		\
			zwarnx("%p %s: changing import state from %s to %s", \
			       imp, libcfs_id2str(imp->imp_connection->c_peer), \
			       zestrpc_import_state_name(imp->imp_state), \
			       zestrpc_import_state_name(state));	\
			imp->imp_state = state;				\
		}							\
	} while(0)


int zestrpc_init_import(struct zestrpc_import *imp)
{
        spin_lock(&imp->imp_lock);

        imp->imp_generation++;
        imp->imp_state =  ZEST_IMP_NEW;

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
int zestrpc_set_import_discon(struct zestrpc_import *imp, u32 conn_cnt)
{
        int rc = 0;

        spin_lock(&imp->imp_lock);

	zwarnx("inhere conn_cnt %u imp_conn_cnt %u, imp->imp_state = %d",
	       conn_cnt, imp->imp_conn_cnt, imp->imp_state);

        if (imp->imp_state == ZEST_IMP_FULL &&
            (conn_cnt == 0 || conn_cnt == (u32)imp->imp_conn_cnt)) {

		zerrorx("Connection to service via nid %s was "
			"lost; in progress operations using this "
			"service will %s.",
			libcfs_nid2str(imp->imp_connection->c_peer.nid),
			imp->imp_replayable ?
			"wait for recovery to complete" : "fail");

                ZIMPORT_SET_STATE_NOLOCK(imp, ZEST_IMP_DISCON);
                spin_unlock(&imp->imp_lock);

                //if (obd_dump_on_timeout)
                //        libcfs_debug_dumplog();

#if PAULS_TODO
		/* inform the client to take action in response to the event */
                zestobd_import_event(imp->imp_obd, imp, IMP_EVENT_DISCON);
#endif
                rc = 1;
        } else {
                spin_unlock(&imp->imp_lock);
                zwarnx("%s: import %p already %s (conn %u, was %u): %s",
			imp->imp_client->cli_name, imp,
			(imp->imp_state == ZEST_IMP_FULL &&
			 (u32)imp->imp_conn_cnt > conn_cnt) ?
			"reconnected" : "not connected", imp->imp_conn_cnt,
			conn_cnt, zestrpc_import_state_name(imp->imp_state));
        }

        return rc;
}

/*
 * This acts as a barrier; all existing requests are rejected, and
 * no new requests will be accepted until the import is valid again.
 */
void zestrpc_deactivate_import(struct zestrpc_import *imp)
{
        ENTRY;

        spin_lock(&imp->imp_lock);
        zwarnx("setting import %p INVALID", imp);
        imp->imp_invalid = 1;
        imp->imp_generation++;
        spin_unlock(&imp->imp_lock);

	zerrorx("Here's where failover is supposed to happen!!!");
        zestrpc_abort_inflight(imp);
        //zestobd_import_event(imp->imp_obd, imp, IMP_EVENT_INACTIVE);
}

/*
 * This function will invalidate the import, if necessary, then block
 * for all the RPC completions, and finally notify the obd to
 * invalidate its state (ie cancel locks, clear pending requests,
 * etc).
 */
void zestrpc_invalidate_import(struct zestrpc_import *imp)
{
        struct l_wait_info lwi;
        int rc;

        if (!imp->imp_invalid)
                zestrpc_deactivate_import(imp);

        LASSERT(imp->imp_invalid);

        /* wait for all requests to error out and call completion callbacks */
        lwi = LWI_TIMEOUT_INTERVAL(MAX(ZOBD_TIMEOUT, 1), HZ, NULL, NULL);
        rc = zcli_wait_event(imp->imp_recovery_waitq,
			     (atomic_read(&imp->imp_inflight) == 0),
			     &lwi);

        if (rc)
                CERROR("rc = %d waiting for callback (%d != 0)\n",
                       rc, atomic_read(&imp->imp_inflight));

        //zestobd_import_event(imp->imp_obd, imp, IMP_EVENT_INVALIDATE);
}

void zestrpc_activate_import(struct zestrpc_import *imp)
{
        //struct obd_device *obd = imp->imp_obd;

        spin_lock(&imp->imp_lock);
        imp->imp_invalid = 0;
        spin_unlock(&imp->imp_lock);

        //zestobd_import_event(obd, imp, IMP_EVENT_ACTIVE);
}

void zestrpc_fail_import(struct zestrpc_import *imp, __u32 conn_cnt)
{
        ENTRY;

	if (imp->imp_state == ZEST_IMP_NEW) {
		zinfo("Failing new import %p", imp);
		zestrpc_deactivate_import(imp);
		return;
	}

        if (zestrpc_set_import_discon(imp, conn_cnt)) {
                if (!imp->imp_replayable) {
                        zwarnx("import for %s not replayable, "
			       "auto-deactivating",
			       libcfs_id2str(imp->imp_connection->c_peer));

                        zestrpc_deactivate_import(imp);
                }
		//CDEBUG(D_HA, "%s: waking up pinger\n",
                //       obd2cli_tgt(imp->imp_obd));

                spin_lock(&imp->imp_lock);
                imp->imp_force_verify = 1;
                spin_unlock(&imp->imp_lock);

                //ptlrpc_pinger_wake_up();
		zfatalx("Shouldn't be here just yet.. failover not ready");
        }
        EXIT;
}
