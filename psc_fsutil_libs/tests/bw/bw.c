/* $Id$ */

#include <sys/types.h>
#include <sys/syscall.h>

#include <err.h>
#include <stdio.h>

#ifdef HAVE_CPUSET
#include <cpuset.h>
#include <bitmask.h>
#include <numa.h>
#endif

#include <procbridge.h>

#include "pfl.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"
#include "psc_rpc/rsx.h"
#include "psc_rpc/service.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/humanscale.h"
#include "psc_util/iostats.h"
#include "psc_util/log.h"
#include "psc_util/strlcpy.h"
#include "psc_util/thread.h"

#define MAX_NTHR		128
#define MAX_SETSZ		32

/* Thread types */
#define THRT_LND		0	/* networking dev thr */
#define THRT_EQPOLL		1	/* LNetEQPoll */
#define THRT_RPC		2	/* RPC handlers */
#define THRT_RQ			3

/* RPC test service settings */
#define RT_BUFSIZE		384
#define RT_MAXREQSIZE		RT_BUFSIZE
#define RT_MAXREPSIZE		256

#define RT_REQ_PORTAL		17
#define RT_REP_PORTAL		18
#define RT_BULK_PORTAL		19

#define RT_VERSION		0x1

/* Message types */
enum {
	MT_WRITE,
	MT_CONNECT
};

struct msg_req {
	int dummy;
};

struct msg_rep {
	int rc;
};

struct thr_init {
	int n;
};

struct cli_thr {
	struct psc_wait_queue set_wq;
	int nsets;
};

struct svr_thr {
	struct pscrpc_thread prt;
	char *buf;
};

#define cli_thr(thr)	((struct cli_thr *)thr->pscthr_private)
#define svr_thr(thr)	((struct svr_thr *)thr->pscthr_private)

int			 nthr = 8;
int			 nbuf = 4096;
int			 g_bufsz = 1024 * 1024;
int			 g_setsz = 8;
int			 g_maxset = 32;

pscrpc_svc_handle_t	 svc;
struct pscrpc_import	*imp;
struct psc_thread	 eqpollthr;
struct psc_thread	 rqthr;
const char		*progname;
struct iostats		*lst;
int			 doserver;
nodemask_t		 ibnodes;
struct psc_listcache	 requests;

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-b nbuf] [-t nthr] -d\n"
	    "       %s [-m maxset] [-S setsz] [-s bufsz] "
#ifndef HAVE_CPUSET
	    "[-t nthr] "
#endif
	    "server-nid\n",
	    progname, progname);
	exit(1);
}

int
write_cb(struct pscrpc_request *rq, __unusedx void *arg, int status)
{
	if (status)
		psc_fatalx("non-zero status in reply: %d", status);
	if (rq->rq_status)
		psc_fatalx("I/O req had %d return status", rq->rq_status);
	return (0);
}

int
set_cb(__unusedx struct pscrpc_request_set *set, __unusedx void *arg,
    __unusedx int status)
{
	struct psc_thread *thr;
	struct cli_thr *ct;

	thr = pscthr_get();
	ct = cli_thr(thr);
	spinlock(&thr->pscthr_lock);
	ct->nsets--;
	psc_waitq_wakeall(&ct->set_wq);
	freelock(&thr->pscthr_lock);
	return (0);
}

__dead void *
eqpollthr_main(__unusedx void *arg)
{
	for (;;) {
		pscrpc_check_events(100);
		sched_yield();
	}
}

__dead void *
rqthr_main(__unusedx void *arg)
{
	struct pscrpc_request *rq;
	int rc;

	for (;;) {
		rq = lc_getwait(&requests);
		rc = pscrpc_push_req(rq);
		if (rc) {
			DEBUG_REQ(PLL_ERROR, rq,
			    "pscrpc_push_req() failed; rc=%d", rc);
			spinlock(&rq->rq_lock);
			rq->rq_status = rc;
			freelock(&rq->rq_lock);
		}
		sched_yield();
	}
}

