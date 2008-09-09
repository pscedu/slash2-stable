/* $Id$ */

#ifndef _PFL_RPC_H_
#define _PFL_RPC_H_

#undef list_head
#undef LIST_HEAD_INIT
#undef LIST_ENTRY_INIT
#undef LIST_HEAD
#undef INIT_LIST_HEAD
#undef INIT_LIST_ENTRY

#undef list_add
#undef list_add_tail
#undef list_del
#undef list_del_init
#undef list_empty
#undef list_splice
#undef list_entry
#undef list_for_each
#undef list_for_each_safe

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

/* Mask cfs internal list definitions */
#undef list_head
#undef LIST_HEAD_INIT
#undef LIST_ENTRY_INIT
#undef LIST_HEAD
#undef INIT_LIST_HEAD
#undef INIT_LIST_ENTRY
#undef list_add
#undef list_add_tail
#undef list_del
#undef list_del_init
#undef list_empty
#undef list_splice
#undef list_entry
#undef list_for_each
#undef list_for_each_safe

#include "psc_types.h"
#include "psc_ds/list.h"
#include "psc_ds/listcache.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

#define PSCRPC_MD_OPTIONS 0
#define BULK_GET_SOURCE    0
#define BULK_PUT_SINK      1
#define BULK_GET_SINK      2
#define BULK_PUT_SOURCE    3

#define PSC_RPC_MSG_REQUEST 4711
#define PSC_RPC_MSG_ERR     4712
#define PSC_RPC_MSG_REPLY   4713

#define PSCRPC_MSG_MAGIC    0x0BD00BD0
#if !CATAMOUNT_CLIENT
#define PSCRPC_MSG_VERSION  0x00000003
#else
#define PSCRPC_MSG_VERSION  0x00000004
#endif

#define ZOBD_FREE(ptr, size) free(ptr)
#define ZOBD_ALLOC(ptr, size) ((ptr) = PSCALLOC(size))

#ifndef PAGE_SIZE
#define PAGE_SIZE               4096
#endif

#define PSCRPC_MAX_BRW_SIZE     LNET_MTU
#define PSCRPC_MAX_BRW_PAGES    (PSCRPC_MAX_BRW_SIZE/PAGE_SIZE)
#define CURRENT_SECONDS         time(NULL)
#define PSC_SERVER   0xff /* differentiate client and server for ni init */
#define PSC_CLIENT   0x0f
#define PSC_SVR_PID  54321
#define ZOBD_TIMEOUT 60

extern lnet_handle_eq_t pscrpc_eq_h;
extern struct psclist_head pscrpc_wait_callbacks;

struct psc_handle {
	u64 cookie;
};
#define DEAD_HANDLE_MAGIC 0xdeadbeefcafebabeULL

struct l_wait_info {
	time_t lwi_timeout;
	long   lwi_interval;
	int  (*lwi_on_timeout)(void *);
	void (*lwi_on_signal)(void *);
	void  *lwi_cb_data;
};

struct pscrpc_wait_callback {
	struct psclist_head    llwc_list;
	int               (*llwc_fn)(void *arg);
	void               *llwc_arg;
};

enum psc_rq_phase {
	ZRQ_PHASE_NEW         = 0xebc0de00,
	ZRQ_PHASE_RPC         = 0xebc0de01,
	ZRQ_PHASE_BULK        = 0xebc0de02,
	ZRQ_PHASE_INTERPRET   = 0xebc0de03,
	ZRQ_PHASE_COMPLETE    = 0xebc0de04,
};

enum psc_imp_state {
	PSC_IMP_CLOSED       = 1,
	PSC_IMP_NEW          = 2,
	PSC_IMP_DISCON       = 3,
	PSC_IMP_CONNECTING   = 4,
	PSC_IMP_REPLAY       = 5,
	PSC_IMP_REPLAY_LOCKS = 6,
	PSC_IMP_REPLAY_WAIT  = 7,
	PSC_IMP_RECOVER      = 8,
	PSC_IMP_FULL         = 9,
	PSC_IMP_EVICTED      = 10
};

