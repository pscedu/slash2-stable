/* $Id$ */

#ifndef _PFL_RPC_H_
#define _PFL_RPC_H_

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <sys/uio.h>
#include <asm/param.h>

#include <time.h>
#include <unistd.h>

#include "libcfs/kp30.h"
#include "libcfs/libcfs.h"
#include "lnet/api.h"
#include "lnet/types.h"

#include "pfl/types.h"
#include "psc_ds/hash2.h"
#include "psc_ds/list.h"
#include "psc_ds/listcache.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

#define PSCRPC_MD_OPTIONS	0
#define BULK_GET_SOURCE		0
#define BULK_PUT_SINK		1
#define BULK_GET_SINK		2
#define BULK_PUT_SOURCE		3

#define PSC_RPC_MSG_REQUEST	4711
#define PSC_RPC_MSG_ERR		4712
#define PSC_RPC_MSG_REPLY	4713

#define PSCRPC_MSG_MAGIC	0x0BD00BD0
#if !CATAMOUNT_CLIENT
#define PSCRPC_MSG_VERSION	0x00000003
#else
#define PSCRPC_MSG_VERSION	0x00000004
#endif

#define ZOBD_FREE(ptr, size)	PSCFREE(ptr)
#define ZOBD_ALLOC(ptr, size)	((ptr) = PSCALLOC(size))

#ifndef PAGE_SIZE
#define PAGE_SIZE		4096
#endif

#define PSCRPC_MAX_BRW_SIZE	LNET_MTU
#define PSCRPC_MAX_BRW_PAGES	(PSCRPC_MAX_BRW_SIZE/PAGE_SIZE)
#define CURRENT_SECONDS		time(NULL)
#define PSCNET_SERVER		0xff	/* differentiate client and server for ni init */
#define PSCNET_CLIENT		0x0f
#define PSC_SVR_PID		54321
#define PSC_NIDSTR_SIZE		32
#define ZOBD_TIMEOUT		60

extern lnet_handle_eq_t		pscrpc_eq_h;
extern struct psclist_head	pscrpc_wait_callbacks;

struct pscrpc_handle {
	uint64_t		cookie;
};
#define DEAD_HANDLE_MAGIC	UINT64_C(0xdeadbeefcafebabe)

struct l_wait_info {
	time_t			  lwi_timeout;
	long			  lwi_interval;
	int			(*lwi_on_timeout)(void *);
	void			(*lwi_on_signal)(void *);
	void			 *lwi_cb_data;
};

struct pscrpc_wait_callback {
	struct psclist_head	  llwc_lentry;
	int			(*llwc_fn)(void *arg);
	void			 *llwc_arg;
};

enum psc_rq_phase {
	ZRQ_PHASE_NEW		= 0xebc0de00,
	ZRQ_PHASE_RPC		= 0xebc0de01,
	ZRQ_PHASE_BULK		= 0xebc0de02,
	ZRQ_PHASE_INTERPRET	= 0xebc0de03,
	ZRQ_PHASE_COMPLETE	= 0xebc0de04,
};

enum psc_imp_state {
	PSC_IMP_CLOSED		= 1,
	PSC_IMP_NEW		= 2,
	PSC_IMP_DISCON		= 3,
	PSC_IMP_CONNECTING	= 4,
	PSC_IMP_REPLAY		= 5,
	PSC_IMP_REPLAY_LOCKS	= 6,
	PSC_IMP_REPLAY_WAIT	= 7,
	PSC_IMP_RECOVER		= 8,
	PSC_IMP_FULL		= 9,
	PSC_IMP_EVICTED		= 10
};

struct psc_uuid {
	//uint8_t uuid[40];
	uint8_t uuid[8];
};

struct pscrpc_cb_id {
	void	(*cbid_fn)(lnet_event_t *ev);	/* specific callback fn */
	void	 *cbid_arg;			/* additional arg */
};

