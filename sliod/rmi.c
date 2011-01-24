/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Routines for issuing RPC requests to MDS from ION.
 */

#include <stdint.h>

#include "psc_rpc/rpc.h"
#include "psc_rpc/rsx.h"

#include "bmap_iod.h"
#include "repl_iod.h"
#include "rpc_iod.h"
#include "slashrpc.h"
#include "slconfig.h"
#include "slconn.h"
#include "slerr.h"
#include "sliod.h"

struct sl_resm *rmi_resm;

int
sli_rmi_getimp(struct slashrpc_cservice **csvcp)
{
	int wait = 1;

	do {
		*csvcp = sli_getmcsvc(rmi_resm);
#if 0
		ctx = fuse_get_context(rq);
		// if process doesn't want to wait
			wait = 0
#endif
	} while (*csvcp == NULL && wait);
	return (0);
}

int
sli_rmi_setmds(const char *name)
{
	struct sl_resource *res;
	struct sl_resm *old;
	lnet_nid_t nid;

	old = rmi_resm;
	nid = libcfs_str2nid(name);
	if (nid == LNET_NID_ANY) {
		res = libsl_str2res(name);
		if (res == NULL)
			return (SLERR_RES_UNKNOWN);
		rmi_resm = psc_dynarray_getpos(&res->res_members, 0);
	} else
		rmi_resm = libsl_nid2resm(nid);

	/* XXX kill any old MDS and purge any bmap updates being held */
//	sl_csvc_disable(old->resm_csvc);

	slconnthr_spawn(rmi_resm, SRMI_REQ_PORTAL, SRMI_REP_PORTAL,
	    SRMI_MAGIC, SRMI_VERSION, &resm2rmii(rmi_resm)->rmii_lock, 0,
	    &resm2rmii(rmi_resm)->rmii_waitq, SLCONNT_MDS, SLITHRT_CONN,
	    "sli");

	return (0);
}

int
sli_rmi_issue_repl_schedwk(struct sli_repl_workrq *w)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_repl_schedwk_req *mq;
	struct srm_generic_rep *mp;
	int rc;

	rc = sli_rmi_getimp(&csvc);
	if (rc)
		goto out;
	rc = SL_RSX_NEWREQ(csvc, SRMT_REPL_SCHEDWK, rq, mq, mp);
	if (rc)
		goto out;
	mq->nid = w->srw_nid;
	mq->fg = w->srw_fg;
	mq->bmapno = w->srw_bmapno;
	mq->bgen = w->srw_bgen;
	mq->rc = w->srw_status;
	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc == 0)
		rc = mp->rc;

 out:
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
	return (rc);
}

void
sli_rmi_read_bminseq(struct pscrpc_request *rq)
{
	struct srt_bmapminseq *sbms;
	struct pscrpc_msg *m;

	m = rq->rq_repmsg;
	if (m == NULL)
		goto error;
	if (m->bufcount < 3)
		goto error;
	sbms = pscrpc_msg_buf(m, m->bufcount - 2, sizeof(*sbms));
	if (sbms == NULL)
		goto error;
	bim_updateseq(sbms->bminseq);
	return;

 error:
	psclog_errorx("no message");
}
