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

/*
 * Routines for handling simple RPC message exchanges.
 */

#define PSC_SUBSYS PSS_RPC

#include <errno.h>
#include <stdio.h>

#include "psc_rpc/rpc.h"
#include "psc_rpc/rsx.h"

/**
 * pfl_rsx_newreq - Create a new request and associate it with the import.
 * @imp: import portal on which to create the request.
 * @version: version of communication protocol of channel.
 * @op: operation ID of command to send.
 * @rqp: value-result of pointer to RPC request.
 * @nqlens: number of request buffers.
 * @qlens: lengths of request buffers.
 * @nplens: number of reply buffers.
 * @plens: lengths of reply buffers.
 * @mqp: value-result of pointer to start of request buffer.
 */
int
_pfl_rsx_newreq(struct pscrpc_import *imp, int version, int op,
    struct pscrpc_request **rqp, int nqlens, int *qlens,
    int nplens, int *plens, void *mq0p)
{
	*(void **)mq0p = NULL;

	*rqp = pscrpc_prep_req(imp, version, op, nqlens, qlens, NULL);
	if (*rqp == NULL)
		return (-ENOMEM);

	/* Setup request buffer. */
	*(void **)mq0p = pscrpc_msg_buf((*rqp)->rq_reqmsg, 0, qlens[0]);
	if (*(void **)mq0p == NULL)
		psc_fatalx("pscrpc_msg_buf");

	/* Setup reply buffer now so asynchronous RPCs work, too. */
	(*rqp)->rq_replen = pscrpc_msg_size(nplens, plens);
	return (0);
}

/**
 * pfl_rsx_waitrep - Wait for a reply of a "simple" command, i.e. an error code.
 * @rq: the RPC request we sent.
 * @replen: anticipated size of response.
 * @mpp: value-result pointer where reply buffer start will be set.
 */
int
pfl_rsx_waitrep(struct pscrpc_request *rq, int replen, void *mpp)
{
	int rc;

	*(void **)mpp = NULL;

	if (rq->rq_reqmsg->opc == 0)
		abort();

	/* Send the request and block on its completion. */
	rc = pscrpc_queue_wait(rq);
	if (rc)
		return (rc);
	*(void **)mpp = pscrpc_msg_buf(rq->rq_repmsg, 0, replen);
	if (*(void **)mpp == NULL)
		return (-ENOMEM);
	return (0);
}

#define OBD_TIMEOUT 60

int
pfl_rsx_timeout(__unusedx void *arg)
{
	return (1);
}

/**
 * rsx_bulkserver - Setup a source or sink for a server.
 * @rq: RPC request associated with GET.
 * @type: GET_SINK receive from client or PUT_SOURCE to push to a client.
 * @ptl: portal to issue bulk xfer across.
 * @iov: iovec array of receive buffer.
 * @n: #iovecs.
 * Returns: 0 or negative errno on error.
 */
int
rsx_bulkserver(struct pscrpc_request *rq, int type, int ptl,
    struct iovec *iov, int n)
{
	int sum, i, rc, comms_error;
	struct pscrpc_bulk_desc *desc;
	struct l_wait_info lwi;
	uint64_t *v8;
	uint8_t *v1;

	psc_assert(type == BULK_GET_SINK || type == BULK_PUT_SOURCE);

	desc = pscrpc_prep_bulk_exp(rq, n, type, ptl);
	if (desc == NULL) {
		psclog_warnx("pscrpc_prep_bulk_exp returned a null desc");
		return (-ENOMEM); // XXX errno
	}
	desc->bd_nob = 0;
	desc->bd_iov_count = n;
	memcpy(desc->bd_iov, iov, n * sizeof(*iov));
	for (i = 0; i < n; i++)
		desc->bd_nob += iov[i].iov_len;

