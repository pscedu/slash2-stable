/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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

#define PSC_SUBSYS PSS_RPC

#include <stdio.h>

#include "pfl/str.h"
#include "psc_ds/tree.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rsx.h"
#include "psc_rpc/service.h"

#include "bmap_mds.h"
#include "repl_mds.h"
#include "rpc_mds.h"
#include "slashd.h"
#include "slashrpc.h"

struct pscrpc_svc_handle slm_rmi_svc;
struct pscrpc_svc_handle slm_rmm_svc;
struct pscrpc_svc_handle slm_rmc_svc;

void
slmresmonthr_main(__unusedx struct psc_thread *thr)
{
	struct slashrpc_cservice *csvc;
	struct sl_resource *r;
	struct sl_resm *m;
	int i;

	while (pscthr_run()) {
		sleep(30);

		SITE_FOREACH_RES(nodeSite, r, i)
			if (RES_ISFS(r)) {
				m = psc_dynarray_getpos(&r->res_members,
				    0);
				csvc = slm_geticsvcf(m, CSVCF_NONBLOCK |
				    CSVCF_NORECON);
				if (!csvc)
					continue;
				if (!mds_sliod_alive(res2iosinfo(r)))
					sl_csvc_disconnect(csvc);
				sl_csvc_decref(csvc);
			}
#if 0
		SL_MDS_WALK(m,
			csvc = slm_getmcsvcf(m, CSVCF_NONBLOCK |
			    CSVCF_NORECON);
			if (!csvc)
				continue;
			if (!slmm_peer_alive(res2mdsinfo(r)))
				sl_csvc_disconnect(csvc);
			sl_csvc_decref(csvc);
		);
#endif
	}
}

void
slm_rpc_initsvc(void)
{
	struct pscrpc_svc_handle *svh;
	struct slmrcm_thread *srcm;
	struct psc_thread *thr;

	/* Setup request service for MDS from ION. */
	svh = &slm_rmi_svc;
	svh->svh_nbufs = SLM_RMI_NBUFS;
	svh->svh_bufsz = SLM_RMI_BUFSZ;
	svh->svh_reqsz = SLM_RMI_BUFSZ;
	svh->svh_repsz = SLM_RMI_REPSZ;
	svh->svh_req_portal = SRMI_REQ_PORTAL;
	svh->svh_rep_portal = SRMI_REP_PORTAL;
	svh->svh_type = SLMTHRT_RMI;
	svh->svh_nthreads = SLM_RMI_NTHREADS;
	svh->svh_handler = slm_rmi_handler;
	strlcpy(svh->svh_svc_name, SLM_RMI_SVCNAME,
	    sizeof(svh->svh_svc_name));
	pscrpc_thread_spawn(svh, struct slmrmi_thread);

	/* Setup request service for MDS from MDS. */
	svh = &slm_rmm_svc;
	svh->svh_nbufs = SLM_RMM_NBUFS;
	svh->svh_bufsz = SLM_RMM_BUFSZ;
	svh->svh_reqsz = SLM_RMM_BUFSZ;
	svh->svh_repsz = SLM_RMM_REPSZ;
	svh->svh_req_portal = SRMM_REQ_PORTAL;
	svh->svh_rep_portal = SRMM_REP_PORTAL;
	svh->svh_type = SLMTHRT_RMM;
	svh->svh_nthreads = SLM_RMM_NTHREADS;
	svh->svh_handler = slm_rmm_handler;
	strlcpy(svh->svh_svc_name, SLM_RMM_SVCNAME,
	    sizeof(svh->svh_svc_name));
	pscrpc_thread_spawn(svh, struct slmrmm_thread);

	/* Setup request service for MDS from client. */
	svh = &slm_rmc_svc;
	svh->svh_nbufs = SLM_RMC_NBUFS;
	svh->svh_bufsz = SLM_RMC_BUFSZ;
	svh->svh_reqsz = SLM_RMC_BUFSZ;
	svh->svh_repsz = SLM_RMC_REPSZ;
	svh->svh_req_portal = SRMC_REQ_PORTAL;
	svh->svh_rep_portal = SRMC_REP_PORTAL;
	svh->svh_type = SLMTHRT_RMC;
	svh->svh_nthreads = SLM_RMC_NTHREADS;
	svh->svh_handler = slm_rmc_handler;
	strlcpy(svh->svh_svc_name, SLM_RMC_SVCNAME,
	    sizeof(svh->svh_svc_name));
	pscrpc_thread_spawn(svh, struct slmrmc_thread);

	thr = pscthr_init(SLMTHRT_RCM, 0, slmrcmthr_main,
	    NULL, sizeof(*srcm), "slmrcmthr");
	srcm = thr->pscthr_private;
	srcm->srcm_page = PSCALLOC(SRM_REPLST_PAGESIZ);
	pscthr_setready(thr);

	pscthr_init(SLMTHRT_RESMON, 0, slmresmonthr_main, NULL, 0,
	    "slmresmonthr");
}