struct pscrpc_export {
	psc_spinlock_t		  exp_lock;
	struct pscrpc_handle	  exp_handle;
	atomic_t		  exp_refcount;
	atomic_t		  exp_rpc_count;
	struct pscrpc_connection *exp_connection;
	int			  exp_failed;
	void			(*exp_hldropf)(void *);
	void			 *exp_private; /* app-specific data */
//	struct psclist_head       exp_outstanding_replies;
};

struct pscrpc_import;

struct pscrpc_connection {
	struct psclist_head	c_link;
	lnet_nid_t		c_self;
	lnet_process_id_t	c_peer;
	struct psc_uuid		c_remote_uuid;
	atomic_t		c_refcount;
	union {
		struct pscrpc_export *cu_exp;		/* meaningful only on server */
		struct pscrpc_import *cu_imp;
	} c_u;
#define c_exp c_u.cu_exp
#define c_imp c_u.cu_imp
};

struct pscrpc_client {
	uint32_t		 cli_request_portal;
	uint32_t		 cli_reply_portal;
	char			*cli_name;
};

struct pscrpc_request_pool {
	psc_spinlock_t		  prp_lock;
	struct psclist_head	  prp_req_list;		/* list of request structs */
	int			  prp_rq_size;
	void			(*prp_populate)(struct pscrpc_request_pool *, int);
};

struct pscrpc_import {
	struct pscrpc_connection *imp_connection;
	struct pscrpc_client     *imp_client;
	struct psclist_head       imp_sending_list;  /* in-flight? */
	struct psclist_head       imp_uncomitted;    /* post send  */
	struct psclist_head       imp_rpcsets;       /* list of sets */
	size_t                    imp_nsets;
	psc_spinlock_t            imp_lock;
	atomic_t                  imp_inflight;
	atomic_t                  imp_refcount;
	int                       imp_generation;
	int                       imp_conn_cnt;
	int                       imp_max_retries;
	enum psc_imp_state        imp_state;
	struct pscrpc_handle      imp_remote_handle;
	uint64_t                  imp_last_replay_transno;
	uint64_t                  imp_last_transno_checked; /* optimize */
	uint64_t                  imp_peer_committed_transno;
	struct psc_waitq          imp_recovery_waitq;
	struct psclist_head       imp_delayed_list;
	unsigned int              imp_invalid:1,
				  imp_server_timeout:1,
				  imp_deactive:1,
				  imp_replayable:1,
				  imp_force_verify:1,
				  imp_igntimeout:1,
				  imp_failed:1;
#if 0
	unsigned int
		imp_dlm_fake:1,
		imp_initial_recov:1, imp_initial_recov_bk:1,
		imp_pingable:1,
		imp_resend_replay:1
#endif
};

struct pscrpc_async_args {
	/* Scratchpad for passing args to completion interpreter. Users
	 * cast to the struct of their choosing, and LASSERT that this is
	 * big enough.  For _tons_ of context, ZOBD_ALLOC a struct and store
	 * a pointer to it here.  The pointer_arg ensures this struct is at
	 * least big enough for that. */
	uint64_t space[2];
	void    *pointer_arg[5];
};

struct pscrpc_request_set;
typedef int (*set_interpreter_func)(struct pscrpc_request_set *, void *, int);

struct pscrpc_request_set {
	struct psclist_head  set_requests;
	int                  set_remaining;
	struct psc_waitq     set_waitq;        /* I block here          */
	set_interpreter_func set_interpret;    /* callback function     */
	void                *set_arg;          /* callback pointer      */
	psc_spinlock_t       set_lock;
	int                  set_flags;
#if 0
	//psc_spinlock_t     rqset_lock;
	//psclist_cache_t    rqset_reqs;       /* the request list      */
#endif
};

/* set flags */
#define PSCRPC_SETF_CHECKING	(1 << 0)