__dead void *
client_main(void *arg)
{
	const struct msg_rep *mp;
	int i, rc, n, maxset = g_maxset, bufsz = g_bufsz, setsz = g_setsz;
	struct pscrpc_request_set *set, **sets;
	struct pscrpc_bulk_desc *desc;
	struct pscrpc_request *rq;
	struct psc_thread *thr;
	struct thr_init *ti;
	struct dynarray da;
	struct timespec ts;
	struct cli_thr *ct;
	struct msg_req *mq;
	struct iovec iov;
	nodemask_t nm;
	char *buf;

	ti = arg;
#ifdef HAVE_CPUSET
	nodemask_zero(&nm);
	nodemask_set(&nm, cpuset_p_rel_to_sys_mem(getpid(), ti->n));
printf("node %d\n", ti->n);
printf("node %d maps to %d\n", ti->n, cpuset_p_rel_to_sys_mem(getpid(), ti->n));
	if (numa_run_on_node_mask(&nm) == -1)
		psc_fatal("numa");
	numa_set_membind(&nm);

#endif
	free(ti);

	ct = PSCALLOC(sizeof(*ct));
	psc_waitq_init(&ct->set_wq);

	thr = PSCALLOC(sizeof(*thr));
	pscthr_init(thr, THRT_RPC, NULL, ct, "rpcthr%d", n);

	dynarray_init(&da);

	buf = psc_alloc(bufsz, PAF_PAGEALIGN);
	for (;;) {
		sets = dynarray_get(&da);
		n = dynarray_len(&da);
		for (i = 0; i < n; i++)
			if (pscrpc_set_finalize(sets[i],
			    0, 1) == 0)
				dynarray_remove(&da, sets[i]);
		spinlock(&thr->pscthr_lock);
		if (ct->nsets >= maxset) {
			ts.tv_sec = 0;
			ts.tv_nsec = 5000 * 1000;
			psc_waitq_timedwait(&ct->set_wq,
			    &thr->pscthr_lock, &ts);
			continue;
		}
		ct->nsets++;
		freelock(&thr->pscthr_lock);
		set = pscrpc_prep_set();
		dynarray_add(&da, set);
		set->set_interpret = set_cb;
		for (i = 0; i < setsz; i++) {
			if ((rc = RSX_NEWREQ(imp, RT_VERSION,
			    MT_WRITE, rq, mq, mp)) != 0)
				psc_fatalx("rsx_newreq: %d", rc);
			rq->rq_interpret_reply = write_cb;
			iov.iov_base = buf;
			iov.iov_len = bufsz;
			rsx_bulkclient(rq, &desc, BULK_GET_SOURCE,
			    RT_BULK_PORTAL, &iov, 1);
			pscrpc_set_add_new_req(set, rq);
			lc_addtail(&requests, rq);
		}
	}
}

int
server_write_handler(struct pscrpc_request *rq)
{
	const struct msg_req *mq;
	struct pscrpc_bulk_desc *desc;
	struct psc_thread *thr;
	struct msg_rep *mp;
	struct svr_thr *st;
	struct iovec iov;
	int rc;

	thr = pscthr_get();
	st = svr_thr(thr);
	if (st->buf == NULL)
		st->buf = psc_alloc(g_bufsz, PAF_PAGEALIGN);

	RSX_ALLOCREP(rq, mq, mp);
	iov.iov_base = st->buf;
	iov.iov_len = g_bufsz;
	rc = rsx_bulkserver(rq, &desc, BULK_GET_SINK, RT_BULK_PORTAL,
	    &iov, 1);
	if (rc)
		psc_fatalx("bulk_recv: %s", strerror(-rc));
	pscrpc_free_bulk(desc);
	return (0);
}

int
server_connect_handler(struct pscrpc_request *rq)
{
	const struct msg_req *mq;
	struct msg_rep *mp;

	RSX_ALLOCREP(rq, mq, mp);
	return (0);
}