	/* Check for client eviction during previous I/O before proceeding. */
	if (desc->bd_export->exp_failed)
		rc = -ENOTCONN;
	else
		rc = pscrpc_start_bulk_transfer(desc);
	if (rc == 0) {
		lwi = LWI_TIMEOUT_INTERVAL(OBD_TIMEOUT / 2,
		    100, pfl_rsx_timeout, desc);

		rc = pscrpc_svr_wait_event(&desc->bd_waitq,
		    (!pscrpc_bulk_active(desc) || desc->bd_export->exp_failed),
		    &lwi, NULL);

		LASSERT(rc == 0 || rc == -ETIMEDOUT);
		if (rc == -ETIMEDOUT) {
			psclog_errorx("timeout on bulk GET");
			pscrpc_abort_bulk(desc);
		} else if (desc->bd_export->exp_failed) {
			psclog_warnx("eviction on bulk GET");
			rc = -ENOTCONN;
			pscrpc_abort_bulk(desc);
		} else if (!desc->bd_success ||
		    desc->bd_nob_transferred != desc->bd_nob) {
			psclog_errorx("%s bulk GET %d(%d)",
			    desc->bd_success ? "truncated" : "network error on",
			    desc->bd_nob_transferred, desc->bd_nob);
			/* XXX should this be a different errno? */
			rc = -ETIMEDOUT;
		}
	} else {
		psclog_errorx("pscrpc I/O bulk get failed: rc %d", rc);
	}
	comms_error = (rc != 0);

	/* count the number of bytes received, and hold for later... */
	if (rc == 0) {
		v1 = desc->bd_iov[0].iov_base;
		v8 = desc->bd_iov[0].iov_base;
		if (v1 == NULL) {
			psclog_errorx("desc->bd_iov[0].iov_base is NULL");
			rc = -ENXIO;
			goto out;
		}

		psclog_info("got %u bytes of bulk data across %d IOVs: "
		    "first byte is %#x (%"PRIx64")",
		    desc->bd_nob, desc->bd_iov_count, *v1, *v8);

		sum = 0;
		for (i = 0; i < desc->bd_iov_count; i++)
			sum += desc->bd_iov[i].iov_len;
		if (sum != desc->bd_nob)
			psclog_warnx("sum (%d) does not match bd_nob (%d)",
			    sum, desc->bd_nob);
		//rc = pscrpc_reply(rq);
	}

 out:
	if (rc == 0)
		;
	else if (!comms_error) {
		/* Only reply if there were no comm problems with bulk. */
		rq->rq_status = rc;
		pscrpc_error(rq);
	} else {
#if 0
		// For now let's not free the reply state..
		if (rq->rq_reply_state != NULL) {
			/* reply out callback would free */
			pscrpc_rs_decref(rq->rq_reply_state);
			rq->rq_reply_state = NULL;
			rq->rq_repmsg      = NULL;
		}
#endif
		CWARN("ignoring bulk I/O comm error; "
		    "id %s - client will retry",
		    libcfs_id2str(rq->rq_peer));
	}
	pscrpc_free_bulk(desc);
	return (rc);
}

/**
 * rsx_bulkclient - Setup a source or sink for a client.
 * @type: GET_SOURCE lets server to pull our buffer,
 *	PUT_SINK sets up a buffer filled in by the server
 * @rq: RPC request.
 * @descp: pointer to bulk xfer descriptor.
 * @ptl: portal to issue bulk xfer across.
 * @iov: iovec array of receive buffer.
 * @n: #iovecs.
 * Returns: 0 or negative errno on error.
 */
int
rsx_bulkclient(struct pscrpc_request *rq, int type, int ptl,
    struct iovec *iov, int n)
{
	struct pscrpc_bulk_desc *desc;
	int i;

	psc_assert(type == BULK_GET_SOURCE || type == BULK_PUT_SINK);

	desc = pscrpc_prep_bulk_imp(rq, n, type, ptl);
	if (desc == NULL)
		psc_fatal("NULL bulk descriptor");
	desc->bd_nob = 0;
	desc->bd_iov_count = n;
	memcpy(desc->bd_iov, iov, n * sizeof(*iov));
	for (i = 0; i < n; i++)
		desc->bd_nob += iov[i].iov_len;
	return (0);
}
