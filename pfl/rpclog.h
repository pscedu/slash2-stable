/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_RPCLOG_H_
#define _PFL_RPCLOG_H_

#include "pfl/pfl.h"
#include "pfl/rpc.h"
#include "pfl/log.h"

static inline const char *
pscrpc_rqphase2str(struct pscrpc_request *req)
{
	switch (req->rq_phase) {
	case PSCRPC_RQ_PHASE_NEW:
		return "New";
	case PSCRPC_RQ_PHASE_RPC:
		return "Rpc";
	case PSCRPC_RQ_PHASE_BULK:
		return "Bulk";
	case PSCRPC_RQ_PHASE_INTERPRET:
		return "Interpret";
	case PSCRPC_RQ_PHASE_COMPLETE:
		return "Complete";
	default:
		return "?Phase?";
	}
}

/* Spare the preprocessor, spoil the bugs. */
#define DEBUG_REQ_FLAGS(rq)						\
	pscrpc_rqphase2str(rq),						\
	(rq)->rq_intr		? "I" : "",				\
	(rq)->rq_replied	? "R" : "",				\
	(rq)->rq_err		? "E" : "",				\
	(rq)->rq_net_err	? "e" : "",				\
	(rq)->rq_timedout	? "X" : "", /* eXpired */		\
	(rq)->rq_resend		? "S" : "",				\
	(rq)->rq_restart	? "T" : "",				\
	(rq)->rq_replay		? "P" : "",				\
	(rq)->rq_no_resend	? "N" : "",				\
	(rq)->rq_abort_reply	? "A" : "",				\
	(rq)->rq_waiting	? "W" : ""

#define DEBUGS_REQ(level, ss, rq, buf, fmt, ...)			\
	psclogs((level), (ss),						\
	    "req@%p x%"PRId64"/t%"PRId64" cb=%p "			\
	    "c%"PRIx64" "						\
	    "o%d->@%s:%d "						\
	    "lens %d/%d ref %d res %d ret %d "				\
	    "fl %s:%s%s%s%s%s%s%s%s%s%s%s/%x/%x "			\
	    "replyc %"PRIx64" rc %d/%d to=%d "				\
	    "sent=%"PSCPRI_TIMET" :: "fmt,				\
	    (rq), (rq)->rq_xid, (rq)->rq_transno,			\
	    (rq)->rq_interpret_reply, (rq)->rq_reqmsg ?			\
	       (rq)->rq_reqmsg->handle.cookie : 0xdeadbeef,		\
	    (rq)->rq_reqmsg ? (int)(rq)->rq_reqmsg->opc : -1,		\
	    (rq)->rq_import ?						\
	       pscrpc_id2str2((rq)->rq_import->imp_connection->c_peer, buf) :	\
	       (rq)->rq_conn ? pscrpc_nid2str2((rq)->rq_conn->			\
		  c_peer.nid, buf) : (rq)->rq_peer.nid != LNET_NID_ANY ?	\
		     pscrpc_nid2str2((rq)->rq_peer.nid, buf) : "<?>",	\
	    (rq)->rq_import ?						\
	       (int)(rq)->rq_import->imp_cli_request_portal : -1,	\
	    (rq)->rq_reqlen, (rq)->rq_replen,				\
	    atomic_read(&(rq)->rq_refcount), (rq)->rq_resend,		\
	    (rq)->rq_retries, DEBUG_REQ_FLAGS(rq),			\
	    (rq)->rq_reqmsg ?						\
	       pscrpc_msg_get_flags((rq)->rq_reqmsg) : -1,		\
	    (rq)->rq_repmsg ?						\
	       pscrpc_msg_get_flags((rq)->rq_repmsg) : 0,		\
	    (rq)->rq_repmsg ?						\
	       (rq)->rq_repmsg->handle.cookie : 0xdeadbeef,		\
	    (rq)->rq_status,						\
	    (rq)->rq_repmsg ? (rq)->rq_repmsg->status : 0,		\
	    (rq)->rq_timeout, (rq)->rq_sent, ## __VA_ARGS__)

#define DEBUG_REQ(level, rq, buf, fmt, ...) DEBUGS_REQ((level), PSS_RPC, (rq), buf, fmt, ## __VA_ARGS__)

#define DEBUG_EXP(level, exp, fmt, ...)					\
	psclogs((level), PSS_RPC,					\
	    "exp@%p h%"PRIx64" conn@%p p:%s ref %d cnt %d f%d :: "fmt,	\
	    (exp), (exp)->exp_handle.cookie, (exp)->exp_connection,	\
	    (exp)->exp_connection ?					\
	       libcfs_id2str((exp)->exp_connection->c_peer) : "<?>",	\
	    atomic_read(&(exp)->exp_refcount),				\
	    atomic_read(&(exp)->exp_rpc_count), (exp)->exp_failed,	\
	    ## __VA_ARGS__)

#endif /* _PFL_RPCLOG_H_ */