int
server_rpc_handler(struct pscrpc_request *rq)
{
	int rc;

	switch (rq->rq_reqmsg->opc) {
	case MT_WRITE:
		rc = rq->rq_status = server_write_handler(rq);
		break;
	case MT_CONNECT:
		rc = rq->rq_status = server_connect_handler(rq);
		break;
	default:
		psc_fatalx("unexpected opcode");
	}
	target_send_reply_msg(rq, rc, 0);
	return (0);
}

extern void *nal_thread(void *);

void *
lndthr_begin(void *arg)
{
	struct psc_thread *thr;

	thr = arg;
	return (nal_thread(thr->pscthr_private));
}

void
lnet_spawnthr(pthread_t *t, void *(*startf)(void *), void *arg)
{
	extern int tcpnal_instances;
	struct psc_thread *pt;

	if (doserver)
		lst = ((bridge)arg)->b_ni->ni_recvstats;
	else
		lst = ((bridge)arg)->b_ni->ni_sendstats;

	if (startf != nal_thread)
		psc_fatalx("unknown LNET start routine");

	pt = PSCALLOC(sizeof(*pt));
	pscthr_init(pt, THRT_LND, lndthr_begin, arg, "lndthr%d",
	    tcpnal_instances - 1);
	*t = pt->pscthr_pthread;
	pt->pscthr_private = arg;
}