struct psc_uuid {
	//u8 uuid[40];
	u8 uuid[8];
};

struct pscrpc_cb_id {
	void   (*cbid_fn)(lnet_event_t *ev);    /* specific callback fn */
	void    *cbid_arg;                      /* additional arg */
};

struct pscrpc_export {
	psc_spinlock_t		  exp_lock;
	struct psc_handle         exp_handle;
	atomic_t                  exp_refcount;
	atomic_t                  exp_rpc_count;
	struct pscrpc_connection *exp_connection;
	int                       exp_failed;
	void			(*exp_destroycb)(void *);
	void			 *exp_private; /* app-specific data */
	//struct psclist_head       exp_outstanding_replies;
};
/* TODO - c_exp needs to become a pointer */
struct pscrpc_connection {
	struct psclist_head   c_link;
	lnet_nid_t            c_self;
	lnet_process_id_t     c_peer;
	struct psc_uuid       c_remote_uuid;
	atomic_t              c_refcount;
	struct pscrpc_export *c_exp;  /* meaningful only on server */
};

struct pscrpc_client {
	u32                   cli_request_portal;
	u32                   cli_reply_portal;
	char                 *cli_name;
};

struct pscrpc_request_pool {
	spinlock_t          prp_lock;
	struct psclist_head prp_req_list;    /* list of request structs */
	int                 prp_rq_size;
	void (*prp_populate)(struct pscrpc_request_pool *, int);
};

struct pscrpc_import {
	struct pscrpc_connection *imp_connection;
	struct pscrpc_client     *imp_client;
	struct psclist_head       imp_list;
	struct psclist_head       imp_conn_list;
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
	struct psc_handle         imp_remote_handle;
	u64                       imp_last_replay_transno;
	u64                       imp_last_transno_checked; /* optimize */
	u64                       imp_peer_committed_transno;
	wait_queue_head_t         imp_recovery_waitq;
	struct psclist_head       imp_delayed_list;
	unsigned int              imp_invalid:1,
		                  imp_server_timeout:1,
		                  imp_deactive:1,
		                  imp_replayable:1,
		                  imp_force_verify:1,
		                  imp_failed:1;
#if 0
	unsigned int              imp_invalid:1, imp_replayable:1,
		imp_dlm_fake:1, imp_server_timeout:1,
		imp_initial_recov:1, imp_initial_recov_bk:1,
		imp_force_verify:1, imp_pingable:1,
		imp_resend_replay:1, imp_deactive:1;
#endif
};

struct pscrpc_async_args {
	/* Scratchpad for passing args to completion interpreter. Users
	 * cast to the struct of their choosing, and LASSERT that this is
	 * big enough.  For _tons_ of context, ZOBD_ALLOC a struct and store
	 * a pointer to it here.  The pointer_arg ensures this struct is at
	 * least big enough for that. */
	u64      space[2];
	void      *pointer_arg[5];
};

struct pscrpc_request_set;
typedef int (*set_interpreter_func)(struct pscrpc_request_set *, void *, int);

