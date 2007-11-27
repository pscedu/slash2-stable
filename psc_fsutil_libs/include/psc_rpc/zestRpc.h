/* $Id: zestRpc.h 2114 2007-11-03 19:39:08Z pauln $ */

#ifndef ZESTRPC_H
#define ZESTRPC_H

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

#include "zestAtomic.h"
#include "zestList.h"
#include "zestListCache.h"
#include "zestLock.h"
#include "zestThread.h"
#include "zestTypes.h"
#include "zestWaitq.h"
#include "zeil.h"

#define ZESTRPC_MD_OPTIONS 0
#define BULK_GET_SOURCE    0
#define BULK_PUT_SINK      1
#define BULK_GET_SINK      2
#define BULK_PUT_SOURCE    3

#define ZEST_RPC_MSG_REQUEST 4711
#define ZEST_RPC_MSG_ERR     4712
#define ZEST_RPC_MSG_REPLY   4713

#define ZESTRPC_MSG_MAGIC    0x0BD00BD0
#if !CATAMOUNT_CLIENT
#define ZESTRPC_MSG_VERSION  0x00000003
#else
#define ZESTRPC_MSG_VERSION  0x00000004
#endif

#define ZOBD_FREE(ptr, size) ((void)(size), free((ptr)))
#define ZOBD_ALLOC(ptr, size) (ptr = ZALLOC(size))

#define ZESTRPC_MAX_BRW_SIZE     LNET_MTU
#define PAGE_SIZE                4096
#define ZESTRPC_MAX_BRW_PAGES    (ZESTRPC_MAX_BRW_SIZE/PAGE_SIZE)

# define CURRENT_SECONDS         time(NULL)

#define ZEST_SERVER 0xff
#define ZEST_CLIENT 0x0f

#define ZEST_SVR_PID 54321

#define ZOBD_TIMEOUT 60

lnet_handle_eq_t zestrpc_eq_h;
struct zlist_head zestrpc_wait_callbacks;

struct zest_handle {
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

struct zestrpc_wait_callback {
        struct zlist_head    llwc_list;
        int               (*llwc_fn)(void *arg);
        void               *llwc_arg;
};

enum rq_phase {
        ZRQ_PHASE_NEW         = 0xebc0de00,
        ZRQ_PHASE_RPC         = 0xebc0de01,
        ZRQ_PHASE_BULK        = 0xebc0de02,
        ZRQ_PHASE_INTERPRET   = 0xebc0de03,
        ZRQ_PHASE_COMPLETE    = 0xebc0de04,
};

enum zest_imp_state {
        ZEST_IMP_CLOSED     = 1,
        ZEST_IMP_NEW        = 2,
        ZEST_IMP_DISCON     = 3,
        ZEST_IMP_CONNECTING = 4,
        ZEST_IMP_REPLAY     = 5,
        ZEST_IMP_REPLAY_LOCKS = 6,
        ZEST_IMP_REPLAY_WAIT  = 7,
        ZEST_IMP_RECOVER    = 8,
        ZEST_IMP_FULL       = 9,
        ZEST_IMP_EVICTED    = 10,
};

struct zest_uuid {
        //u8 uuid[40];
        u8 uuid[8];
};

struct zestrpc_cb_id {
        void   (*cbid_fn)(lnet_event_t *ev);    /* specific callback fn */
        void    *cbid_arg;                      /* additional arg */
};

struct zestrpc_export {
	zest_spinlock_t		   exp_lock;
        struct zest_handle         exp_handle;
        atomic_t                   exp_refcount;
        atomic_t                   exp_rpc_count;
	struct zestrpc_connection *exp_connection;
	int                        exp_failed;
	struct zeiltree		   exp_zeiltree;
	//struct zlist_head           exp_outstanding_replies;
};
/* TODO - c_exp needs to become a pointer */
struct zestrpc_connection {
        struct zlist_head      c_link;
        lnet_nid_t             c_self;
        lnet_process_id_t      c_peer;
        struct zest_uuid       c_remote_uuid;
	atomic_t               c_refcount;
	struct zestrpc_export *c_exp;  //only meaningful on server
};

struct zestrpc_client {
        u32                   cli_request_portal;
        u32                   cli_reply_portal;
        char                 *cli_name;
};

struct zestrpc_request_pool {
        spinlock_t prp_lock;
        struct zlist_head prp_req_list;    /* list of zestrpc_request structs */
        int prp_rq_size;
        void (*prp_populate)(struct zestrpc_request_pool *, int);
};

struct zestrpc_import {
	struct zestrpc_connection *imp_connection;
	struct zestrpc_client     *imp_client;
	struct zlist_head           imp_list;
	struct zlist_head           imp_conn_list;
	struct zlist_head           imp_sending_list;  /* in-flight? */
	struct zlist_head           imp_uncomitted;    /* post send  */
	struct zlist_head           imp_rpcsets;       /* list of sets */
	size_t                     imp_nsets;
	zest_spinlock_t            imp_lock;
	atomic_t                   imp_inflight;
	atomic_t                   imp_refcount;
	int                        imp_generation;
	int                        imp_conn_cnt;
	int                        imp_max_retries;
        enum zest_imp_state      imp_state;
	struct zest_handle         imp_remote_handle;
	u64                        imp_last_replay_transno;
	u64                        imp_last_transno_checked; /* optimize */
	u64                        imp_peer_committed_transno;
	wait_queue_head_t          imp_recovery_waitq;
	struct zlist_head           imp_delayed_list;
	unsigned int               imp_invalid:1,
		                   imp_server_timeout:1,
		                   imp_deactive:1,
		                   imp_replayable:1,
		                   imp_force_verify:1;
#if 0
	unsigned int              imp_invalid:1, imp_replayable:1,
		imp_dlm_fake:1, imp_server_timeout:1,
		imp_initial_recov:1, imp_initial_recov_bk:1,
		imp_force_verify:1, imp_pingable:1,
		imp_resend_replay:1, imp_deactive:1;
#endif
};

struct zestrpc_async_args {
        /* Scratchpad for passing args to completion interpreter. Users
	 * cast to the struct of their choosing, and LASSERT that this is
	 * big enough.  For _tons_ of context, ZOBD_ALLOC a struct and store
	 * a pointer to it here.  The pointer_arg ensures this struct is at
         * least big enough for that. */
        __u64      space[2];
        void      *pointer_arg[5];
};

struct zestrpc_request_set;
typedef int (*set_interpreter_func)(struct zestrpc_request_set *, void *, int);

struct zestrpc_request_set {
	struct zlist_head     set_list;         /* associate with import */
	struct zlist_head     set_requests;
	zest_spinlock_t       set_new_req_lock;
        struct zlist_head     set_new_requests;