void
sigsegv(__unusedx int sig)
{
	char buf[50];

	snprintf(buf, sizeof(buf), "gstack %d", getpid());
	system(buf);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *endp, ratebuf[PSC_CTL_HUMANBUF_SZ];
	lnet_process_id_t server_id, my_id;
	struct timeval tv, lastv, intv;
	struct pscrpc_request *rq;
	struct thr_init *ti;
	struct msg_req *mq;
	struct msg_rep *mp;
	int nb, i, rc, c;
	double rate, tm;
	pthread_t pthr;
	long l;

	signal(SIGSEGV, sigsegv);

#ifdef HAVE_CPUSET
	unsigned int nnodes;
	nodemask_t nonibnodes;
	struct bitmask *bm;
	struct cpuset *cs;
	int j;
#endif

	doserver = 0;
	progname = argv[0];
	while ((c = getopt(argc, argv, "b:dm:S:s:t:w:")) != -1)
		switch (c) {
		case 'b':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > INT_MAX ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid nbuf: %s", optarg);
			nbuf = (int)l;
			break;
		case 'd':
			doserver = 1;
			break;
		case 'm':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > INT_MAX ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid maxset: %s", optarg);
			g_maxset = (int)l;
			break;
		case 'S':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > MAX_SETSZ ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid setsz: %s", optarg);
			g_setsz = (int)l;
			break;
		case 's':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > INT_MAX ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid bufsz: %s", optarg);
			g_bufsz = (int)l;
			break;
		case 't':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > MAX_NTHR ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid nthr: %s", optarg);
			nthr = (int)l;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#ifdef HAVE_CPUSET
#define PATH_CS_ROOT "/"
	if (cpuset_move(getpid(), PATH_CS_ROOT) == -1)
		psc_fatal("cpuset_move pid %d to %s:",
		    getpid(), PATH_CS_ROOT);
#endif

	lnet_thrspawnf = lnet_spawnthr;
	pfl_init(256);

	if (doserver) {
		if (argc)
			usage();

		if (setenv("TCPLND_SERVER", "1", 1) == -1)
			psc_fatal("setenv");

		if (pscrpc_init_portals(PSC_SERVER))
			psc_fatal("pscrpc_init_portals");

		svc.svh_nbufs = nbuf;
		svc.svh_bufsz = RT_BUFSIZE;
		svc.svh_reqsz = RT_MAXREQSIZE;
		svc.svh_repsz = RT_MAXREPSIZE;
		svc.svh_req_portal = RT_REQ_PORTAL;
		svc.svh_rep_portal = RT_REP_PORTAL;
		svc.svh_nthreads = nthr;
		svc.svh_handler = server_rpc_handler;
		svc.svh_type = THRT_RPC;
		strlcpy(svc.svh_svc_name, "svc",
		    sizeof(svc.svh_svc_name));
		pscrpc_thread_spawn(&svc, struct svr_thr);
	} else {
		if (argc != 1)
			usage();

		numa_exit_on_error = 1;
		nonibnodes = numa_all_nodes;
		for (i = 0; i < NUMA_NUM_NODES; i++)
			if (nodemask_isset(&ibnodes, i))
				nodemask_clr(&nonibnodes, i);
		extern void get_ib_node_neighbors(nodemask_t *);
		get_ib_node_neighbors(&ibnodes);

		if (numa_migrate_pages(getpid(), &nonibnodes, &ibnodes) == -1)
			psc_fatal("cpuset_migrate_pages");
		numa_set_membind(&ibnodes);

		if (pscrpc_init_portals(PSC_CLIENT))
			psc_fatal("pscrpc_init_portals");

		if (LNetGetId(1, &my_id))
			psc_fatalx("LNetGetId() failed");

		server_id.pid = 0;
		server_id.nid = libcfs_str2nid(argv[0]);
		if (server_id.nid == LNET_NID_ANY)
			psc_fatalx("invalid server name %s", argv[0]);

		/*
		 * XXX bound the process doing lnet_send() to this same
		 * node where the memory resides.
		 */
		imp = new_import();
		imp->imp_client = PSCALLOC(sizeof(*imp->imp_client));
		imp->imp_client->cli_request_portal = RT_REQ_PORTAL;
		imp->imp_client->cli_reply_portal = RT_REP_PORTAL;
		imp->imp_max_retries = 2;

		imp->imp_connection = pscrpc_get_connection(server_id,
		    my_id.nid, NULL);
		imp->imp_connection->c_peer.pid = PSC_SVR_PID;

		pscthr_init(&eqpollthr, THRT_EQPOLL,
		    eqpollthr_main, NULL, "eqpollthr");
		lc_init(&requests, struct pscrpc_request, rq_history_list);
		pscthr_init(&rqthr, THRT_RQ, rqthr_main, NULL, "rqthr");

		rc = RSX_NEWREQ(imp, RT_VERSION,
		    MT_CONNECT, rq, mq, mp);
		if (rc)
			psc_fatalx("rsx_newreq: %d", rc);
		rc = RSX_WAITREP(rq, mp);
		if (rc)
			psc_fatalx("rsx_waitrep: %d", rc);

		imp->imp_connection->c_peer.pid = rq->rq_peer.pid;
		imp->imp_state = PSC_IMP_FULL;
		imp->imp_failed = 0;
		pscrpc_req_finished(rq);

		for (i = 0; i < nthr; i++) {
			ti = PSCALLOC(sizeof(*ti));
			ti->n = i;
			rc = pthread_create(&pthr, NULL, client_main,
			    ti);
			if (rc)
				psc_fatalx("pthread_create: %s",
				    strerror(rc));
		}
	}
	printf("time (s)       lnet-rate =======\n");
	if (gettimeofday(&lastv, NULL) == -1)
		psc_fatal("gettimeofday");
	for (;;) {
		if (gettimeofday(&tv, NULL) == -1)
			psc_fatal("gettimeofday");
		if (tv.tv_sec != lastv.tv_sec) {
			timersub(&tv, &lastv, &intv);
			lastv = tv;
			tm = intv.tv_sec * (uint64_t)1000000 +
			    intv.tv_usec;
			printf("\r%2.5fs ", tm / 1000000);

			nb = 0;
			nb = atomic_xchg(&lst->ist_bytes_intv, nb);
			rate = nb / (tm / 1000000);
			psc_humanscale(ratebuf, rate);
			printf(" %14.3f %7s", rate, ratebuf);
			fflush(stdout);
		}
		sleep(1);
	}
}