struct pscrpc_bulk_desc {
	unsigned int              bd_success:1;    /* completed successfully */
	unsigned int              bd_network_rw:1; /* accessible to network  */
	unsigned int              bd_type:2;       /* {put,get}{source,sink} */
	unsigned int              bd_registered:1; /* client side            */
	psc_spinlock_t            bd_lock;         /* serialise w/ callback  */
	int                       bd_import_generation;
	struct pscrpc_import     *bd_import;       /* client only            */
	struct pscrpc_connection *bd_connection;   /* server only            */
	struct pscrpc_export     *bd_export;       /* server only            */
	struct pscrpc_request    *bd_req;          /* associated request     */
	struct psc_waitq          bd_waitq;        /* server side only WQ    */
	int                       bd_iov_count;    /* # entries in bd_iov    */
	int                       bd_max_iov;      /* alloc'd size of bd_iov */
	int                       bd_nob;          /* # bytes covered        */
	int                       bd_nob_transferred; /* # bytes GOT/PUT     */
	uint64_t                  bd_last_xid;     /* track xid for retry    */
	uint32_t                  bd_portal;       /* which portal           */
	struct pscrpc_cb_id       bd_cbid;         /* network callback info  */
	lnet_handle_md_t          bd_md_h;         /* associated MD          */
	lnet_md_iovec_t           bd_iov[0];       /* must be last           */
};

struct psc_msg {
	struct pscrpc_handle handle;
	uint32_t magic;
	uint32_t type;
	uint32_t version;
	uint32_t opc;
	uint64_t last_xid;
	uint64_t last_committed;
	uint64_t transno;
	uint32_t status;
	uint32_t flags;
	uint32_t conn_cnt;
	uint32_t bufcount;
	uint32_t buflens[0];
};

struct pscrpc_reply_state;
struct pscrpc_request_buffer_desc;
struct pscrpc_nbrequests;

struct pscrpc_peer_qlen {
	atomic_t		 qlen;
	lnet_process_id_t	 id;
	struct psc_hashent	 hentry;
};

struct pscrpc_request {
	int			rq_type;
	int			rq_status;
	int			rq_retry;
	int			rq_timeout;         /* time to wait for reply (seconds) */
	int			rq_request_portal;
	int			rq_reply_portal;
	int			rq_nob_received;    /* client-side # reply bytes received */
	int			rq_reqlen;
	int			rq_replen;
	int			rq_import_generation;
	time_t			rq_sent;
	psc_spinlock_t		rq_lock;
	uint64_t		rq_transno;
	uint64_t		rq_xid;
	uint64_t		rq_history_seq;
	unsigned int		rq_intr:1,
				rq_replied:1,
				rq_err:1,
				rq_timedout:1,
				rq_resend:1,
				rq_restart:1,
				rq_replay:1,
				rq_no_resend:1,
				rq_waiting:1,
				rq_receiving_reply:1,
				rq_no_delay:1,
				rq_net_err:1;
	atomic_t		rq_refcount; /* client-side refcnt for SENT race */
	atomic_t		rq_retries;  /* count retries */
	atomic_t               *rq_compl_cntr;
	lnet_process_id_t	rq_peer;
	lnet_nid_t		rq_self;
	enum   psc_rq_phase         rq_phase; /* one of RQ_PHASE_* */
	enum   psc_imp_state        rq_send_state;
	struct psclist_head         rq_list_entry;
	struct psclist_head         rq_set_chain_lentry;
	struct psclist_head         rq_history_lentry;
	struct pscrpc_import       *rq_import;     /* client side */
	struct pscrpc_request_pool *rq_pool;
	struct pscrpc_connection   *rq_conn;       /* svr-side */
	struct pscrpc_export       *rq_export;
	struct psc_thread          *rq_svc_thread; /* who's servicing req */
	struct pscrpc_request_set  *rq_set;
	struct pscrpc_nbrequests   *rq_nbreqs;
	/* client+server request */
	struct psc_msg             *rq_reqmsg;
	struct psc_msg             *rq_repmsg;
	/* request and reply callbacks */
	struct pscrpc_cb_id         rq_req_cbid;
	struct pscrpc_cb_id         rq_reply_cbid;
	struct pscrpc_bulk_desc    *rq_bulk;    /* attach bulk */
	int			  (*rq_interpret_reply)(struct pscrpc_request *,
					struct pscrpc_async_args *);
	struct pscrpc_async_args    rq_async_args;      /* Async completion context */
	lnet_handle_md_t            rq_req_md_h;
	/* client-only incoming reply */
	lnet_handle_md_t            rq_reply_md_h;
	struct psc_waitq            rq_reply_waitq;
	/* server-side... */
	struct timeval              rq_arrival_time; /* request arrival time */
	struct pscrpc_reply_state  *rq_reply_state;  /* separated reply state */
	struct pscrpc_request_buffer_desc *rq_rqbd;  /* incoming req  buffer*/
	struct pscrpc_peer_qlen	   *rq_peer_qlen;
};

