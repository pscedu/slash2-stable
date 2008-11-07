/* $Id$ */

#ifndef _PFL_RPCLOG_H_
#define _PFL_RPCLOG_H_

#include "psc_rpc/rpc.h"
#include "psc_util/log.h"

static inline const char *
pscrpc_rqphase2str(struct pscrpc_request *req)
{
	switch (req->rq_phase) {
	case ZRQ_PHASE_NEW:
		return "New";
	case ZRQ_PHASE_RPC:
		return "Rpc";
	case ZRQ_PHASE_BULK:
		return "Bulk";
	case ZRQ_PHASE_INTERPRET:
		return "Interpret";
	case ZRQ_PHASE_COMPLETE:
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
	(rq)->rq_waiting	? "W" : ""

#define REQ_FLAGS_FMT "%s:%s%s%s%s%s%s%s%s%s%s"

#define DEBUG_REQ(level, rq, fmt, ...)					\
    do {								\
	struct pscrpc_import *__imp = (rq)->rq_import;			\
	char __nidstr[PSC_NIDSTR_SIZE], __idstr[PSC_NIDSTR_SIZE];	\
									\
	if ((rq)->rq_conn)						\
		psc_nid2str((rq)->rq_conn->c_peer.nid, __nidstr);	\
	if (__imp)							\
		psc_id2str(__imp->imp_connection->c_peer, __idstr);	\
	_psclog(__FILE__, __func__, __LINE__,				\
	    PSS_RPC, (level), 0,					\
	    " req@%p x%"_P_U64"d/t%"_P_U64"d "				\
	    "c%"_P_U64"x "						\
	    "o%d->@%s:%d "						\
	    "lens %d/%d ref %d res %d ret %d fl "REQ_FLAGS_FMT		\
	    "/%x/%x replyc %"_P_U64"x rc %d/%d to=%d :: "fmt,		\
	    (rq), (rq)->rq_xid, (rq)->rq_transno,			\
	    (rq)->rq_reqmsg ? (rq)->rq_reqmsg->handle.cookie : 0xdeadbeef, \
	    (rq)->rq_reqmsg ? (int)(rq)->rq_reqmsg->opc : -1,		\
	    __imp ? __idstr : (rq)->rq_conn ? __nidstr : "<?>",		\
	    __imp && __imp->imp_client ?				\
	      (int)__imp->imp_client->cli_request_portal : -1,		\
	    (rq)->rq_reqlen, (rq)->rq_replen,				\
	    atomic_read(&(rq)->rq_refcount), (rq)->rq_resend,		\
	    atomic_read(&(rq)->rq_retries), DEBUG_REQ_FLAGS(rq),	\
	    (rq)->rq_reqmsg ? psc_msg_get_flags((rq)->rq_reqmsg) : -1,	\
	    (rq)->rq_repmsg ? psc_msg_get_flags((rq)->rq_repmsg) : 0,	\
	    (rq)->rq_repmsg ? (rq)->rq_repmsg->handle.cookie : 0xdeadbeef, \
	    (rq)->rq_status,						\
	    (rq)->rq_repmsg ? (rq)->rq_repmsg->status : 0,		\
	    (rq)->rq_timeout, ## __VA_ARGS__);				\
    } while (0)

#define DEBUG_EXP(level, exp, fmt, ...)					\
	_psclog(__FILE__, __func__, __LINE__,				\
	    PSS_RPC, level, 0, " exp@%p h%"_P_U64"x "			\
	    "conn@%p p:%s ref %d cnt %d f%d :: "fmt,			\
	    exp, exp->exp_handle.cookie, exp->exp_connection,		\
	    exp->exp_connection ?					\
	       libcfs_id2str(exp->exp_connection->c_peer) : "<?>",	\
	    atomic_read(&exp->exp_refcount),				\
	    atomic_read(&exp->exp_rpc_count), exp->exp_failed,		\
	    ## __VA_ARGS__)

#endif /* _PFL_RPCLOG_H_ */
