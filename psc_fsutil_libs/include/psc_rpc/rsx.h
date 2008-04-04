/* $Id$ */

struct pscrpc_request;
struct pscrpc_import;
struct pscrpc_bulk_desc;
struct iovec;

#define GENERIC_REPLY(rq, prc, mp)							\
	do {										\
		int _rc, _size;								\
											\
		_size = sizeof(*(mp));							\
		if (_size > (rq)->rq_rqbd->rqbd_service->srv_max_reply_size)		\
			psc_fatalx("reply size greater than max");			\
		_rc = psc_pack_reply((rq), 1, &_size, NULL);				\
		if (_rc) {								\
			psc_assert(_rc == -ENOMEM);					\
			psc_errorx("psc_pack_reply failed: %s", strerror(_rc));		\
			return (_rc);							\
		}									\
		(mp) = psc_msg_buf((rq)->rq_repmsg, 0, _size);				\
		if ((mp) == NULL) {							\
			psc_errorx("connect repbody is null");				\
			return (-ENOMEM);						\
		}									\
		(mp)->rc = (prc);							\
		return (0);								\
	} while (0)

#define GET_CUSTOM_REPLY_SZ(rq, mp, sz)							\
	do {										\
		int _rc, _size;								\
											\
		_size = sz;								\
		if (_size > (rq)->rq_rqbd->rqbd_service->srv_max_reply_size)		\
			psc_fatalx("reply size greater than max");			\
		_rc = psc_pack_reply((rq), 1, &_size, NULL);				\
		if (_rc) {								\
			psc_assert(_rc == -ENOMEM);					\
			psc_errorx("psc_pack_reply failed: %s", strerror(_rc));		\
			return (_rc);							\
		}									\
		(mp) = psc_msg_buf((rq)->rq_repmsg, 0, _size);				\
		if ((mp) == NULL) {							\
			psc_errorx("connect repbody is null");				\
			return (-ENOMEM);						\
		}									\
	} while (0)

#define GET_CUSTOM_REPLY(rq, mp) GET_CUSTOM_REPLY_SZ(rq, mp, sizeof(*(mp)))

#define GET_GEN_REQ(rq, mq, mp)								\
	do {										\
		(mq) = psc_msg_buf((rq)->rq_reqmsg, 0, sizeof(*(mq)));			\
		if ((mq) == NULL) {							\
			psc_warnx("reqbody is null");					\
			GENERIC_REPLY((rq), -ENOMSG, (mp));				\
		}									\
	} while (0)

int rsx_newreq(struct pscrpc_import *, int, int, int, int, struct pscrpc_request **, void *);
int rsx_getrep(struct pscrpc_request *, int, void *);
int rsx_bulkgetsink(struct pscrpc_request *, struct pscrpc_bulk_desc **, int, struct iovec *, int);
int rsx_bulkgetsource(struct pscrpc_request *, struct pscrpc_bulk_desc **, int, struct iovec *, int);