/* Each service installs its own request handler */
typedef int (*svc_handler_t)(struct pscrpc_request *req);

/* Server side request management */
struct pscrpc_service {
	int srv_max_req_size;      /* max request sz to recv  */
	int srv_max_reply_size;    /* biggest reply to send   */
	int srv_buf_size;          /* size of buffers         */
	int srv_nbuf_per_group;
	int srv_watchdog_timeout;  /* soft watchdog timeout, in ms */
	int srv_n_active_reqs;     /* # reqs being served */
	int srv_n_difficult_replies; /* # 'difficult' replies */
	int srv_nthreads;          /* # running threads */
	int srv_failure;           /* we've failed, no new requests */
	int srv_rqbd_timeout;
	int srv_n_queued_reqs;     /* # reqs waiting to be served */
	int srv_nrqbd_receiving;   /* # posted request buffers */
	int srv_n_history_rqbds;   /* # request buffers in history */
	int srv_max_history_rqbds; /* max # request buffers in history */
	int srv_nbufs;             /* total # req buffer descs allocated */
	int srv_count_peer_qlens:1;
	uint32_t srv_req_portal;
	uint32_t srv_rep_portal;
	uint64_t srv_request_seq;       /* next request sequence # */
	uint64_t srv_request_max_cull_seq; /* highest seq culled from history */
	atomic_t            srv_outstanding_replies;
	struct psclist_head srv_lentry;      /* chain thru all services */
	struct psclist_head srv_threads;
	struct psclist_head srv_request_queue;   /* reqs waiting     */
	struct psclist_head srv_request_history; /* request history */
	struct psclist_head srv_idle_rqbds;      /* buffers to be reposted */
	struct psclist_head srv_active_rqbds;    /* req buffers receiving */
	struct psclist_head srv_history_rqbds;   /* request buffer history */
	struct psclist_head srv_active_replies;  /* all the active replies */
	struct psclist_head srv_reply_queue;     /* replies waiting  */
	struct psclist_head srv_free_rs_list;
	struct psc_waitq    srv_free_rs_waitq;
	struct psc_waitq    srv_waitq; /* all threads sleep on this. This
					* wait-queue is signalled when new
					* incoming request arrives and when
					* difficult reply has to be handled. */
	struct psc_hashtbl srv_peer_qlentab;

	psc_spinlock_t srv_lock;
	svc_handler_t  srv_handler;
	char          *srv_name;  /* only statically allocated strings here,
				   * we don't clean them */
	/*
	 * if non-NULL called during thread creation (pscrpc_start_thread())
	 * to initialize service specific per-thread state.
	 */
	int (*srv_init)(struct psc_thread *);
	/*
	 * if non-NULL called during thread shutdown (pscrpc_main()) to
	 * destruct state created by ->srv_init().
	 */
	void (*srv_done)(struct psc_thread *);
};

struct pscrpc_request_buffer_desc {
	int                    rqbd_refcount;
	char                  *rqbd_buffer;
	lnet_handle_md_t       rqbd_md_h;
	struct psclist_head    rqbd_lentry;
	struct psclist_head    rqbd_reqs;
	struct pscrpc_service *rqbd_service;
	struct pscrpc_cb_id    rqbd_cbid;
	struct pscrpc_request  rqbd_req;
};