void
sl_resm_hldrop(struct sl_resm *resm)
{
	if (resm->resm_type == SLREST_MDS) {
	} else {
		mds_repl_reset_scheduled(resm->resm_res_id);
		mds_repl_node_clearallbusy(resm2rmmi(resm));
	}
}

void
slm_rpc_ion_pack_bmapminseq(struct pscrpc_msg *m)
{
	struct srt_bmapminseq *sbms;

	if (m == NULL) {
		psclog_errorx("unable to export bmapminseq");
		return;
	}
	sbms = pscrpc_msg_buf(m, m->bufcount - 2, sizeof(*sbms));
	if (sbms == NULL) {
		psclog_errorx("unable to export bmapminseq");
		return;
	}
	mds_bmap_getcurseq(NULL, &sbms->bminseq);
}

void
slm_rpc_ion_unpack_statfs(struct pscrpc_request *rq, int type)
{
	struct resm_mds_info *rmmi;
	struct sl_mds_iosinfo *si;
	struct srt_statfs *f;
	struct pscrpc_msg *m;
	struct sl_resm *resm;

	m = type == PSCRPC_MSG_REPLY ? rq->rq_repmsg : rq->rq_reqmsg;
	if (m == NULL) {
		psclog_errorx("unable to import statfs");
		return;
	}
	if (m->bufcount < 2) {
		psclog_errorx("unable to import statfs");
		return;
	}
	f = pscrpc_msg_buf(m, m->bufcount - 2, sizeof(*f));
	if (f == NULL) {
		psclog_errorx("unable to import statfs");
		return;
	}
	resm = libsl_nid2resm(pscrpc_req_getconn(rq)->c_peer.nid);
	if (resm == NULL) {
		psclog_errorx("unknown peer");
		return;
	}
	rmmi = resm2rmmi(resm);
	RMMI_LOCK(rmmi);
	si = res2iosinfo(resm->resm_res);
	memcpy(&si->si_ssfb, f, sizeof(*f));
	RMMI_ULOCK(rmmi);
}

int
slrpc_newreq(struct slashrpc_cservice *csvc, int op,
    struct pscrpc_request **rqp, int qlen, int plen, void *mqp)
{
	int rc;

	if (csvc->csvc_ctype == SLCONNT_IOD) {
		int qlens[] = {
			qlen,
			sizeof(struct srt_bmapminseq),
			sizeof(struct srt_authbuf_footer)
		};
		int plens[] = {
			plen,
			sizeof(struct srt_statfs),
			sizeof(struct srt_authbuf_footer)
		};

		rc = RSX_NEWREQN(csvc->csvc_import, csvc->csvc_version,
		    op, *rqp, nitems(qlens), qlens, nitems(plens),
		    plens, *(void **)mqp);
	} else
		rc = slrpc_newgenreq(csvc, op, rqp, qlen, plen, mqp);
	if (rc == 0 && op == SRMT_CONNECT) {
		struct srm_connect_req *mq = *(void **)mqp;

		mq->fsuuid = slm_fsuuid;
	}
	return (rc);
}

int
slrpc_waitrep(struct slashrpc_cservice *csvc,
    struct pscrpc_request *rq, int plen, void *mpp)
{
	int rc;

	if (csvc->csvc_ctype == SLCONNT_IOD)
		slm_rpc_ion_pack_bmapminseq(rq->rq_reqmsg);
	rc = slrpc_waitgenrep(rq, plen, mpp);
	if (csvc->csvc_ctype == SLCONNT_IOD)
		slm_rpc_ion_unpack_statfs(rq, PSCRPC_MSG_REPLY);
	return (rc);
}

int
slrpc_allocrep(struct pscrpc_request *rq, void *mqp, int qlen,
    void *mpp, int plen, int rcoff)
{
	int rc;

	if (rq->rq_rqbd->rqbd_service == slm_rmi_svc.svh_service) {
		int plens[] = {
			plen,
			sizeof(struct srt_bmapminseq),
			sizeof(struct srt_authbuf_footer)
		};

		rc = slrpc_allocrepn(rq, mqp, qlen, mpp, nitems(plens),
		    plens, rcoff);
		slm_rpc_ion_unpack_statfs(rq, PSCRPC_MSG_REQUEST);
		slm_rpc_ion_pack_bmapminseq(rq->rq_repmsg);
	} else
		rc = slrpc_allocgenrep(rq, mqp, qlen, mpp, plen, rcoff);
	if (rc == 0 && rq->rq_reqmsg->opc == SRMT_CONNECT) {
		struct srm_connect_rep *mp = *(void **)mpp;

		mp->fsuuid = slm_fsuuid;
	}
	return (rc);
}
