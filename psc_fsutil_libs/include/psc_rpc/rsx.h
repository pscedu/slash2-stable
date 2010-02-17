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

#define RSX_ALLOCREP(rq, mq, mp)					\
	do {								\
		int __rc, __psz;					\
									\
		__psz = sizeof(*(mp));					\
		if (__psz >						\
		    (rq)->rq_rqbd->rqbd_service->srv_max_reply_size)	\
			psc_fatalx("reply size greater than max");	\
		__rc = psc_pack_reply((rq), 1, &__psz, NULL);		\
		if (__rc) {						\
			psc_assert(__rc == -ENOMEM);			\
			psc_errorx("psc_pack_reply failed: %s",		\
			    strerror(__rc));				\
			return (__rc);					\
		}							\
		if (((mp) = psc_msg_buf((rq)->rq_repmsg,		\
		    0, __psz)) == NULL) {				\
			psc_errorx("reply is NULL");			\
			return (-ENOMEM);				\
		}							\
		(mp)->rc = 0;						\
		if (((mq) = psc_msg_buf((rq)->rq_reqmsg,		\
		    0, sizeof(*(mq)))) == NULL) {			\
			/* XXX psc_fatalx */				\
			psc_errorx("request is NULL");			\
			(mp)->rc = -ENOMEM;				\
			return (-ENOMEM);				\
		}							\
	} while (0)

#define RSX_NEWREQ(imp, version, op, rq, mq, mp)			\
	pfl_rsx_newreq((imp), (version), (op), sizeof(*(mq)),		\
	    sizeof(*(mp)), &(rq), &(mq))

#define RSX_WAITREP(rq, mp)						\
	pfl_rsx_waitrep((rq), sizeof(*(mp)), &(mp))

int pfl_rsx_newreq(struct pscrpc_import *, int, int, int, int,
	struct pscrpc_request **, void *);
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