struct pscrpc_request_set {
	struct psclist_head  set_list;         /* associate with import */
	struct psclist_head  set_requests;
	psc_spinlock_t       set_new_req_lock;
	struct psclist_head  set_new_requests;
	int                  set_remaining;
	psc_waitq_t          set_waitq;        /* I block here          */
	psc_waitq_t         *set_wakeup_ptr;   /* Others wait here..    */
	set_interpreter_func set_interpret;    /* callback function     */
	void                *set_arg;          /* callback pointer      */
	psc_spinlock_t       set_lock;
#if 0
	//psc_spinlock_t     rqset_lock;
	//psclist_cache_t    rqset_reqs;       /* the request list      */
#endif
};


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
	psc_waitq_t               bd_waitq;        /* server side only WQ    */
	int                       bd_iov_count;    /* # entries in bd_iov    */
	int                       bd_max_iov;      /* alloc'd size of bd_iov */
	int                       bd_nob;          /* # bytes covered        */
	int                       bd_nob_transferred; /* # bytes GOT/PUT     */
	u64                       bd_last_xid;     /* track xid for retry    */
	u32                       bd_portal;       /* which portal           */
	struct pscrpc_cb_id       bd_cbid;         /* network callback info  */
	lnet_handle_md_t          bd_md_h;         /* associated MD          */
	lnet_md_iovec_t           bd_iov[0];       /* must be last           */
};

#if 0
struct pscrpc_thread {
	struct psclist_head t_link; /* active threads, from svc->srv_threads */
	void               *t_data; /* thread-private data (prealloc memory) */
	unsigned int        t_flags;
	unsigned int        t_id;  /* service thread index */
	wait_queue_head_t   t_ctl_waitq;
};
#endif

struct psc_msg {
	struct psc_handle handle;
	u32 magic;
	u32 type;
	u32 version;
	u32 opc;
	u64 last_xid;
	u64 last_committed;
	u64 transno;
	u32 status;
	u32 flags;
	u32 conn_cnt;
	u32 bufcount;
	u32 buflens[0];
};

struct pscrpc_reply_state;
struct pscrpc_request_buffer_desc;
struct pscrpc_nbrequests;

struct pscrpc_request {
	int rq_type;
	int rq_status;
	int rq_retry;
	int rq_timeout;         /* time to wait for reply (seconds) */
	int rq_request_portal;
	int rq_reply_portal;
	int rq_nob_received;    /* client-side # reply bytes received */
	int rq_reqlen;
	int rq_replen;
	int rq_import_generation;
	time_t rq_sent;
	psc_spinlock_t rq_lock;
	u64 rq_transno;
	u64 rq_xid;
	u64 rq_history_seq;
	unsigned int rq_intr:1, rq_replied:1, rq_err:1, rq_timedout:1,
		     rq_resend:1, rq_restart:1, rq_replay:1, rq_no_resend:1,
		     rq_waiting:1, rq_receiving_reply:1, rq_no_delay:1,
		     rq_net_err:1;
	atomic_t rq_refcount; /* client-side refcnt for SENT race */
	atomic_t rq_retries;  /* count retries */
	lnet_process_id_t rq_peer;
	lnet_nid_t        rq_self;
	enum   psc_rq_phase         rq_phase; /* one of RQ_PHASE_* */
	enum   psc_imp_state        rq_send_state;
	struct psclist_head         rq_list_entry;
	struct psclist_head         rq_set_chain_lentry;
	struct psclist_head         rq_list_set;   /* for request set */
	struct psclist_head         rq_history_list;
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
	void                       *rq_interpret_reply; /* Async completion handler */
	struct pscrpc_async_args    rq_async_args;      /* Async completion context */
	lnet_handle_md_t            rq_req_md_h;
	/* client-only incoming reply */
	lnet_handle_md_t            rq_reply_md_h;
	psc_waitq_t                 rq_reply_waitq;
	/* server-side... */
	struct timeval              rq_arrival_time; /* request arrival time */
	struct pscrpc_reply_state  *rq_reply_state;  /* separated reply state */
	struct pscrpc_request_buffer_desc *rq_rqbd;  /* incoming req  buffer*/
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
	int srv_num_threads;       /* # threads to start/started */
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
	u32 srv_req_portal;
	u32 srv_rep_portal;
	u64 srv_request_seq;       /* next request sequence # */
	u64 srv_request_max_cull_seq; /* highest seq culled from history */
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
	psc_waitq_t         srv_free_rs_waitq;
	psc_waitq_t         srv_waitq; /* all threads sleep on this. This
					* wait-queue is signalled when new
					* incoming request arrives and when
					* difficult reply has to be handled. */
	psc_spinlock_t srv_lock;
	svc_handler_t  srv_handler;
	char          *srv_name;  /* only statically allocated strings here,
				   * we don't clean them */
	/*
	 * if non-NULL called during thread creation (pscrpc_start_thread())
	 * to initialize service specific per-thread state.
	 */
	int (*srv_init)(struct psc_thread *thread);
	/*
	 * if non-NULL called during thread shutdown (pscrpc_main()) to
	 * destruct state created by ->srv_init().
	 */
	void (*srv_done)(struct psc_thread *thread);
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
	u64                    rs_xid;
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
	//psc_spinlock_t            *nb_lock;
	struct pscrpc_request_set *nb_reqset;
	nbreq_callback             nb_callback;
	atomic_t                   nb_outstanding;
};