struct pscrpc_reply_state {
	struct pscrpc_cb_id    rs_cb_id;       /* reply callback */
	struct psclist_head    rs_list_entry;
	int                    rs_size;
	uint64_t               rs_xid;
	unsigned int           rs_difficult:1; /* ACK/commit stuff */
	unsigned int           rs_scheduled:1; /* being handled? */
	unsigned int           rs_scheduled_ever:1;/* any schedule attempts? */
	unsigned int           rs_handled:1;   /* been handled yet? */
	unsigned int           rs_on_net:1;    /* reply_out_callback pending?*/
	unsigned int           rs_prealloc:1;  /* rs from prealloc list */
	atomic_t               rs_refcount;
	lnet_handle_md_t       rs_md_h;
	struct pscrpc_service *rs_service;     /* backpointer to my service */
	struct psc_msg         rs_msg;         /* msg struct -- MUST BE LAST MEMBER */
};

/*
 * Non-blocking request sets
 */
typedef int (*nbreq_callback)(struct pscrpc_request *,
			      struct pscrpc_async_args *);

struct pscrpc_nbreqset {
	struct pscrpc_request_set	nb_reqset;
	nbreq_callback			nb_callback;
	atomic_t			nb_outstanding;
};

#define PSCPRC_SET_INIT(v, cb, cbarg)					\
	{ PSCLIST_HEAD_INIT((v).set_requests), 0, PSC_WAITQ_INIT,	\
	    (cb), (cbarg), LOCK_INITIALIZER, 0 }

#define PSCRPC_NBREQSET_INIT(v, setcb, rqcb)				\
	{ PSCPRC_SET_INIT((v).nb_reqset, (setcb), NULL), (rqcb),	\
	    ATOMIC_INIT(0) }

struct pscrpc_nbreqset *
	 nbreqset_init(set_interpreter_func, nbreq_callback);
void	 nbreqset_add(struct pscrpc_nbreqset *, struct pscrpc_request *);
int	 nbrequest_reap(struct pscrpc_nbreqset *nbs);
int	 nbrequest_flush(struct pscrpc_nbreqset *);

struct pscrpc_bulk_desc *
	 pscrpc_prep_bulk_exp(struct pscrpc_request *, int, int, int);
void	 psc_free_reply_state(struct pscrpc_reply_state *);
void	 pscrpc_req_finished(struct pscrpc_request *);
void	 pscrpc_free_bulk(struct pscrpc_bulk_desc *);
int	 psc_pack_reply(struct pscrpc_request *, int, int *, char **);
int	 pscrpc_put_connection(struct pscrpc_connection *);
void	 pscrpc_fill_bulk_md(lnet_md_t *, struct pscrpc_bulk_desc *);

/*  events.c */
lnet_pid_t psc_get_pid(void);
void	 request_in_callback(lnet_event_t *);
void	 request_out_callback(lnet_event_t *);
void	 client_bulk_callback (lnet_event_t *);
void	 server_bulk_callback (lnet_event_t *);
void	 reply_in_callback(lnet_event_t *);
void	 reply_out_callback(lnet_event_t *);
void	 pscrpc_deregister_wait_callback(void *);
int	 pscrpc_check_events(int);
int	 pscrpc_wait_event(int);
int	 pscrpc_ni_init(int);
int	 pscrpc_init_portals(int);

/* packgeneric.c */
int	 psc_msg_size(int, int *);
int	 psc_msg_swabbed(struct psc_msg *);
int	 psc_pack_request (struct pscrpc_request *, int, int *, char **);
int	 psc_pack_reply (struct pscrpc_request *, int, int *, char **);
int	 psc_unpack_msg(struct psc_msg *, int);
void	*psc_msg_buf(struct psc_msg *, int, int);
int	 psc_msg_buflen(struct psc_msg *, int);
char	*psc_msg_string(struct psc_msg *, int, int);

