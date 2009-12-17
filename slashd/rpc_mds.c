/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <stdio.h>

#include "psc_ds/tree.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rsx.h"
#include "psc_rpc/service.h"
#include "psc_util/strlcpy.h"

#include "cfd.h"
#include "rpc_mds.h"
#include "slashd.h"
#include "slashrpc.h"

void (*slexp_freef[SLNCONNT])(struct pscrpc_export *) = {
	mexpcli_destroy
};

int
slm_rim_issue_ping(struct pscrpc_import *imp)
{
	const struct srm_generic_rep *mp;
	struct pscrpc_request *rq;
	struct srm_ping_req *mq;
	int rc;

	if ((rc = RSX_NEWREQ(imp, SRIM_VERSION,
	    SRMT_PING, rq, mq, mp)) != 0)
		return (rc);
	rc = RSX_WAITREP(rq, mp);
	pscrpc_req_finished(rq);
	return (rc);
}

void
slm_rpc_initsvc(void)
{
	struct pscrpc_svc_handle *svh;

	/* Setup request service for MDS from ION. */
	svh = PSCALLOC(sizeof(*svh));
	svh->svh_nbufs = SLM_RMI_NBUFS;
	svh->svh_bufsz = SLM_RMI_BUFSZ;
	svh->svh_reqsz = SLM_RMI_BUFSZ;
	svh->svh_repsz = SLM_RMI_REPSZ;
	svh->svh_req_portal = SRMI_REQ_PORTAL;
	svh->svh_rep_portal = SRMI_REP_PORTAL;
	svh->svh_type = SLMTHRT_RMI;
	svh->svh_nthreads = SLM_RMI_NTHREADS;
	svh->svh_handler = slm_rmi_handler;
	strlcpy(svh->svh_svc_name, SLM_RMI_SVCNAME, sizeof(svh->svh_svc_name));
	pscrpc_thread_spawn(svh, struct slmrmi_thread);

	/* Setup request service for MDS from MDS. */
	svh = PSCALLOC(sizeof(*svh));
	svh->svh_nbufs = SLM_RMM_NBUFS;
	svh->svh_bufsz = SLM_RMM_BUFSZ;
	svh->svh_reqsz = SLM_RMM_BUFSZ;
	svh->svh_repsz = SLM_RMM_REPSZ;
	svh->svh_req_portal = SRMM_REQ_PORTAL;
	svh->svh_rep_portal = SRMM_REP_PORTAL;
	svh->svh_type = SLMTHRT_RMM;
	svh->svh_nthreads = SLM_RMM_NTHREADS;
	svh->svh_handler = slm_rmm_handler;
	strlcpy(svh->svh_svc_name, SLM_RMM_SVCNAME, sizeof(svh->svh_svc_name));
	pscrpc_thread_spawn(svh, struct slmrmm_thread);

	/* Setup request service for MDS from client. */
	svh = PSCALLOC(sizeof(*svh));
	svh->svh_nbufs = SLM_RMC_NBUFS;
	svh->svh_bufsz = SLM_RMC_BUFSZ;
	svh->svh_reqsz = SLM_RMC_BUFSZ;
	svh->svh_repsz = SLM_RMC_REPSZ;
	svh->svh_req_portal = SRMC_REQ_PORTAL;
	svh->svh_rep_portal = SRMC_REP_PORTAL;
	svh->svh_type = SLMTHRT_RMC;
	svh->svh_nthreads = SLM_RMC_NTHREADS;
	svh->svh_handler = slm_rmc_handler;
	strlcpy(svh->svh_svc_name, SLM_RMC_SVCNAME, sizeof(svh->svh_svc_name));
	pscrpc_thread_spawn(svh, struct slmrmc_thread);
}

struct mexp_cli *
mexpcli_get(struct pscrpc_export *exp)
{
	struct slashrpc_export *slexp;
	struct mexp_cli *mexp_cli;
	int locked;

	locked = reqlock(&exp->exp_lock);
	slexp = slashrpc_export_get(exp, SLCONNT_CLI);
	mexp_cli = slexp->slexp_data;
	if (mexp_cli == NULL) {
		mexp_cli = slexp->slexp_data =
		    PSCALLOC(sizeof(*mexp_cli));
		LOCK_INIT(&mexp_cli->mc_lock);
		SPLAY_INIT(&mexp_cli->mc_cfdtree);
	}
	ureqlock(&exp->exp_lock, locked);
	return (mexp_cli);
}

void
mexpcli_destroy(struct pscrpc_export *exp)
{
	struct slashrpc_export *slexp = exp->exp_private;
	struct mexp_cli *mexpc = slexp->slexp_data;

	cfdfreeall(exp, slexp->slexp_peertype);
	if (mexpc && mexpc->mc_csvc)
		slashrpc_csvc_free(mexpc->mc_csvc);
	PSCFREE(mexpc);
}
