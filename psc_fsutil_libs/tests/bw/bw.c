/* $Id$ */

#include <sys/types.h>
#include <sys/syscall.h>

#include <err.h>
#include <stdio.h>

#ifdef HAVE_CPUSET
#include <cpuset.h>
#include <bitmask.h>
#include <numaif.h>
#endif

#include <procbridge.h>

#include "pfl.h"
#include "psc_rpc/rpc.h"
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

struct mythr {
	struct pscrpc_thread prt;
#ifdef HAVE_CPUSET
	int bindnode;
#endif
	char *buf;
	int maxset;
	int nsets;
	struct psc_wait_queue set_wq;
	psc_spinlock_t setlock;
};

#define mythr(thr)	((struct mythr *)thr->pscthr_private)

int			 nthr = 8;
//int			 maxset = 512;
int			 nbuf = 4096;
int			 setsz = 8;
int			 bufsz = 1024 * 1024;
atomic_t		 nsets = ATOMIC_INIT(0);

lnet_process_id_t	 mynid;
pscrpc_svc_handle_t	 svc;
struct pscrpc_import	*imp;
struct psc_thread	 eqpollthr;
const char		*progname;
struct iostats		 ist, *lst;
int			 doserver;

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-b nbuf] [-t nthr] -d\n"
	    "       %s [-m maxset] [-S setsz] [-s bufsz] [-t nthr] server-nid\n",
	    progname, progname);
	exit(1);
}

#ifdef HAVE_CPUSET
int
bindnode(int nid)
{
	struct bitmask *mems, *cpus;
	unsigned int i;
	cpu_set_t m;
	int rc;

	rc = 0;
	cpus = bitmask_alloc(cpuset_cpus_nbits());
	mems = bitmask_alloc(cpuset_mems_nbits());
	bitmask_clearall(cpus);
	bitmask_clearall(mems);

	bitmask_setbit(mems, nid);
	if (cpuset_localcpus(mems, cpus) == -1) {
		rc = -1;
		goto out;
	}
	CPU_ZERO(&m);
	for (i = 0; i < bitmask_nbits(cpus); i++)
		if (bitmask_isbitset(cpus, i))
			CPU_SET(i, &m);

	if (sched_setaffinity(syscall(SYS_gettid),
	    sizeof(m), &m) == -1) {
		rc = -1;
		goto out;
	}
	if (set_mempolicy(MPOL_DEFAULT, NULL, 0) == -1)
		rc = -1;
 out:
	bitmask_free(mems);
	bitmask_free(cpus);
	return (rc);
}
#endif

int
write_cb(struct pscrpc_request *rq, __unusedx void *arg, int status)
{
	atomic_add(bufsz, &ist.ist_bytes_intv);
	if (status)
		psc_fatalx("non-zero status in reply: %d", status);
	if (rq->rq_status)
		psc_fatalx("I/O req had %d return status", rq->rq_status);
	return (0);
}