/* niobuf.c */
void	 psc_free_reply_state(struct pscrpc_reply_state *rs);
int	 psc_send_rpc(struct pscrpc_request *request, int noreply);
void	 pscrpc_abort_bulk(struct pscrpc_bulk_desc *desc);
int	 pscrpc_error(struct pscrpc_request *req);
void	 pscrpc_fill_bulk_md(lnet_md_t *md, struct pscrpc_bulk_desc *desc);
void	 pscrpc_free_bulk(struct pscrpc_bulk_desc *desc);
void	 pscrpc_free_req(struct pscrpc_request *request);
int	 pscrpc_register_bulk(struct pscrpc_request *req);
int	 pscrpc_register_rqbd(struct pscrpc_request_buffer_desc *rqbd);
int	 pscrpc_reply(struct pscrpc_request *req);
void	 pscrpc_req_finished(struct pscrpc_request *request);
int	 pscrpc_send_reply(struct pscrpc_request *req, int may_be_difficult);
int	 pscrpc_start_bulk_transfer(struct pscrpc_bulk_desc *desc);
void	 pscrpc_unregister_bulk(struct pscrpc_request *req);
void	 pscrpc_abort_inflight(struct pscrpc_import *imp);
void	 pscrpc_drop_conns(lnet_process_id_t *);

#define psc_nid2str(addr, buf)	libcfs_nid2str2((addr), (buf))
#define psc_id2str(addr, buf)	libcfs_id2str2((addr), (buf))

struct pscrpc_connection *
	 pscrpc_get_connection(lnet_process_id_t, lnet_nid_t, struct psc_uuid *);
struct pscrpc_connection *
	 pscrpc_lookup_conn_locked(lnet_process_id_t, lnet_nid_t);
struct pscrpc_connection *
	 pscrpc_connection_addref(struct pscrpc_connection *);

/* rpcclient.c */
int	 pscrpc_expire_one_request(struct pscrpc_request *);
struct pscrpc_request *
	 pscrpc_prep_req(struct pscrpc_import *, uint32_t, int, int, int *, char **);
struct pscrpc_bulk_desc *
	 pscrpc_prep_bulk_imp(struct pscrpc_request *, int, int, int);
struct pscrpc_request *
	 pscrpc_request_addref(struct pscrpc_request *);
void	 pscrpc_import_put(struct pscrpc_import *);
struct pscrpc_import *
	 pscrpc_new_import(void);
int	 pscrpc_queue_wait(struct pscrpc_request *);
struct pscrpc_request_set *
	 pscrpc_prep_set(void);
void	 pscrpc_set_init(struct pscrpc_request_set *);
int	 pscrpc_push_req(struct pscrpc_request *);
void	 pscrpc_set_add_new_req(struct pscrpc_request_set *, struct pscrpc_request *);
int	 pscrpc_check_set(struct pscrpc_request_set *, int);
int	 pscrpc_set_finalize(struct pscrpc_request_set *, int, int);
int	 pscrpc_set_wait(struct pscrpc_request_set *);
void	 pscrpc_set_destroy(struct pscrpc_request_set *);
void	 pscrpc_set_lock(struct pscrpc_request_set *);

static __inline int
pscrpc_bulk_active(struct pscrpc_bulk_desc *desc)
{
	int rc, l;

	l = reqlock(&desc->bd_lock);
	rc = desc->bd_network_rw;
	ureqlock(&desc->bd_lock, l);
	return (rc);
}

/* service.c */
int	 target_send_reply_msg(struct pscrpc_request *, int, int);
void	 pscrpc_fail_import(struct pscrpc_import *, uint32_t);

/* service.c done */

static __inline void
pscrpc_rs_addref(struct pscrpc_reply_state *rs)
{
	LASSERT(atomic_read(&rs->rs_refcount) > 0);
	atomic_inc(&rs->rs_refcount);
}

static __inline void
pscrpc_rs_decref(struct pscrpc_reply_state *rs)
{
	LASSERT(atomic_read(&rs->rs_refcount) > 0);
	if (atomic_dec_and_test(&rs->rs_refcount))
		psc_free_reply_state(rs);
}

static __inline void
psc_str2uuid(struct psc_uuid *uuid, char *tmp)
{
	strncpy((char *)uuid->uuid, tmp, sizeof(uuid->uuid));
	uuid->uuid[sizeof(uuid->uuid) - 1] = '\0';
}

/* Flags that are operation-specific go in the top 16 bits. */
#define MSG_OP_FLAG_MASK   0xffff0000
#define MSG_OP_FLAG_SHIFT  16

