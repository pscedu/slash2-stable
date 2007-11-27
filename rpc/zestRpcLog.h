/* $Id: zestRpcLog.h 1988 2007-10-26 01:54:27Z pauln $ */

#ifndef HAVE_ZRPCLOG_H
#define HAVE_ZRPCLOG_H 1

#include "zestLog.h"
#include "zestRpc.h"

static inline const char *
zestrpc_rqphase2str(struct zestrpc_request *req)
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
#define FLAG(field, str) (field ? str : "")

#define DEBUG_REQ_FLAGS(req)                                                \
        zestrpc_rqphase2str(req),                                            \
		FLAG(req->rq_intr, "I"), FLAG(req->rq_replied, "R"),        \
		FLAG(req->rq_err, "E"),                                     \
		FLAG(req->rq_timedout, "X") /* eXpired */,                  \
                FLAG(req->rq_resend, "S"),				    \
		FLAG(req->rq_restart, "T"), FLAG(req->rq_replay, "P"),      \
		FLAG(req->rq_no_resend, "N"),                               \
		FLAG(req->rq_waiting, "W")

#define REQ_FLAGS_FMT "%s:%s%s%s%s%s%s%s%s%s"

#define DEBUG_REQ(level, req, fmt, ...)					\
	do {								\
		_zlog(__FILE__, __func__, __LINE__,			\
		      ZS_RPC, level, 0,					\
		      " req@%p x"LPD64"/t"LPD64" c"LPX64" o%d->@%s:%d "	\
		      "lens %d/%d ref %d res %d ret %d fl "REQ_FLAGS_FMT \
		      "/%x/%x replyc "LPX64" rc %d/%d to=%d :: "fmt,	\
		      req, req->rq_xid, req->rq_transno,		\
		      req->rq_reqmsg ? (u64)req->rq_reqmsg->handle.cookie : 0xfefefefe,	\
		      req->rq_reqmsg ? (int)req->rq_reqmsg->opc : -1,	\
		      req->rq_import ?					\
		      libcfs_id2str(req->rq_import->imp_connection->c_peer) : \
		      req->rq_conn ?					\
		      libcfs_id2str(req->rq_conn->c_peer) : "<?>",	\
		      (req->rq_import && req->rq_import->imp_client) ?	\
		      (int)req->rq_import->imp_client->cli_request_portal : -1,	\
		      req->rq_reqlen, req->rq_replen, atomic_read(&req->rq_refcount), \
		      req->rq_resend,					\
		      atomic_read(&req->rq_retries), DEBUG_REQ_FLAGS(req), \
		      req->rq_reqmsg ? zest_msg_get_flags(req->rq_reqmsg) : 0, \
		      req->rq_repmsg ? zest_msg_get_flags(req->rq_repmsg) : 0, \
		      req->rq_repmsg ? (u64)req->rq_repmsg->handle.cookie : 0xfefefefe,	\
		      req->rq_status,					\
		      req->rq_repmsg ? req->rq_repmsg->status : 0, req->rq_timeout, \
		      ## __VA_ARGS__);					\
	} while(0)


#define DEBUG_EXP(level, exp, fmt, ...)                                      \
do {                                                                         \
        _zlog(__FILE__, __func__, __LINE__,                                  \
	      ZS_RPC, level, 0,					     \
	      " exp@%p h"LPX64" conn@%p p:%s ref %d cnt %d f%d :: "fmt,      \
	      exp, exp->exp_handle.cookie, exp->exp_connection,              \
	      exp->exp_connection ?                                          \
	         libcfs_id2str(exp->exp_connection->c_peer) : "<?>",	     \
	      atomic_read(&exp->exp_refcount),			 	     \
	      atomic_read(&exp->exp_rpc_count), exp->exp_failed,  	     \
	      ## __VA_ARGS__);                                               \
} while(0)




#endif