int
set_cb(__unusedx struct pscrpc_request_set *set, void *arg,
    __unusedx int status)
{
	struct mythr *mt = arg;

	atomic_dec(&nsets);
	spinlock(&mt->setlock);
	mt->nsets--;
	psc_waitq_wakeall(&mt->set_wq);
	freelock(&mt->setlock);
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
client_main(void *arg)
{
	struct psc_thread *thr = arg;
	const struct msg_rep *mp;
	struct pscrpc_bulk_desc *desc;
	struct pscrpc_request_set *set, **sets;
	struct pscrpc_request *rq;
	struct msg_req *mq;
	struct dynarray da;
	struct timespec ts;
	struct iovec iov;
	struct mythr *mt;
	int i, rc, n;
	char *buf;

	mt = mythr(thr);
	dynarray_init(&da);

#ifdef HAVE_CPUSET
	/* bind to node zero which has IB */
	if (bindnode(mt->bindnode) == -1)
		psc_fatal("bindnode %d", mt->bindnode);
#endif

	buf = psc_alloc(bufsz, PAF_PAGEALIGN);
	for (;;) {
		sets = dynarray_get(&da);
		n = dynarray_len(&da);
		for (i = 0; i < n; i++)
			if (pscrpc_set_finalize(sets[i],
			    0, 1) == 0)
				dynarray_remove(&da, sets[i]);
		spinlock(&mt->setlock);
		if (mt->nsets >= mt->maxset) {
			ts.tv_sec = 0;
			ts.tv_nsec = 5000 * 1000;
			psc_waitq_timedwait(&mt->set_wq,
			    &mt->setlock, &ts);
			continue;
		}
		atomic_inc(&nsets);
		mt->nsets++;
		freelock(&mt->setlock);
		set = pscrpc_prep_set();
		dynarray_add(&da, set);
		set->set_interpret = set_cb;
		set->set_arg = mt;
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
			rc = pscrpc_push_req(rq);
			if (rc)
				psc_fatalx("pscrpc_push_req: %d", rc);
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
	struct mythr *mt;
	struct iovec iov;
	int rc;

	thr = pscthr_get();
	mt = mythr(thr);
	if (mt->buf == NULL)
		mt->buf = psc_alloc(bufsz, PAF_PAGEALIGN);

	RSX_ALLOCREP(rq, mq, mp);
	iov.iov_base = mt->buf;
	iov.iov_len = bufsz;
	rc = rsx_bulkserver(rq, &desc, BULK_GET_SINK, RT_BULK_PORTAL,
	    &iov, 1);
	if (rc)
		psc_fatalx("bulk_recv: %s", strerror(-rc));
	pscrpc_free_bulk(desc);

	atomic_add(bufsz, &ist.ist_bytes_intv);
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

#ifdef HAVE_CPUSET
	/* bind to node zero which has IB */
	if (bindnode(0) == -1)
		psc_fatal("bindnode %d", 0);
#endif

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

int
main(int argc, char *argv[])
{
	char *endp, ratebuf[PSC_CTL_HUMANBUF_SZ];
	int maxset = 32, nb, i, rc, c;
	lnet_process_id_t server_id;
	struct pscrpc_request *rq;
	struct timeval tv, lastv;
	struct msg_req *mq;
	struct msg_rep *mp;
	struct mythr *mt;
	double rate, tm;
	long l;

#ifdef HAVE_CPUSET
	unsigned int nnodes;
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
			maxset = (int)l;
			break;
		case 'S':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > MAX_SETSZ ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid setsz: %s", optarg);
			setsz = (int)l;
			break;
		case 's':
			endp = NULL;
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > INT_MAX ||
			    endp == optarg || *endp != '\0')
				errx(1, "invalid bufsz: %s", optarg);
			bufsz = (int)l;
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
	iostats_init(&ist, "ist");

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
		pscrpc_thread_spawn(&svc, struct mythr);
	} else {
		if (argc != 1)
			usage();

		if (pscrpc_init_portals(PSC_CLIENT))
			psc_fatal("pscrpc_init_portals");

		if (LNetGetId(1, &mynid))
			psc_fatalx("LNetGetId() failed");

		server_id.pid = 0;
		server_id.nid = libcfs_str2nid(argv[0]);
		if (server_id.nid == LNET_NID_ANY)
			psc_fatalx("invalid server name %s", argv[0]);

		imp = new_import();
		imp->imp_client = PSCALLOC(sizeof(*imp->imp_client));
		imp->imp_client->cli_request_portal = RT_REQ_PORTAL;
		imp->imp_client->cli_reply_portal = RT_REP_PORTAL;
		imp->imp_max_retries = 2;

		imp->imp_connection = pscrpc_get_connection(server_id,
		    mynid.nid, NULL);
		imp->imp_connection->c_peer.pid = PSC_SVR_PID;

		pscthr_init(&eqpollthr, THRT_EQPOLL,
		    eqpollthr_main, NULL, "eqpollthr");

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

#ifdef HAVE_CPUSET
		nnodes = cpuset_mems_nbits();
		cs = cpuset_alloc();
		bm = bitmask_alloc(nnodes);
#define PATH_CS_ROOT "/"
		if (cpuset_query(cs, PATH_CS_ROOT) == -1)
			psc_fatalx("cpuset_query");
		if (cpuset_getmems(cs, bm) == -1)
			psc_fatalx("cpuset_getmems");
		for (i = 0; i < (int)nnodes; i++) {
			if (bitmask_isbitset(bm, i)) {
				bitmask_clearbit(bm, i);
#define THR_PER_NODE 1
				for (j = 0; j < THR_PER_NODE; j++) {
					mt = PSCALLOC(sizeof(*mt));
					mt->bindnode = i;
					mt->maxset = maxset;
					psc_waitq_init(&mt->set_wq);
					LOCK_INIT(&mt->setlock);
					pscthr_init(PSCALLOC(sizeof(struct psc_thread)),
					    THRT_RPC, client_main, mt, "rpcthr%d", i);
				}
			}
		}
		bitmask_free(bm);
		cpuset_free(cs);
#else
		for (i = 0; i < nthr; i++) {
			mt = PSCALLOC(sizeof(*mt));
			mt->maxset = maxset;
			psc_waitq_init(&mt->set_wq);
			LOCK_INIT(&mt->setlock);
			pscthr_init(PSCALLOC(sizeof(struct psc_thread)),
			    THRT_RPC, client_main, mt, "rpcthr%d", i);
		}
#endif
	}
	printf("time (s)        app-rate =======      lnet-rate =======   nsets\n");
	if (gettimeofday(&lastv, NULL) == -1)
		psc_fatal("gettimeofday");
	for (;;) {
		if (gettimeofday(&tv, NULL) == -1)
			psc_fatal("gettimeofday");
		if (tv.tv_sec != lastv.tv_sec) {
			timersub(&tv, &lastv, &ist.ist_intv);
			lastv = tv;
			tm = ist.ist_intv.tv_sec * (uint64_t)1000000 +
			    ist.ist_intv.tv_usec;
			printf("\r%2.5fs ", tm / 1000000);

			nb = 0;
			nb = atomic_xchg(&ist.ist_bytes_intv, nb);
			rate = nb / (tm / 1000000);
			psc_humanscale(ratebuf, rate);
			printf(" %14.3f %7s", rate, ratebuf);

			nb = 0;
			nb = atomic_xchg(&lst->ist_bytes_intv, nb);
			rate = nb / (tm / 1000000);
			psc_humanscale(ratebuf, rate);
			printf(" %14.3f %7s", rate, ratebuf);

			printf(" %7d", atomic_read(&nsets));
			fflush(stdout);
		}
		sleep(1);
	}
}