/* Flags that apply to all requests are in the bottom 16 bits */
#define MSG_GEN_FLAG_MASK      0x0000ffff
#define MSG_LAST_REPLAY        1
#define MSG_RESENT             2
#define MSG_REPLAY             4

static __inline int
psc_msg_get_flags(struct psc_msg *msg)
{
	return (msg->flags & MSG_GEN_FLAG_MASK);
}

static __inline void
psc_msg_add_flags(struct psc_msg *msg, int flags)
{
	msg->flags |= MSG_GEN_FLAG_MASK & flags;
}

static __inline void
psc_msg_set_flags(struct psc_msg *msg, int flags)
{
	msg->flags &= ~MSG_GEN_FLAG_MASK;
	psc_msg_add_flags(msg, flags);
}

static __inline void
psc_msg_clear_flags(struct psc_msg *msg, int flags)
{
	msg->flags &= ~(MSG_GEN_FLAG_MASK & flags);
}

static __inline int
psc_msg_get_op_flags(struct psc_msg *msg)
{
	return (msg->flags >> MSG_OP_FLAG_SHIFT);
}

static __inline void
psc_msg_add_op_flags(struct psc_msg *msg, int flags)
{
	msg->flags |= ((flags & MSG_GEN_FLAG_MASK) << MSG_OP_FLAG_SHIFT);
}

static __inline void
psc_msg_set_op_flags(struct psc_msg *msg, int flags)
{
	msg->flags &= ~MSG_OP_FLAG_MASK;
	psc_msg_add_op_flags(msg, flags);
}


static __inline int
pscrpc_client_receiving_reply (struct pscrpc_request *req)
{
	int           rc;

	spinlock(&req->rq_lock);
	rc = req->rq_receiving_reply;
	freelock(&req->rq_lock);
	return (rc);
}

static __inline int
pscrpc_client_replied(struct pscrpc_request *req)
{
	int           rc;

	spinlock(&req->rq_lock);
	rc = req->rq_replied;
	freelock(&req->rq_lock);
	return (rc);
}

static __inline void
pscrpc_wake_client_req(struct pscrpc_request *req)
{
	if (req->rq_set == NULL)
		psc_waitq_wakeall(&req->rq_reply_waitq);
	else
		psc_waitq_wakeall(&req->rq_set->set_waitq);
}

/* NB: LWI_TIMEOUT ignores signals completely */
#define LWI_TIMEOUT(time, cb, data)             \
((struct l_wait_info) {                         \
	.lwi_timeout    = time,                 \
	.lwi_on_timeout = cb,                   \
	.lwi_cb_data    = data,                 \
	.lwi_interval   = 0                     \
})

#define LWI_TIMEOUT_INTERVAL(time, interval, cb, data)  \
((struct l_wait_info) {                                 \
	.lwi_timeout    = time,                         \
	.lwi_on_timeout = cb,                           \
	.lwi_cb_data    = data,                         \
	.lwi_interval   = interval                      \
})

#define LWI_TIMEOUT_INTR(time, time_cb, sig_cb, data)                        \
((struct l_wait_info) {                                                      \
	.lwi_timeout    = time,                                              \
	.lwi_on_timeout = time_cb,                                           \
	.lwi_on_signal  = sig_cb,                                            \
	.lwi_cb_data    = data,                                              \
	.lwi_interval   = 0                                                  \
})

#define LWI_INTR(cb, data)  LWI_TIMEOUT_INTR(0, NULL, (cb), (data))

#define PSC_FATAL_SIGS (sigmask(SIGKILL) | sigmask(SIGINT)  |              \
			sigmask(SIGTERM) | sigmask(SIGQUIT) |              \
			sigmask(SIGALRM))

/**
 * __psc_server_wait_event - implement a timed wait using waitqs and pthread_cond_timedwait
 * @wq: the waitq to block on
 * @condition: condition to check on
 * @info: the timeout info strict (l_wait_info)
 * @ret: the return val
 * @excl: unused
 * @lck: optional spinlock used for waitq - see psc_util/waitq.h
 */

