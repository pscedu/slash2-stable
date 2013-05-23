/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_RSX_H_
#define _PFL_RSX_H_

struct pscrpc_request;
struct pscrpc_import;
struct iovec;

struct rsx_msg_conversion {
	int			 rmc_offset;
	int			 rmc_size;
};

#define RSX_PORT_NAME_MAX	50

struct rsx_msg_portablizer {
	char			 rmp_name[50];
	int			 rmp_nconv;
	struct rpcmsg_conv	*rmp_conv;
};

#define RSX_ALLOCREPNRC(rq, mq0, q0len, mp0, np, plens, rcoff)		\
	do {								\
		int _rc, *_rcp;						\
									\
		if (pscrpc_msg_size((np), (plens)) >			\
		    (rq)->rq_rqbd->rqbd_service->srv_max_reply_size)	\
			psc_fatalx("reply size (%d) greater than "	\
			    "max (%d)",					\
			    pscrpc_msg_size((np), (plens)),		\
			    (rq)->rq_rqbd->rqbd_service->		\
			     srv_max_reply_size);			\
		_rc = pscrpc_pack_reply((rq), (np), (plens), NULL);	\
		if (_rc) {						\
			psc_assert(_rc == -ENOMEM);			\
			psclog_errorx("pscrpc_pack_reply failed: %s",	\
			    strerror(_rc));				\
			return (_rc);					\
		}							\
		if (((mp0) = pscrpc_msg_buf((rq)->rq_repmsg, 0,		\
		    (plens)[0])) == NULL) {				\
			psclog_errorx("reply is NULL");			\
			return (-ENOMEM);				\
		}							\
		_rcp = PSC_AGP((mp0), (rcoff));				\
		*_rcp = 0;						\
		if (((mq0) = pscrpc_msg_buf((rq)->rq_reqmsg, 0,		\
		    (q0len))) == NULL) {				\
			/* XXX tie into pscmem reap */			\
			psclog_errorx("request is NULL");		\
			*_rcp = -ENOMEM;				\
			return (-ENOMEM);				\
		}							\
	} while (0)

#define RSX_ALLOCREPN(rq, mq0, q0len, mp0, np, plens)			\
	RSX_ALLOCREPNRC(rq, mq0, q0len, mp0, np, plens,			\
	    offsetof(typeof(*(mp0)), rc))

#define RSX_ALLOCREP(rq, mq, mp)					\
	do {								\
		int _plen;						\
									\
		_plen = sizeof(*(mp));					\
		RSX_ALLOCREPN((rq), (mq), sizeof(*(mq)), (mp), 1,	\
		    &_plen);						\
	} while (0)

#define RSX_NEWREQN(imp, version, op, rq, nq, qlens, np, plens, mq0)	\
	_pfl_rsx_newreq((imp), (version), (op), &(rq), (nq), (qlens),	\
	    (np), (plens), &(mq0))

#define RSX_NEWREQ(imp, version, op, rq, mq, mp)			\
	_PFL_RVSTART {							\
		int _qlen, _plen;					\
									\
		_qlen = sizeof(*(mq));					\
		_plen = sizeof(*(mp));					\
		RSX_NEWREQN((imp), (version), (op), (rq), 1, &_qlen,	\
		    1, &_plen, (mq));					\
	} _PFL_RVEND

#define RSX_WAITREP(rq, mp)						\
	pfl_rsx_waitrep((rq), sizeof(*(mp)), &(mp))

int _pfl_rsx_newreq(struct pscrpc_import *, int, int,
	struct pscrpc_request **, int, int *, int, int *, void *);
int pfl_rsx_waitrep(struct pscrpc_request *, int, void *);

int rsx_bulkserver(struct pscrpc_request *, int, int, struct iovec *, int);
int rsx_bulkclient(struct pscrpc_request *, int, int, struct iovec *, int);

int pfl_rsx_conv2net(int, void *);
int pfl_rsx_conv2host(int, void *);

extern struct rsx_msg_portablizer	rpcmsg_portablizers[];
extern int				rpcmsg_nportablizers;

#endif /* _PFL_RSX_H_ */