	int                   set_remaining;
	zwaitq_t              set_waitq;        /* I block here          */
	zwaitq_t             *set_wakeup_ptr;   /* Others wait here..    */
	set_interpreter_func  set_interpret;    /* callback function     */
	void                 *set_arg;          /* callback pointer      */
	//zest_spinlock_t     rqset_lock;
	//zlist_cache_t       rqset_reqs;       /* the request list      */
};


struct zestrpc_bulk_desc {
	unsigned int bd_success:1;              /* completed successfully */
        unsigned int bd_network_rw:1;           /* accessible to the network */
        unsigned int bd_type:2;                 /* {put,get}{source,sink} */
        unsigned int bd_registered:1;           /* client side */
        zest_spinlock_t   bd_lock;              /* serialise with callback */
        int bd_import_generation;
        struct zestrpc_import *bd_import;       /* client only (pauln) */
	/*
	 * Zest doesn't want 1 export per client, so
	 *   we'll just attach the conn struct to the
	 *   bulk desc
	 */
	struct zestrpc_connection *bd_connection; /* server only i think (pauln) */
	struct zestrpc_export     *bd_export;

        u32 bd_portal;
        struct zestrpc_request *bd_req;          /* associated request */
        zwaitq_t                bd_waitq;        /* server side only WQ */
        int                     bd_iov_count;    /* # entries in bd_iov */
        int                     bd_max_iov;      /* allocated size of bd_iov */
        int                     bd_nob;          /* # bytes covered */
        int                     bd_nob_transferred; /* # bytes GOT/PUT */

        u64                     bd_last_xid;

        struct zestrpc_cb_id    bd_cbid;         /* network callback info */
        lnet_handle_md_t        bd_md_h;         /* associated MD */

        lnet_md_iovec_t         bd_iov[0];
};

struct zestrpc_thread {

        struct zlist_head t_link; /* active threads for service, from svc->srv_threads */

        void *t_data;            /* thread-private data (preallocated memory) */
        __u32 t_flags;