struct pscrpc_nbreqset *
nbreqset_init(set_interpreter_func nb_interpret,
	      nbreq_callback       nb_callback);

void
nbreqset_add(struct pscrpc_nbreqset *nbs,
	     struct pscrpc_request  *req);

int
nbrequest_reap(struct pscrpc_nbreqset *nbs);

int
nbrequest_flush(struct pscrpc_nbreqset *);

/* End non-blocking request sets */

void
psc_free_reply_state(struct pscrpc_reply_state *rs);

void
pscrpc_req_finished(struct pscrpc_request *request);

struct pscrpc_bulk_desc *
pscrpc_prep_bulk_exp (struct pscrpc_request *req, int npages,
		      int type, int portal);

void
pscrpc_free_bulk(struct pscrpc_bulk_desc *desc);

int
psc_pack_reply (struct pscrpc_request *req,
		int count, int *lens, char **bufs);

int
pscrpc_put_connection(struct pscrpc_connection *c);

void
pscrpc_fill_bulk_md (lnet_md_t *md, struct pscrpc_bulk_desc *desc);

/*  events.c */
void request_in_callback(lnet_event_t *ev);
void request_out_callback(lnet_event_t *ev);
void client_bulk_callback (lnet_event_t *ev);
void server_bulk_callback (lnet_event_t *ev);
void reply_in_callback(lnet_event_t *ev);
void reply_out_callback(lnet_event_t *ev);
void pscrpc_deregister_wait_callback (void *opaque);
int pscrpc_check_events (int timeout);
int pscrpc_wait_event (int timeout);
lnet_pid_t psc_get_pid(void);
int pscrpc_ni_init(int type);
int pscrpc_init_portals(int);
/* events.c done */

/* packgeneric.c */
int psc_msg_size(int count, int *lengths);
int psc_msg_swabbed(struct psc_msg *msg);

int psc_pack_request (struct pscrpc_request *req,
		  int count, int *lens, char **bufs);

int psc_pack_reply (struct pscrpc_request *req,
		int count, int *lens, char **bufs);

int psc_unpack_msg(struct psc_msg *m, int len);

void *
psc_msg_buf(struct psc_msg *m, int n, int min_size);

/**
 * psc_msg_buflen - return the length of buffer @n in message @m
 * @m - psc_msg (request or reply) to look at
 * @n - message index (base 0)
 *
 * returns zero for non-existent message indices
 */
int
psc_msg_buflen(struct psc_msg *m, int n);

char *
psc_msg_string(struct psc_msg *m, int idx, int max_len);
/* packgeneric.c done */

