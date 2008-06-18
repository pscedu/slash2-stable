/* $Id$ */

struct pscrpc_request;
struct pscrpc_import;
struct pscrpc_bulk_desc;
struct iovec;

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
			return (0);					\
		}							\
	} while (0)

#define RSX_NEWREQ(imp, version, op, rq, mq, mp)			\
	rsx_newreq((imp), (version), (op), sizeof(*(mq)),		\
	    sizeof(*(mp)), &(rq), &(mq))

#define RSX_WAITREP(rq, mp)						\
	rsx_waitrep((rq), sizeof(*(mp)), &(mp))

int rsx_newreq(struct pscrpc_import *, int, int, int, int,
	struct pscrpc_request **, void *);
int rsx_waitrep(struct pscrpc_request *, int, void *);
int rsx_bulkserver(struct pscrpc_request *, struct pscrpc_bulk_desc **,
	int, int, struct iovec *, int);
int rsx_bulkclient(struct pscrpc_request *, struct pscrpc_bulk_desc **,
	int, int, struct iovec *, int);