        unsigned int t_id; /* service thread index, from ptlrpc_start_threads */
        wait_queue_head_t t_ctl_waitq;
};


struct zest_msg {
        struct zest_handle handle;
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


struct zestrpc_reply_state;
struct zestrpc_request_buffer_desc;
struct zestrpc_nbrequests;

struct zestrpc_request {
	int                rq_type;
	struct zlist_head  rq_list_entry;
	struct zlist_head  rq_list_set;  /* for request set */
	struct zlist_head  rq_history_list;
	zest_spinlock_t    rq_lock;
	
	u64 rq_history_seq;

	int rq_status;
	int rq_retry;

	struct zestrpc_import       *rq_import; /* client side */
	struct zestrpc_request_pool *rq_pool;
	struct zestrpc_connection   *rq_conn; /* svr-side */
	struct zestrpc_export       *rq_export;

	unsigned int rq_intr:1, rq_replied:1, rq_err:1,
                rq_timedout:1, rq_resend:1, rq_restart:1,
		/*
		 * when ->rq_replay is set, request is kept by the client even
		 * after server commits corresponding transaction. This is
		 * used for operations that require sequence of multiple
		 * requests to be replayed. The only example currently is file
		 * open/close. When last request in such a sequence is
		 * committed, ->rq_replay is cleared on all requests in the
		 * sequence.
		 */
		 rq_replay:1, rq_no_resend:1, rq_waiting:1, rq_receiving_reply:1,
		 rq_no_delay:1, rq_net_err:1;

	enum rq_phase rq_phase; /* one of RQ_PHASE_* */
	atomic_t rq_refcount;   /* client-side refcount for SENT race     */
	atomic_t rq_retries;    /* how many times the req has bee retried */

	int rq_request_portal;  /* XXX FIXME bug 249 */
        int rq_reply_portal;    /* XXX FIXME bug 249 */

	int rq_timeout;         /* time to wait for reply (seconds) */
	time_t rq_sent;

	int rq_nob_received;    /* client-side # reply bytes actually received */

	int rq_reqlen;
	int rq_replen;

        struct zestion_thread *rq_svc_thread; /* initial thread servicing req */

	struct zestrpc_request_set *rq_set;
	struct zestrpc_nbrequests  *rq_nbreqs;
	struct zlist_head rq_set_chain;

        int rq_import_generation;
        enum zest_imp_state rq_send_state;

	struct zest_msg *rq_reqmsg;
	struct zest_msg *rq_repmsg;

	/* client+server request */
        lnet_handle_md_t  rq_req_md_h;

        struct zestrpc_cb_id rq_req_cbid;
	struct zestrpc_cb_id rq_reply_cbid;

	u64 rq_transno;
        u64 rq_xid;

	/* client-only incoming reply */
        lnet_handle_md_t  rq_reply_md_h;
        zwaitq_t          rq_reply_waitq;

	struct zestrpc_bulk_desc *rq_bulk;       /* client side bulk */

	lnet_process_id_t rq_peer;
	lnet_nid_t        rq_self;

        void *rq_interpret_reply;               /* Async completion handler */
        struct zestrpc_async_args rq_async_args; /* Async completion context */

	/* server-side... */
        struct timeval       rq_arrival_time;       /* request arrival time */
        struct zestrpc_reply_state *rq_reply_state;  /* separated reply state */
        struct zestrpc_request_buffer_desc *rq_rqbd; /* incoming request buffer*/
};

/*
 * Each service installs its own request handler
 */
typedef int (*svc_handler_t)(struct zestrpc_request *req);
/*
 * Server side request management
 */
struct zestrpc_service {
	struct zlist_head  srv_list_entry;              /* chain thru all services */
	int               srv_max_req_size;      /* biggest request to receive */
        int               srv_max_reply_size;    /* biggest reply to send */
        int               srv_buf_size;          /* size of individual buffers */
	int               srv_nbuf_per_group;
        int              srv_watchdog_timeout; /* soft watchdog timeout, in ms */
        int              srv_num_threads;       /* # threads to start/started */
        int              srv_n_active_reqs;     /* # reqs being served */
        int              srv_n_difficult_replies; /* # 'difficult' replies */
        int              srv_nthreads;          /* # running threads */
	int              srv_failure;            /* we've failed, no new requests */
	int              srv_rqbd_timeout;
        u32               srv_req_portal;
        u32               srv_rep_portal;