#define SVR_TIMEOUT 60
#define SVR_SHORT_TIMEO 1
#define  __psc_server_wait_event(wq, condition, info, ret, excl, lck)	\
	do {								\
		time_t __now       = time(NULL);			\
		time_t __timeout   = SVR_TIMEOUT;			\
		time_t __then      = __now;				\
		int    __timed_out = 0;					\
		struct timespec abstime = {0, 0};			\
									\
		ret = 0;						\
									\
		while (!(condition)) {					\
			if (__timeout)					\
				abstime.tv_sec = SVR_SHORT_TIMEO + __now; \
			abstime.tv_nsec = 0;				\
			ret = psc_waitq_timedwait(wq, lck, &abstime);	\
			if (ret && (ret != ETIMEDOUT)) {		\
				ret = -ret;				\
				break;					\
			} else						\
				ret = 0;				\
									\
			if (condition)					\
				break;					\
									\
			if (!__timed_out && info->lwi_timeout != 0) {	\
				__now = time(NULL);			\
				__timeout -= __now - __then;		\
				__then = __now;				\
									\
				if (__timeout > 0) continue;		\
				__timeout = 0;				\
				__timed_out = 1;			\
				if (info->lwi_on_timeout == NULL ||	\
				    info->lwi_on_timeout(info->lwi_cb_data)) { \
					ret = -ETIMEDOUT;		\
					break;				\
				}					\
			} else						\
				__now = time(NULL);			\
		}							\
	} while (0)


/**
 * __psc_client_wait_event - the below call is for clients.  Clients are
 *	single threaded due to catamount/panther.  This means that clients must
 *	block in LNetEQPoll - this occurs in liblustre_wait_event().  A similar
 *	model can be used for server threads so long as liblustre_wait_event()
 *	is replaced with something that uses timed waitq's.
 */
#define pscrpc_timeout 1
#define __psc_client_wait_event(wq, condition, info, ret, excl)		\
	do {								\
		time_t __timeout = info->lwi_timeout;			\
		long __now;						\
		long __then = 0;					\
		int  __timed_out = 0;					\
		int  __interval = pscrpc_timeout;			\
									\
		ret = 0;						\
		if (condition)						\
			break;						\
									\
		if (__timeout != 0)					\
			__then = time(NULL);				\
		if (__timeout && __timeout < __interval)		\
			__interval = __timeout;				\
		if (info->lwi_interval && info->lwi_interval < __interval) \
			__interval = info->lwi_interval;		\
									\
		for (; !(condition); (ret) = 0) {			\
			ret = pscrpc_wait_event(__interval);		\
			if (0<ret) ret=0; /* preserve the previous semantics */	\
			if (condition){					\
				if (ret){				\
					ret = 0; /* if it's true now */ \
					/* don't claim timeout */	\
				}					\
				break;					\
			}						\
			if (-ETIMEDOUT==ret) break;			\
			if (!__timed_out && info->lwi_timeout != 0) {	\
				__now = time(NULL);			\
				__timeout -= __now - __then;		\
				__then = __now;				\
									\
				if (__timeout > 0)			\
					continue;			\
									\
				__timeout = 0;				\
				__timed_out = 1;			\
				if (info->lwi_on_timeout == NULL ||	\
				    info->lwi_on_timeout(info->lwi_cb_data)) { \
					ret = -ETIMEDOUT;		\
					break;				\
				}					\
			}						\
		}							\
	} while (0)

#ifdef HAVE_LIBPTHREAD
# define psc_cli_wait_event(wq, condition, info)			\
	psc_svr_wait_event((wq), (condition), (info), NULL)
#else
# define psc_cli_wait_event(wq, condition, info)			\
	({								\
		int                 __ret;				\
		struct l_wait_info *__info = (info);			\
									\
		__psc_client_wait_event((wq), (condition), __info,	\
		    __ret, 0);						\
		__ret;							\
	})
#endif

#define psc_svr_wait_event(wq, condition, info, lck)			\
	({								\
		int                 __ret;				\
		struct l_wait_info *__info = (info);			\
									\
		__psc_server_wait_event((wq), (condition), __info,	\
		    __ret, 0, (lck));					\
		__ret;							\
	})

#endif /* _PFL_RPC_H_ */
