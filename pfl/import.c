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

#include "psc_rpc/rpc.h"

static inline char *
pscrpc_import_state_name(enum pscrpc_imp_state state)
{
	static char* import_state_names[] = {
		"<UNKNOWN>", "CLOSED",  "NEW", "DISCONN",
		"CONNECTING", "REPLAY", "REPLAY_LOCKS", "REPLAY_WAIT",
		"RECOVER", "FULL", "EVICTED",
	};

	LASSERT(state <= PSCRPC_IMP_EVICTED);
	return import_state_names[state];
}

/* A CLOSED import should remain so. */
#define PSCIMPORT_SET_STATE_NOLOCK(imp, state)					\
	do {									\
		if ((imp)->imp_state != PSCRPC_IMP_CLOSED) {			\
			psclog_warnx("%p %s: changing import state %s to %s",	\
			    (imp),						\
			    libcfs_id2str((imp)->imp_connection->c_peer),	\
			    pscrpc_import_state_name((imp)->imp_state),		\
			    pscrpc_import_state_name(state));			\
			(imp)->imp_state = state;				\
		}								\
	} while (0)

int
pscrpc_init_import(struct pscrpc_import *imp)
{
	spinlock(&imp->imp_lock);

	imp->imp_generation++;
	imp->imp_state =  PSCRPC_IMP_NEW;

	freelock(&imp->imp_lock);

	return 0;
}

/**
 * pscrpc_set_import_discon - Returns true if import was FULL,
 *	false if import was already not connected.
 * @imp - import to be disconnected
 * @conn_cnt - connection count (epoch) of the request that timed out
 *             and caused the disconnection.  In some cases, multiple
 *             inflight requests can fail to a single target (e.g. OST
 *             bulk requests) and if one has already caused a reconnection
 *             (increasing the import->conn_cnt) the older failure should
 *             not also cause a reconnection.  If zero it forces a reconnect.
 */
int
pscrpc_set_import_discon(struct pscrpc_import *imp, uint32_t conn_cnt)
{
	int rc = 0;

	spinlock(&imp->imp_lock);

	psclog_warnx("inhere conn_cnt=%u imp_conn_cnt=%u imp_state=%d",
	    conn_cnt, imp->imp_conn_cnt, imp->imp_state);

	if (imp->imp_state == PSCRPC_IMP_FULL &&
	    (conn_cnt == 0 || conn_cnt == (uint32_t)imp->imp_conn_cnt)) {

		psclog_errorx("connection to service via nid %s was "
		    "lost; in progress operations using this "
		    "service will %s",
		    libcfs_nid2str(imp->imp_connection->c_peer.nid),
		    imp->imp_replayable ?
		    "wait for recovery to complete" : "fail");

		PSCIMPORT_SET_STATE_NOLOCK(imp, PSCRPC_IMP_DISCON);
		freelock(&imp->imp_lock);

		//if (obd_dump_on_timeout)
		//        libcfs_debug_dumplog();

#if PAULS_TODO
		/* inform the client to take action in response to the event */
		pscobd_import_event(imp->imp_obd, imp, IMP_EVENT_DISCON);
#endif
		rc = 1;
	} else {
		freelock(&imp->imp_lock);
		psclog_warnx("%s: import %p already %s (conn %u, was %u): %s",
		    imp->imp_client->cli_name, imp,
		    (imp->imp_state == PSCRPC_IMP_FULL &&
		     (uint32_t)imp->imp_conn_cnt > conn_cnt) ?
		    "reconnected" : "not connected", imp->imp_conn_cnt,
		    conn_cnt, pscrpc_import_state_name(imp->imp_state));
	}

	return rc;
}

/*
 * This acts as a barrier; all existing requests are rejected, and
 * no new requests will be accepted until the import is valid again.
 */
void
pscrpc_deactivate_import(struct pscrpc_import *imp)
{
	spinlock(&imp->imp_lock);
	psclog_info("setting import %p INVALID", imp);
	imp->imp_invalid = 1;
	imp->imp_generation++;
	freelock(&imp->imp_lock);

	pscrpc_abort_inflight(imp);
	//pscobd_import_event(imp->imp_obd, imp, IMP_EVENT_INACTIVE);
}

/*
 * This function will invalidate the import, if necessary, then block
 * for all the RPC completions, and finally notify the obd to
 * invalidate its state (ie cancel locks, clear pending requests,
 * etc).
 */
void
pscrpc_invalidate_import(struct pscrpc_import *imp)
{
	struct l_wait_info lwi;
	int rc;

	if (!imp->imp_invalid)
		pscrpc_deactivate_import(imp);

	LASSERT(imp->imp_invalid);

	/* wait for all requests to error out and call completion callbacks */
	lwi = LWI_TIMEOUT_INTERVAL(MAX(PSCRPC_OBD_TIMEOUT, 1), 100, NULL, NULL);
	rc = pscrpc_cli_wait_event(&imp->imp_recovery_waitq,
			    (atomic_read(&imp->imp_inflight) == 0),
			    &lwi);

	if (rc)
		CERROR("rc = %d waiting for callback (%d != 0)\n",
		       rc, atomic_read(&imp->imp_inflight));

	//pscobd_import_event(imp->imp_obd, imp, IMP_EVENT_INVALIDATE);
}

void
pscrpc_activate_import(struct pscrpc_import *imp)
{
	//struct obd_device *obd = imp->imp_obd;

	spinlock(&imp->imp_lock);
	imp->imp_invalid = 0;
	freelock(&imp->imp_lock);

	//pscobd_import_event(obd, imp, IMP_EVENT_ACTIVE);
}

void
pscrpc_fail_import(struct pscrpc_import *imp, uint32_t conn_cnt)
{
	if (imp->imp_state == PSCRPC_IMP_NEW) {
		char *addr = NULL;

		if (imp && imp->imp_connection)
			addr = libcfs_nid2str(
			    imp->imp_connection->c_peer.nid);
		psclog_notice("failing new import %p peer %s", imp,
		    addr);
		pscrpc_deactivate_import(imp);
		return;
	}

	if (pscrpc_set_import_discon(imp, conn_cnt)) {
		if (!imp->imp_replayable) {
			psclog_warnx("import for %s not replayable, "
			    "auto-deactivating",
			    libcfs_id2str(imp->imp_connection->c_peer));

			pscrpc_deactivate_import(imp);
		}
		//CDEBUG(D_HA, "%s: waking up pinger\n",
		//       obd2cli_tgt(imp->imp_obd));

		psclog_notice("failing import %p", imp);
		spinlock(&imp->imp_lock);
		imp->imp_force_verify = 1;
		imp->imp_failed = 1;
		freelock(&imp->imp_lock);

		if (imp->imp_hldropf)
			imp->imp_hldropf(imp->imp_hldrop_arg);
	}
}