	int               srv_n_queued_reqs;     /* # reqs waiting to be served */
        struct zlist_head  srv_request_queue;     /* reqs waiting for service */
	struct zlist_head  srv_request_history;   /* request history */

        u64               srv_request_seq;       /* next request sequence # */
        u64               srv_request_max_cull_seq; /* highest seq culled from history */

	struct zlist_head  srv_idle_rqbds;        /* request buffers to be reposted */
        struct zlist_head  srv_active_rqbds;      /* req buffers receiving */
        struct zlist_head  srv_history_rqbds;     /* request buffer history */
        int               srv_nrqbd_receiving;   /* # posted request buffers */
        int               srv_n_history_rqbds;   /* # request buffers in history */
        int               srv_max_history_rqbds; /* max # request buffers in history */
	int               srv_nbufs;             /* total # req buffer descs allocated */

	atomic_t          srv_outstanding_replies;
        struct zlist_head  srv_active_replies;    /* all the active replies */
        struct zlist_head  srv_reply_queue;       /* replies waiting for service */

	struct zlist_head  srv_free_rs_list;
	zwaitq_t          srv_free_rs_waitq;

        zwaitq_t          srv_waitq; /* all threads sleep on this. This
				      * wait-queue is signalled when new
				      * incoming request arrives and when
				      * difficult reply has to be handled. */

        struct zlist_head  srv_threads;
        svc_handler_t     srv_handler;

        char *srv_name;  /* only statically allocated strings here; we don't clean them */

        zest_spinlock_t        srv_lock;
	/*
         * if non-NULL called during thread creation (zestrpc_start_thread())
	 * to initialize service specific per-thread state.
	 */
        int (*srv_init)(struct zestion_thread *thread);
        /*
	 * if non-NULL called during thread shutdown (zestrpc_main()) to
	 * destruct state created by ->srv_init().
	 */
        void (*srv_done)(struct zestion_thread *thread);
};

struct zestrpc_request_buffer_desc {
        struct zlist_head        rqbd_list;
        struct zlist_head        rqbd_reqs;
        struct zestrpc_service *rqbd_service;
        lnet_handle_md_t        rqbd_md_h;
        int                     rqbd_refcount;
        char                   *rqbd_buffer;
        struct zestrpc_cb_id    rqbd_cbid;
        struct zestrpc_request  rqbd_req;
};


struct zestrpc_reply_state {
	struct zestrpc_cb_id    rs_cb_id;
        struct zlist_head        rs_list_entry;

	unsigned int            rs_difficult:1;     /* ACK/commit stuff */
        unsigned int            rs_scheduled:1;     /* being handled? */
        unsigned int            rs_scheduled_ever:1;/* any schedule attempts? */
        unsigned int            rs_handled:1;  /* been handled yet? */
        unsigned int            rs_on_net:1;   /* reply_out_callback pending? */
        unsigned int            rs_prealloc:1; /* rs from prealloc list */
	atomic_t                rs_refcount;

	lnet_handle_md_t        rs_md_h;
	struct zestrpc_service *rs_service;
	u64                     rs_xid;
	int                     rs_size;