/* niobuf.c */
int  pscrpc_start_bulk_transfer(struct pscrpc_bulk_desc *desc);
void pscrpc_abort_bulk(struct pscrpc_bulk_desc *desc);
int  pscrpc_register_bulk(struct pscrpc_request *req);
void pscrpc_unregister_bulk(struct pscrpc_request *req);
int  pscrpc_send_reply(struct pscrpc_request *req, int may_be_difficult);
int  pscrpc_reply(struct pscrpc_request *req);
int  pscrpc_error(struct pscrpc_request *req);
int  psc_send_rpc(struct pscrpc_request *request, int noreply);
int  pscrpc_register_rqbd(struct pscrpc_request_buffer_desc *rqbd);
void psc_free_reply_state(struct pscrpc_reply_state *rs);
void pscrpc_free_req(struct pscrpc_request *request);
void pscrpc_req_finished(struct pscrpc_request *request);
void pscrpc_free_bulk(struct pscrpc_bulk_desc *desc);
void pscrpc_fill_bulk_md(lnet_md_t *md, struct pscrpc_bulk_desc *desc);
/* niobuf.c done */

struct pscrpc_connection *
pscrpc_get_connection(lnet_process_id_t peer,
		      lnet_nid_t self, struct psc_uuid *uuid);

struct pscrpc_connection*
pscrpc_lookup_conn_locked (lnet_process_id_t peer, lnet_nid_t self);

struct pscrpc_connection *
pscrpc_connection_addref(struct pscrpc_connection *c);

void
pscrpc_abort_inflight(struct pscrpc_import *imp);

/*  rpcclient.c */
int
pscrpc_expire_one_request(struct pscrpc_request *req);

struct pscrpc_request *
pscrpc_prep_req(struct pscrpc_import *imp, __u32 version, int opcode,
		int count, int *lengths, char **bufs);

struct pscrpc_bulk_desc *
pscrpc_prep_bulk_imp (struct pscrpc_request *req, int npages,
		      int type, int portal);

struct pscrpc_request *
pscrpc_request_addref(struct pscrpc_request *req);

void
import_put(struct pscrpc_import *import);

struct pscrpc_import *
new_import(void);

int
pscrpc_queue_wait(struct pscrpc_request *req);

struct pscrpc_request_set *
pscrpc_prep_set(void);

int
pscrpc_push_req(struct pscrpc_request *req);

void
pscrpc_set_add_new_req(struct pscrpc_request_set *set,
		       struct pscrpc_request     *req);

int
pscrpc_check_set(struct pscrpc_request_set *set,
		  int check_allsent);

int  pscrpc_set_wait(struct pscrpc_request_set *);
void pscrpc_set_destroy(struct pscrpc_request_set *);

/*  rpcclient.c done */

static inline int
pscrpc_bulk_active (struct pscrpc_bulk_desc *desc)
{
	int rc;

	spinlock(&desc->bd_lock);
	rc = desc->bd_network_rw;
	freelock(&desc->bd_lock);
	return (rc);
}

/* service.c */
int
target_send_reply_msg (struct pscrpc_request *req, int rc, int fail_id);

void
pscrpc_fail_import(struct pscrpc_import *imp, __u32 conn_cnt);

/* service.c done */

static inline void
pscrpc_rs_addref(struct pscrpc_reply_state *rs)
{
	LASSERT(atomic_read(&rs->rs_refcount) > 0);
	atomic_inc(&rs->rs_refcount);
}

static inline void
pscrpc_rs_decref(struct pscrpc_reply_state *rs)
{
	LASSERT(atomic_read(&rs->rs_refcount) > 0);
	if (atomic_dec_and_test(&rs->rs_refcount))
		psc_free_reply_state(rs);
}

static inline void
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

static inline int
psc_msg_get_flags(struct psc_msg *msg)
{
	return (msg->flags & MSG_GEN_FLAG_MASK);
}

static inline void
psc_msg_add_flags(struct psc_msg *msg, int flags)
{
	msg->flags |= MSG_GEN_FLAG_MASK & flags;
}

static inline void
psc_msg_set_flags(struct psc_msg *msg, int flags)
{
	msg->flags &= ~MSG_GEN_FLAG_MASK;
	psc_msg_add_flags(msg, flags);
}

