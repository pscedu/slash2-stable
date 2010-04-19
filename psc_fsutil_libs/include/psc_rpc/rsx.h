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

#ifndef _PFL_RSX_H_
#define _PFL_RSX_H_

struct pscrpc_request;
struct pscrpc_import;
struct pscrpc_bulk_desc;
struct iovec;

struct rsx_msg_conversion {
	int	rmc_offset;
	int	rmc_size;
};

#define RSX_PORT_NAME_MAX 50

struct rsx_msg_portablizer {
	char			 rmp_name[50];
	int			 rmp_nconv;
	struct rpcmsg_conv	*rmp_conv;
};

#define RSX_ALLOCREPN(rq, mq0, mp0, nreplens, replens)			\
	do {								\
		int _rc;						\
									\
		if (pscrpc_msg_size(nreplens, replens) >		\
		    (rq)->rq_rqbd->rqbd_service->srv_max_reply_size)	\
			psc_fatalx("reply size greater than max");	\
		_rc = pscrpc_pack_reply((rq), nreplens, replens, NULL);	\
		if (_rc) {						\
			psc_assert(_rc == -ENOMEM);			\
			psc_errorx("pscrpc_pack_reply failed: %s",	\
			    strerror(_rc));				\
			return (_rc);					\
		}							\
		if (((mp0) = pscrpc_msg_buf((rq)->rq_repmsg,		\
		    0, (replens)[0])) == NULL) {			\
			psc_errorx("reply is NULL");			\
			return (-ENOMEM);				\
		}							\
		(mp0)->rc = 0;						\
		if (((mq0) = pscrpc_msg_buf((rq)->rq_reqmsg,		\
		    0, sizeof(*(mq0)))) == NULL) {			\
			/* XXX psc_fatalx */				\
			psc_errorx("request is NULL");			\
			(mp0)->rc = -ENOMEM;				\
			return (-ENOMEM);				\
		}							\
	} while (0)

#define RSX_ALLOCREP(rq, mq, mp)					\
	do {								\
		int _replen;						\
									\
		_replen = sizeof(*(mp));				\
		RSX_ALLOCREPN((rq), (mq), (mp), 1, &_replen);		\
	} while (0)

#define RSX_NEWREQN(imp, version, op, rq, nqlens, qlens,		\
	    nplens, plens, mq0)						\
	_pfl_rsx_newreq((imp), (version), (op), &(rq),			\
	    (nqlens), (qlens), (nplens), (plens), &(mq0))

#define _RSX_NEWREQ(imp, version, op, rq, mq, mp)			\
	{								\
		int _reqlen, _replen;					\
									\
		_reqlen = sizeof(*(mq));				\
		_replen = sizeof(*(mp));				\
		RSX_NEWREQN((imp), (version), (op), (rq),		\
		    1, &_reqlen, 1, &_replen, (mq));			\
	}

#define RSX_NEWREQ(imp, version, op, rq, mq, mp)			\
	(_RSX_NEWREQ((imp), (version), (op), (rq), (mq), (mp)))

#define RSX_WAITREP(rq, mp)						\
	pfl_rsx_waitrep((rq), sizeof(*(mp)), &(mp))

int _pfl_rsx_newreq(struct pscrpc_import *, int, int,
	struct pscrpc_request **, int, int *, int, int *, void *);
int pfl_rsx_waitrep(struct pscrpc_request *, int, void *);

int rsx_bulkserver(struct pscrpc_request *, struct pscrpc_bulk_desc **,
	int, int, struct iovec *, int);
int rsx_bulkclient(struct pscrpc_request *, struct pscrpc_bulk_desc **,
	int, int, struct iovec *, int);

int pfl_rsx_conv2net(int, void *);
int pfl_rsx_conv2host(int, void *);

extern struct rsx_msg_portablizer	rpcmsg_portablizers[];
extern int				rpcmsg_nportablizers;

#endif /* _PFL_RSX_H_ */