	struct zest_msg         rs_msg;
};

/*
 * Non-blocking request sets 
 */
typedef int (*nbreq_callback)(struct zestrpc_request *,
                              struct zestrpc_async_args *);

struct zestrpc_nbreqset {
        //zest_spinlock_t            *nb_lock;                                  
        struct zestrpc_request_set *nb_reqset;
        nbreq_callback              nb_callback;
        atomic_t                    nb_outstanding;
};

extern struct zestrpc_nbreqset *
nbreqset_init(set_interpreter_func nb_interpret,
              nbreq_callback       nb_callback);

extern void
nbreqset_add(struct zestrpc_nbreqset *nbs,
             struct zestrpc_request  *req);

extern int
nbrequest_reap(struct zestrpc_nbreqset *nbs);

/*
 * End Non-blocking request sets 
 */

void zest_free_reply_state(struct zestrpc_reply_state *rs);

void zestrpc_req_finished(struct zestrpc_request *request);

struct zestrpc_bulk_desc *
    zestrpc_prep_bulk_exp (struct zestrpc_request *req, int npages,
	int type, int portal);

void zestrpc_free_bulk(struct zestrpc_bulk_desc *desc);

int zest_pack_reply (struct zestrpc_request *req,
                       int count, int *lens, char **bufs);

int zestrpc_put_connection(struct zestrpc_connection *c);

void zestrpc_fill_bulk_md (lnet_md_t *md, struct zestrpc_bulk_desc *desc);

/*
 *  zestEvents.c
 */
extern
void zrequest_in_callback(lnet_event_t *ev);

extern
void zrequest_out_callback(lnet_event_t *ev);

extern
void zclient_bulk_callback (lnet_event_t *ev);

extern
void zserver_bulk_callback (lnet_event_t *ev);

extern
void zreply_in_callback(lnet_event_t *ev);

extern
void zreply_out_callback(lnet_event_t *ev);

extern void
zestrpc_deregister_wait_callback (void *opaque);

extern int
zestrpc_check_events (int timeout);

extern int
zestrpc_wait_event (int timeout);

extern lnet_pid_t
zest_get_pid(void);

extern int zestrpc_ni_init(int type);

extern int zestrpc_init_portals(int);

/*
 * zestEvents.c done
 */
/*
 * zestPackGeneric.c
 */
int zest_msg_size(int count, int *lengths);

extern
int zest_msg_swabbed(struct zest_msg *msg);

extern
int zest_pack_request (struct zestrpc_request *req,
                         int count, int *lens, char **bufs);

extern
int zest_pack_reply (struct zestrpc_request *req,
                       int count, int *lens, char **bufs);

extern
int zest_unpack_msg(struct zest_msg *m, int len);

void *zest_msg_buf(struct zest_msg *m, int n, int min_size);

/**
 * zest_msg_buflen - return the length of buffer @n in message @m
 * @m - zest_msg (request or reply) to look at
 * @n - message index (base 0)
 *
 * returns zero for non-existent message indices
 */
extern
int zest_msg_buflen(struct zest_msg *m, int n);

extern
char *zest_msg_string (struct zest_msg *m, int idx, int max_len);
/*
 * zestPackGeneric.c done
 */
/*
 * zestNiobuf.c
 */
extern
int zestrpc_start_bulk_transfer (struct zestrpc_bulk_desc *desc);

extern
void zestrpc_abort_bulk (struct zestrpc_bulk_desc *desc);

extern
int zestrpc_register_bulk (struct zestrpc_request *req);

extern
void zestrpc_unregister_bulk (struct zestrpc_request *req);

extern
int zestrpc_send_reply (struct zestrpc_request *req, int may_be_difficult);

extern
int zestrpc_reply (struct zestrpc_request *req);

extern
int zestrpc_error(struct zestrpc_request *req);

extern
int zest_send_rpc(struct zestrpc_request *request, int noreply);

extern
int zestrpc_register_rqbd (struct zestrpc_request_buffer_desc *rqbd);

extern
void zest_free_reply_state (struct zestrpc_reply_state *rs);

extern
void zestrpc_free_req(struct zestrpc_request *request);

extern
void zestrpc_req_finished(struct zestrpc_request *request);

extern
void zestrpc_free_bulk(struct zestrpc_bulk_desc *desc);

extern
void zestrpc_fill_bulk_md (lnet_md_t *md, struct zestrpc_bulk_desc *desc);
/*
 * zestNiobuf.c done
 */
extern struct zestrpc_connection *
zestrpc_get_connection(lnet_process_id_t peer,
                       lnet_nid_t self, struct zest_uuid *uuid);

extern struct zestrpc_connection*
zestrpc_lookup_conn_locked (lnet_process_id_t peer);

extern struct zestrpc_connection *
zestrpc_connection_addref(struct zestrpc_connection *c);

extern void
zestrpc_abort_inflight(struct zestrpc_import *imp);


/*
 *  zestRpcClient.c
 */
extern
int zestrpc_expire_one_request(struct zestrpc_request *req);

struct zestrpc_request *
zestrpc_prep_req(struct zestrpc_import *imp, __u32 version, int opcode,
		int count, int *lengths, char **bufs);

struct zestrpc_bulk_desc *
zestrpc_prep_bulk_imp (struct zestrpc_request *req, int npages,
		      int type, int portal);

extern struct zestrpc_request *
zestrpc_request_addref(struct zestrpc_request *req);

extern void
import_put(struct zestrpc_import *import);

struct zestrpc_import *new_import(void);

int zestrpc_queue_wait(struct zestrpc_request *req);

extern 
struct zestrpc_request_set * zestrpc_prep_set(void);

extern int 
zestrpc_push_req(struct zestrpc_request *req);

extern void
zestrpc_set_add_new_req(struct zestrpc_request_set *set,
                        struct zestrpc_request     *req);

extern int 
zestrpc_check_set(struct zestrpc_request_set *set, 
		  int check_allsent);

extern int 
zestrpc_set_wait(struct zestrpc_request_set *set);

/*
 *  zestRpcClient.c done
 */
static inline int zestrpc_bulk_active (struct zestrpc_bulk_desc *desc)
{
        int           rc;

        spinlock(&desc->bd_lock);
        rc = desc->bd_network_rw;
        freelock(&desc->bd_lock);
        return (rc);
}

/*
 *  zestService.c
 */
extern int
target_send_reply_msg (struct zestrpc_request *req, int rc, int fail_id);

extern void
zrpcthr_spawn(int type);
/*
 *  zestService.c done
 */

/*
 *  zestService.c
 */
extern void
zestrpc_fail_import(struct zestrpc_import *imp, __u32 conn_cnt);
/*
 *  zestService.c done
 */


static inline void
zestrpc_rs_addref(struct zestrpc_reply_state *rs)
{
        LASSERT(atomic_read(&rs->rs_refcount) > 0);
        atomic_inc(&rs->rs_refcount);
}

static inline void
zestrpc_rs_decref(struct zestrpc_reply_state *rs)
{
        LASSERT(atomic_read(&rs->rs_refcount) > 0);
        if (atomic_dec_and_test(&rs->rs_refcount))
                zest_free_reply_state(rs);
}

static inline void
zest_str2uuid(struct zest_uuid *uuid, char *tmp)
{
        strncpy((char *)uuid->uuid, tmp, sizeof(*uuid));
        uuid->uuid[sizeof(*uuid) - 1] = '\0';
}

/* Flags that are operation-specific go in the top 16 bits. */
#define MSG_OP_FLAG_MASK   0xffff0000
#define MSG_OP_FLAG_SHIFT  16

/* Flags that apply to all requests are in the bottom 16 bits */
#define MSG_GEN_FLAG_MASK      0x0000ffff
#define MSG_LAST_REPLAY        1
#define MSG_RESENT             2
#define MSG_REPLAY             4

static inline int zest_msg_get_flags(struct zest_msg *msg)
{
        return (msg->flags & MSG_GEN_FLAG_MASK);
}

static inline void zest_msg_add_flags(struct zest_msg *msg, int flags)
{
        msg->flags |= MSG_GEN_FLAG_MASK & flags;
}

static inline void zest_msg_set_flags(struct zest_msg *msg, int flags)
{
        msg->flags &= ~MSG_GEN_FLAG_MASK;
        zest_msg_add_flags(msg, flags);
}

static inline void zest_msg_clear_flags(struct zest_msg *msg, int flags)
{
        msg->flags &= ~(MSG_GEN_FLAG_MASK & flags);
}

static inline int zest_msg_get_op_flags(struct zest_msg *msg)
{
        return (msg->flags >> MSG_OP_FLAG_SHIFT);
}

static inline void zest_msg_add_op_flags(struct zest_msg *msg, int flags)
{
        msg->flags |= ((flags & MSG_GEN_FLAG_MASK) << MSG_OP_FLAG_SHIFT);
}

static inline void zest_msg_set_op_flags(struct zest_msg *msg, int flags)
{
        msg->flags &= ~MSG_OP_FLAG_MASK;
        zest_msg_add_op_flags(msg, flags);
}


static inline int
zestrpc_client_receiving_reply (struct zestrpc_request *req)
{
        int           rc;

        spin_lock(&req->rq_lock);
        rc = req->rq_receiving_reply;
        spin_unlock(&req->rq_lock);
        return (rc);
}

static inline int
zestrpc_client_replied (struct zestrpc_request *req)
{
        int           rc;

        spin_lock(&req->rq_lock);
        rc = req->rq_replied;
        spin_unlock(&req->rq_lock);
        return (rc);
}

static inline void
zestrpc_wake_client_req (struct zestrpc_request *req)
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

#define ZEST_FATAL_SIGS (sigmask(SIGKILL) | sigmask(SIGINT) |              \
                           sigmask(SIGTERM) | sigmask(SIGQUIT) |             \
                           sigmask(SIGALRM))

/**
 * __zest_server_wait_event - implement a timed wait using waitqs and pthread_cond_timedwait
 * @wq: the waitq to block on
 * @condition: condition to check on
 * @info: the timeout info strict (l_wait_info)
 * @ret: the return val
 * @excl: unused
 * @lck: optional spinlock used for zest waitq - see zestWaitq.h
 */

#define SVR_TIMEOUT 60
#define  __zest_server_wait_event(wq, condition, info, ret, excl, lck)       \
do {                                                                         \
	time_t __now       = time(NULL);                                     \
	time_t __timeout   = SVR_TIMEOUT;					\
	time_t __then      = 0;                                              \
	int    __timed_out = 0;                                              \
	struct timespec abstime = {0, 0};                                    \
                                                                             \
        ret = 0;                                                             \
	if (condition) break;                                                \
                                                                             \
	while (!(condition)) {                                                 \
                if (__timeout)                                               \
                       abstime.tv_sec = __timeout + __now;                   \
                abstime.tv_nsec = 0;                                         \
		ret = zwaitq_timedwait(wq, lck, &abstime);                   \
		if (ret) {                                                   \
			ret = -ret;                                          \
			break;                                               \
		}		                                             \
		if (condition) break;                                        \
                                                                             \
		if (!__timed_out && info->lwi_timeout != 0) {                \
			__now = time(NULL);                                  \
			__timeout -= __now - __then;                         \
			__then = __now;                                      \
			                                                     \
			if (__timeout > 0) continue;                         \
			__timeout = 0;                                       \
			__timed_out = 1;                                     \
			if (info->lwi_on_timeout == NULL ||                  \
			    info->lwi_on_timeout(info->lwi_cb_data)) {       \
				ret = -ETIMEDOUT;                            \
				break;                                       \
			}                                                    \
		}                                                            \
	}	                                                             \
} while (0)


/**
 * __zest_client_wait_event - the below call is for clients.  Clients are single threaded due to catamount/panther.  This means that clients must block in LNetEQPoll - this occurs in liblustre_wait_event().  A similar model can be used for server threads so long as liblustre_wait_event() is replaced with something that uses timed waitq's.
 *
 */

#define zestrpc_timeout 100
#define __zest_client_wait_event(wq, condition, info, ret, excl)        \
do {                                                                    \
        time_t __timeout = info->lwi_timeout;                           \
        long __now;                                                     \
        long __then = 0;                                                \
        int  __timed_out = 0;                                           \
        int  __interval = zestrpc_timeout;                              \
                                                                        \
        ret = 0;                                                        \
        if (condition)                                                  \
                break;                                                  \
                                                                        \
        if (__timeout != 0)                                             \
                __then = time(NULL);                                    \
        if (__timeout && __timeout < __interval)                        \
                __interval = __timeout;                                 \
        if (info->lwi_interval && info->lwi_interval < __interval)      \
                __interval = info->lwi_interval;                        \
                                                                        \
        while (!(condition)) {                                          \
                zestrpc_wait_event(__interval);				\
                if (condition)                                          \
                        break;                                          \
                                                                        \
                if (!__timed_out && info->lwi_timeout != 0) {           \
                        __now = time(NULL);                             \
                        __timeout -= __now - __then;                    \
                        __then = __now;                                 \
                                                                        \
                        if (__timeout > 0)                              \
                                continue;                               \
                                                                        \
                        __timeout = 0;                                  \
                        __timed_out = 1;                                \
                        if (info->lwi_on_timeout == NULL ||             \
                            info->lwi_on_timeout(info->lwi_cb_data)) {  \
                                ret = -ETIMEDOUT;                       \
                                break;                                  \
                        }                                               \
                }                                                       \
        }                                                               \
} while (0)


#define zcli_wait_event(wq, condition, info)                            \
({                                                                      \
        int                 __ret;                                      \
        struct l_wait_info *__info = (info);                            \
                                                                        \
        __zest_client_wait_event(wq, condition, __info, __ret, 0);      \
        __ret;                                                          \
})

#define zsvr_wait_event(wq, condition, info, lck)                       \
({                                                                      \
        int                 __ret;                                      \
        struct l_wait_info *__info = (info);                            \
                                                                        \
        __zest_server_wait_event(wq, condition, __info, __ret, 0, lck); \
        __ret;                                                          \
})

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

#endif