static inline void
psc_msg_clear_flags(struct psc_msg *msg, int flags)
{
	msg->flags &= ~(MSG_GEN_FLAG_MASK & flags);
}

static inline int
psc_msg_get_op_flags(struct psc_msg *msg)
{
	return (msg->flags >> MSG_OP_FLAG_SHIFT);
}

static inline void
psc_msg_add_op_flags(struct psc_msg *msg, int flags)
{
	msg->flags |= ((flags & MSG_GEN_FLAG_MASK) << MSG_OP_FLAG_SHIFT);
}

static inline void
psc_msg_set_op_flags(struct psc_msg *msg, int flags)
{
	msg->flags &= ~MSG_OP_FLAG_MASK;
	psc_msg_add_op_flags(msg, flags);
}


static inline int
pscrpc_client_receiving_reply (struct pscrpc_request *req)
{
	int           rc;

	spin_lock(&req->rq_lock);
	rc = req->rq_receiving_reply;
	spin_unlock(&req->rq_lock);
	return (rc);
}

static inline int
pscrpc_client_replied (struct pscrpc_request *req)
{
	int           rc;

	spin_lock(&req->rq_lock);
	rc = req->rq_replied;
	spin_unlock(&req->rq_lock);
	return (rc);
}

static inline void
pscrpc_wake_client_req (struct pscrpc_request *req)
{
	if (req->rq_set == NULL)
		wake_up(&req->rq_reply_waitq);
	else
		wake_up(&req->rq_set->set_waitq);
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

#define LWI_INTR(cb, data)  LWI_TIMEOUT_INTR(0, NULL, cb, data)

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
			}						\
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
			}						\
		}							\
	} while (0)


/**
 * __psc_client_wait_event - the below call is for clients.  Clients are
 *	single threaded due to catamount/panther.  This means that clients must
 *	block in LNetEQPoll - this occurs in liblustre_wait_event().  A similar
 *	model can be used for server threads so long as liblustre_wait_event()
 *	is replaced with something that uses timed waitq's.
 */
#define pscrpc_timeout 13
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
		while (!(condition)) {					\
			ret = pscrpc_wait_event(__interval);		\
			if (0<ret) ret=0; /* preserve the previous semantics */	\
			if (condition)					\
				break;					\
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


#define psc_cli_wait_event(wq, condition, info, lck)			\
	({								\
		int                 __ret;				\
		struct l_wait_info *__info = (info);			\
									\
		__psc_client_wait_event(wq, condition, __info, __ret, 0); \
		__ret;							\
	})

#define psc_svr_wait_event(wq, condition, info, lck)			\
	({								\
		int                 __ret;				\
		struct l_wait_info *__info = (info);			\
									\
		__psc_server_wait_event(wq, condition, __info, __ret, 0, lck); \
		__ret;							\
	})

#define psc_wait_event psc_svr_wait_event

#undef list_head
#undef LIST_HEAD_INIT
#undef LIST_ENTRY_INIT
#undef LIST_HEAD
#undef INIT_LIST_HEAD
#undef INIT_LIST_ENTRY

#undef list_add
#undef list_add_tail
#undef list_del
#undef list_del_init
#undef list_empty
#undef list_splice
#undef list_entry
#undef list_for_each
#undef list_for_each_safe

#define list_head		ERROR
#define LIST_HEAD_INIT		ERROR
#define LIST_ENTRY_INIT		ERROR
#define LIST_HEAD		ERROR
#define INIT_LIST_HEAD		ERROR
#define INIT_LIST_ENTRY		ERROR

#define list_add		ERROR
#define list_add_tail		ERROR
#define list_del		ERROR
#define list_del_init		ERROR
#define list_empty		ERROR
#define list_splice		ERROR
#define list_entry		ERROR
#define list_for_each		ERROR
#define list_for_each_safe	ERROR

#endif /* _PFL_RPC_H_ */
