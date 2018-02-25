/* $Id$ */
/*
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright 2006 Ricardo Correia.
 * Use is subject to license terms.
 */

#include <sys/param.h>
#include <sys/poll.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/mount.h>

#ifdef HAVE_NO_POLL_DEV
#include <sys/select.h>
#endif

#include <errno.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <fuse_lowlevel.h>

#include "pfl/alloc.h"
#include "pfl/ctl.h"
#include "pfl/ctlsvr.h"
#include "pfl/dynarray.h"
#include "pfl/fs.h"
#include "pfl/fsmod.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/pool.h"
#include "pfl/random.h"
#include "pfl/sys.h"
#include "pfl/waitq.h"
#include "pfl/workthr.h"

#ifdef __LP64__
#  include "pfl/hashtbl.h"
#endif

#define MAX_FILESYSTEMS			5
#define MAX_FDS				(MAX_FILESYSTEMS + 1)

/*
 *
 */
#define fusefi_to_pfh(fi)		((struct pflfs_filehandle *)(void *)(uintptr_t)(fi)->fh)
#define fusefi_to_pri(fi)		fusefi_to_pfh(fi)->pfh_mod_data
#define pfr_to_fusefi(pfr)		((struct fuse_file_info *)(pfr)->pfr_ufsi_fhdata)
#define pfr_to_pfh(pfr)			fusefi_to_pfh(pfr_to_fusefi(pfr))

#define fusefi_stash_pri(fi, data)					\
	do {								\
		struct pflfs_filehandle *_pfh;				\
									\
		_pfh = psc_pool_get(pflfs_filehandle_pool);		\
		memset(_pfh, 0, sizeof(*_pfh));				\
		INIT_LISTENTRY(&_pfh->pfh_lentry);			\
		_pfh->pfh_mod_data = data;				\
		(fi)->fh = (unsigned long)_pfh;				\
		pll_add(&pflfs_filehandles, _pfh);			\
	} while (0)

#ifdef __LP64__
#  define INUM_FUSE2PSCFS(inum)		(inum)
#  define INUM_FUSE2PSCFS_DEL(inum)	(inum)
#  define INUM_PSCFS2FUSE(inum, tmo)	(inum)
#else
#  define INUM_FUSE2PSCFS(inum)		pscfs_inum_fuse2pscfs((inum), 0)
#  define INUM_FUSE2PSCFS_DEL(inum)	pscfs_inum_fuse2pscfs((inum), 1)
#  define INUM_PSCFS2FUSE(inum, tmo)	pscfs_inum_pscfs2fuse((inum), (tmo))

struct pscfs_inumcol {
	pscfs_inum_t		 pfic_pscfs_inum;
	uint64_t		 pfic_key;		/* fuse inum */
	psc_atomic32_t		 pfic_refcnt;
	struct timeval		 pfic_extime;		/* when fuse expires it */
	struct psc_listentry	 pfic_lentry;		/* pool */
	struct pfl_hashentry	 pfic_hentry;
};

static struct psc_poolmaster	 pflfs_inumcol_poolmaster;
static struct psc_poolmgr	*pflfs_inumcol_pool;
static struct psc_hashtbl	 pflfs_inumcol_hashtbl;
#endif

typedef struct {
	int			 fd;
	size_t			 bufsize;
	struct fuse_chan	*ch;
	struct fuse_session	*se;
	int			 mntlen;
} fuse_fs_info_t;

double				 pscfs_entry_timeout;
double				 pscfs_attr_timeout;
struct psc_poolmaster		 pflfs_req_poolmaster;
struct psc_poolmgr		*pflfs_req_pool;
struct psc_dynarray		 pscfs_modules;
extern struct pscfs		 pscfs_default_ops;

struct psc_poolmaster		 pflfs_filehandle_poolmaster;
struct psc_poolmgr		*pflfs_filehandle_pool;

struct psc_lockedlist pflfs_requests = PLL_INIT(
    &pflfs_requests, struct pscfs_req, pfr_lentry);

struct psc_lockedlist pflfs_filehandles = PLL_INIT(
    &pflfs_filehandles, struct pflfs_filehandle, pfh_lentry);

int				 pscfs_exit_fuse_listener;
int				 newfs_fd[2];
int				 pflfs_nfds;
struct pollfd			 pflfs_fds[MAX_FDS];
static fuse_fs_info_t		 pflfs_fsinfo[MAX_FDS];
static char			*mountpoints[MAX_FDS];
struct fuse_session		*fuse_session;

#ifdef HAVE_NO_POLL_DEV
fd_set				*pscfs_fdset;
fd_set				*pscfs_fdset_rd;
size_t				 pscfs_fdset_size;
#endif

static void
pscfs_fuse_interrupt(__unusedx fuse_req_t req, void *d)
{
	struct pscfs_req *pfr = d;
	struct psc_thread *thr;

	thr = pfr->pfr_thread;
	pfr->pfr_interrupted = 1;
	OPSTAT_INCR("msl.fuse-intr");
	psclog_diag("op interrupted, thread = %p, pfr = %p, name = %s", 
		thr, pfr, pfr->pfr_opname);
}

int
pscfs_fuse_newfs(const char *mntpoint, struct fuse_chan *ch)
{
	fuse_fs_info_t info;

	memset(&info, 0, sizeof(info));

	info.fd = fuse_chan_fd(ch);
	info.bufsize = fuse_chan_bufsize(ch);
	info.ch = ch;
	info.se = fuse_chan_session(ch);
	info.mntlen = strlen(mntpoint);

	if (write(newfs_fd[1], &info, sizeof(info)) !=
	    sizeof(info)) {
		perror("Warning (while writing pflfs_fsinfo to newfs_fd)");
		return (-1);
	}

	if (write(newfs_fd[1], mntpoint, info.mntlen) !=
	    info.mntlen) {
		perror("Warning (while writing mntpoint to newfs_fd)");
		return (-1);
	}
	return (0);
}

static int
fd_read_loop(int fd, void *buf, int bytes)
{
	int read_bytes = 0;
	int left_bytes = bytes;

	while (left_bytes > 0) {
		int ret = read(fd, ((char *) buf) + read_bytes, left_bytes);
		if (ret == 0)
			return (-1);

		if (ret == -1) {
			if (errno == EINTR)
				continue;
			perror("read");
			return (-1);
		}
		read_bytes += ret;
		left_bytes -= ret;
	}
	return (0);
}

void
pscfs_fuse_addfd(int fd)
{
#ifdef HAVE_NO_POLL_DEV
	size_t sz;

	sz = howmany(fd + 1, NFDBITS) * sizeof(fd_mask);
	if (sz > pscfs_fdset_size) {
		pscfs_fdset = psc_realloc(pscfs_fdset, sz, 0);
		pscfs_fdset_rd = psc_realloc(pscfs_fdset_rd, sz, 0);
		pscfs_fdset_size = sz;
	}
	FD_SET(fd, pscfs_fdset);
	FD_SET(fd, pscfs_fdset_rd);
#endif
	pflfs_fds[pflfs_nfds].fd = fd;
	pflfs_fds[pflfs_nfds].events = POLLIN;
	pflfs_nfds++;
}

/*
 * Add a new filesystem/file descriptor to the poll set
 * Must be called with mtx locked
 */
static void
pscfs_fuse_new(void)
{
	fuse_fs_info_t fs;

	/*
	 * This should never fail (famous last words) since the fd
	 * is only closed in fuse_listener_exit()
	 *
	 * Receive fuse_fs_info_t() from pscfs_fuse_newfs().
	 */
	psc_assert(fd_read_loop(pflfs_fds[0].fd, &fs,
	    sizeof(fuse_fs_info_t)) == 0);

	char *mntpoint = PSCALLOC(fs.mntlen + 1);
	if (mntpoint == NULL) {
		fprintf(stderr, "Warning: out of memory!\n");
		return;
	}

	psc_assert(fd_read_loop(pflfs_fds[0].fd, mntpoint, fs.mntlen) == 0);

	mntpoint[fs.mntlen] = '\0';

	if (pflfs_nfds == MAX_FDS) {
		fprintf(stderr, "Warning: filesystem limit (%i) "
		    "reached, unmounting..\n", MAX_FILESYSTEMS);
		fuse_unmount(mntpoint, fs.ch);
		PSCFREE(mntpoint);
		return;
	}

	psclog_info("adding filesystem %i at mntpoint %s", pflfs_nfds,
	    mntpoint);

	pflfs_fsinfo[pflfs_nfds] = fs;
	mountpoints[pflfs_nfds] = mntpoint;
	pscfs_fuse_addfd(fs.fd);
}

/*
 * Delete a filesystem/file descriptor from the poll set
 * Must be called with mtx locked
 */
static void
pscfs_fuse_destroy(int i)
{
#ifdef DEBUG
	fprintf(stderr, "Filesystem %i (%s) is being unmounted\n", i,
	    mountpoints[i]);
#endif
	fuse_session_reset(pflfs_fsinfo[i].se);
	fuse_session_destroy(pflfs_fsinfo[i].se);
	close(pflfs_fds[i].fd);
	pflfs_fds[i].fd = -1;

#ifdef HAVE_NO_POLL_DEV
	FD_CLR(pflfs_fds[i].fd, pscfs_fdset);
#endif

	PSCFREE(mountpoints[i]);
}

void
pscfs_fuse_listener_loop(__unusedx struct psc_thread *thr)
{
	static psc_spinlock_t lock = SPINLOCK_INIT;
	static struct psc_waitq wq = PSC_WAITQ_INIT("fuse-loop");
	static int busy;

	size_t bufsize = 0;
	char *buf = NULL;

	spinlock(&lock);
	while (busy) {
		psc_waitq_wait(&wq, &lock);
		spinlock(&lock);
	}
	busy = 1;
	freelock(&lock);

	while (!pscfs_exit_fuse_listener) {
		int i;

#ifdef HAVE_NO_POLL_DEV
		struct timeval tv = { 1, 0 };

		FD_COPY(pscfs_fdset, pscfs_fdset_rd);
		int ret = select(MAX_FDS, pscfs_fdset_rd, NULL,
		    NULL, &tv);
		if (ret == 0 || (ret == -1 && errno == EINTR))
			continue;

		if (ret == -1) {
			perror("select");
			continue;
		}
#else
		int ret = poll(pflfs_fds, pflfs_nfds, 1000);
		if (ret == 0 || (ret == -1 && errno == EINTR))
			continue;

		if (ret == -1) {
			perror("poll");
			continue;
		}
#endif

		int oldfds = pflfs_nfds;

		for (i = 0; i < oldfds; i++) {
#ifdef HAVE_NO_POLL_DEV
			if (!FD_ISSET(pflfs_fds[i].fd, pscfs_fdset_rd))
				continue;
#else
			short rev = pflfs_fds[i].revents;

			if (rev == 0)
				continue;

			pflfs_fds[i].revents = 0;

			psc_assert((rev & POLLNVAL) == 0);

			if (!(rev & POLLIN) &&
			    !(rev & POLLERR) && !(rev & POLLHUP))
				continue;
#endif

			if (i == 0) {
				pscfs_fuse_new();
			} else {
				/* Handle request */

				if (pflfs_fsinfo[i].bufsize > bufsize) {
					char *new_buf = realloc(buf, pflfs_fsinfo[i].bufsize);
					if (new_buf == NULL) {
						fprintf(stderr, "Warning: out of memory!\n");
						continue;
					}
					buf = new_buf;
					bufsize = pflfs_fsinfo[i].bufsize;
				}

				int res = fuse_chan_recv(&pflfs_fsinfo[i].ch,
				    buf, pflfs_fsinfo[i].bufsize);
				if (res == -1 || fuse_session_exited(pflfs_fsinfo[i].se)) {
					pscfs_fuse_destroy(i);
					continue;
				}

				if (res == 0)
					continue;

				struct fuse_session *se = pflfs_fsinfo[i].se;
				struct fuse_chan *ch = pflfs_fsinfo[i].ch;

				/*
				 * While we process this request, we let another
				 * thread receive new events
				 */
				spinlock(&lock);
				busy = 0;
				psc_waitq_wakeone(&wq);
				freelock(&lock);

				fuse_session_process(se, buf, res, ch);

				/* Acquire the mutex before proceeding */
				spinlock(&lock);
				while (busy) {
					psc_waitq_wait(&wq, &lock);
					spinlock(&lock);
				}
				busy = 1;
				freelock(&lock);

				/*
				 * At this point, we can no longer trust oldfds
				 * to be accurate, so we exit this loop
				 */
				break;
			}
		}

		/* Free the closed file descriptors entries */
		int read_ptr, write_ptr = 0;
		for (read_ptr = 0; read_ptr < pflfs_nfds; read_ptr++) {
			if (pflfs_fds[read_ptr].fd == -1)
				continue;
			if (read_ptr != write_ptr) {
				pflfs_fds[write_ptr] = pflfs_fds[read_ptr];
				pflfs_fsinfo[write_ptr] = pflfs_fsinfo[read_ptr];
				mountpoints[write_ptr] = mountpoints[read_ptr];
			}
			write_ptr++;
		}
		pflfs_nfds = write_ptr;
	}

	spinlock(&lock);
	busy = 0;
	psc_waitq_wakeone(&wq);
	freelock(&lock);
}

#ifdef PFL_CTL
#ifdef HAVE_FUSE_DEBUGLEVEL
void
pscfs_ctlparam_fuse_debug_get(char buf[PCP_VALUE_MAX])
{
	snprintf(buf, PCP_VALUE_MAX, "%d",
	    fuse_lowlevel_getdebug(fuse_session));
}

int
pscfs_ctlparam_fuse_debug_set(const char *value)
{
	char *endp;
	long val;

	endp = NULL;
	val = strtol(value, &endp, 10);
	if (val < 0 || val > 1 ||
	    endp == value || *endp != '\0')
		return (-1);
	fuse_lowlevel_setdebug(fuse_session, val);
	return (0);
}
#endif

void
pscfs_ctlparam_fuse_version_get(char buf[PCP_VALUE_MAX])
{
	snprintf(buf, PCP_VALUE_MAX, "%d.%d",
	    FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
}

void
pscfs_ctlparam_entry_timeout_get(char buf[PCP_VALUE_MAX])
{
	snprintf(buf, PCP_VALUE_MAX, "%g", pscfs_entry_timeout);
}

int
pscfs_ctlparam_entry_timeout_set(const char *s)
{
	double val;
	char *endp;

	endp = NULL;
	val = strtod(s, &endp);
	if (val < 0. || val > 60. ||
	    endp == s || *endp != '\0')
		return (-1);
	pscfs_entry_timeout = val;
	return (0);
}

void
pscfs_ctlparam_attr_timeout_get(char buf[PCP_VALUE_MAX])
{
	snprintf(buf, PCP_VALUE_MAX, "%g", pscfs_attr_timeout);
}

int
pscfs_ctlparam_attr_timeout_set(const char *s)
{
	char *endp;
	long val;

	endp = NULL;
	val = strtod(s, &endp);
	if (val < 0. || val > 60. ||
	    endp == s || *endp != '\0')
		return (-1);
	pscfs_attr_timeout = val;
	return (0);
}
#endif

void
pfl_fuse_atexit(void)
{
	int i;

	for (i = 1; i < pflfs_nfds; i++) {
		if (pflfs_fds[i].fd == -1)
			continue;
#if defined(HAVE_UMOUNT2)
		umount2(mountpoints[i], MNT_DETACH);
#elif defined(HAVE_UNMOUNT)
		unmount(mountpoints[i], MNT_FORCE);
#endif
	}
}

int
pscfs_main(int nthr, const char *thrname)
{
	struct psc_thread *thr;
	struct pfl_fsthr *pft;
	struct pscfs *m;
	pthread_t *thrv;
	int i;

	pflfs_module_add(PFLFS_MOD_POS_LAST, &pscfs_default_ops);

#ifndef __LP64__
	struct pscfs_inumcol *pfic;

#define INUMCOL_SZ (4096 - 1)
	psc_poolmaster_init(&pflfs_inumcol_poolmaster, struct
	    pscfs_inumcol, pfic_lentry, PPMF_AUTO, INUMCOL_SZ,
	    INUMCOL_SZ / 2, INUMCOL_SZ * 2, NULL,
	    "inumcol");
	pflfs_inumcol_pool = psc_poolmaster_getmgr(&pflfs_inumcol_poolmaster);
	psc_hashtbl_init(&pflfs_inumcol_hashtbl, 0,
	    struct pscfs_inumcol, pfic_key, pfic_hentry, INUMCOL_SZ * 4,
	    NULL, "inumcol");

	pfic = psc_pool_get(pflfs_inumcol_pool);
	memset(pfic, 0, sizeof(*pfic));
	psc_hashent_init(&pflfs_inumcol_hashtbl, pfic);
	pfic->pfic_pscfs_inum = 1;
	pfic->pfic_key = 1;
	psc_atomic32_set(&pfic->pfic_refcnt, 1);
	psc_hashtbl_add_item(&pflfs_inumcol_hashtbl, pfic);
#endif

	psc_poolmaster_init(&pflfs_req_poolmaster, struct pscfs_req,
	    pfr_lentry, PPMF_AUTO, 64, 64, 1024, NULL,
	    "fsrq");
	pflfs_req_pool = psc_poolmaster_getmgr(&pflfs_req_poolmaster);

	psc_poolmaster_init(&pflfs_filehandle_poolmaster,
	    struct pflfs_filehandle, pfh_lentry, PPMF_AUTO, 64, 64,
	    0, NULL, "fh");
	pflfs_filehandle_pool = psc_poolmaster_getmgr(
	    &pflfs_filehandle_poolmaster);

#ifdef PFL_CTL
	/* XXX: add max_fuse_iosz */
#ifdef HAVE_FUSE_DEBUGLEVEL
	psc_ctlparam_register_simple("fuse.debug",
	    pscfs_ctlparam_fuse_debug_get,
	    pscfs_ctlparam_fuse_debug_set);
#endif
	psc_ctlparam_register_simple("fuse.version",
	    pscfs_ctlparam_fuse_version_get, NULL);
	psc_ctlparam_register_simple("pscfs.entry_timeout",
	    pscfs_ctlparam_entry_timeout_get,
	    pscfs_ctlparam_entry_timeout_set);
	psc_ctlparam_register_simple("pscfs.attr_timeout",
	    pscfs_ctlparam_attr_timeout_get,
	    pscfs_ctlparam_attr_timeout_set);
#endif

	thrv = PSCALLOC(sizeof(*thrv) * nthr);

	pflfs_modules_wrpin();

	for (i = 0; i < nthr; i++) {
		thr = pscthr_init(PFL_THRT_FS, pscfs_fuse_listener_loop,
		    sizeof(*pft), "%sfsthr%02d", thrname, i);
		thrv[i] = thr->pscthr_pthread;
		pscthr_setready(thr);
	}

	pflfs_modules_wrunpin();

	DYNARRAY_FOREACH(m, i, &pscfs_modules)
		if (m->pf_thr_init)
			_pflfs_module_init_threads(m);

	/* XXX run destructors */

	pfl_atexit(pfl_fuse_atexit);

	thr = pscthr_get();
	for (i = 0; i < nthr; i++) {

		thr->pscthr_waitq = "join";
		int ret = pthread_join(thrv[i], NULL);
		thr->pscthr_waitq = NULL;

		if (ret != 0)
			fprintf(stderr, "Warning: pthread_join() on "
			    "thread %i returned %i\n", i, ret);
	}

#ifdef DEBUG
	fprintf(stderr, "Exiting...\n");
#endif

	for (i = 1; i < pflfs_nfds; i++) {
		if (pflfs_fds[i].fd == -1)
			continue;

		fuse_session_exit(pflfs_fsinfo[i].se);
		fuse_session_reset(pflfs_fsinfo[i].se);
		fuse_unmount(mountpoints[i], pflfs_fsinfo[i].ch);
		fuse_session_destroy(pflfs_fsinfo[i].se);

		PSCFREE(mountpoints[i]);
	}
	close(newfs_fd[0]);
	close(newfs_fd[1]);
	return (0);
}

void
pscfs_addarg(struct pscfs_args *pfa, const char *arg)
{
	if (fuse_opt_add_arg(&pfa->pfa_av, arg) == -1)
		psc_fatal("fuse_opt_add_arg");
}

void
pscfs_freeargs(struct pscfs_args *pfa)
{
	fuse_opt_free_args(&pfa->pfa_av);
}

struct pscfs_clientctx *
pscfs_getclientctx(struct pscfs_req *pfr)
{
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_ufsi_req);
	struct pscfs_clientctx *pfcc = &pfr->pfr_clientctx;

	if (pfcc->pfcc_pid == 0)
		pfcc->pfcc_pid = ctx->pid;
	return (pfcc);
}

void
pscfs_getcreds(struct pscfs_req *pfr, struct pscfs_creds *pcr)
{
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_ufsi_req);

	pcr->pcr_uid = ctx->uid;
	pcr->pcr_gid = ctx->gid;
	if (pscfs_getgroups(pfr, pcr->pcr_gidv, &pcr->pcr_ngid)) {
		pcr->pcr_gidv[0] = ctx->gid;
		pcr->pcr_ngid = 1;
	}
}

mode_t
pscfs_getumask(struct pscfs_req *pfr)
{
#if FUSE_VERSION > FUSE_MAKE_VERSION(2,7)
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_ufsi_req);

	return (ctx->umask);
#endif
	(void)pfr;
	/* XXX read from /proc ? */
//	psclog_warn("fabricating umask")
	return (0644);
}

#if 0
int
pscfs_inum_reclaim(struct psc_poolmgr *m, int n)
{
	time_t now;
	int nrel = 0;

	PFL_GETTIMEVAL(&now);
	/* XXX start at random position */
	PSC_HASHTBL_FOREACH_BUCKET(t, b) {
		psc_hashbkt_lock(b);
		PSC_HASHBKT_FOREACH_ENTRY(t, b, pfic) {
			if (pfic->pfic_extime &&
			    timercmp(&now, &pfic->pfic_extime, >) &&
			    refcnt == 0)
				evict
		}
		psc_hashbkt_unlock(b);
	}
}
#endif

#ifndef __LP64__
/* convert a 32-bit system inum to a 64-bit pscfs inum */
pscfs_inum_t
pscfs_inum_fuse2pscfs(fuse_ino_t f_inum, int del)
{
	struct pscfs_inumcol *pfic;
	struct psc_hashbkt *b;
	pscfs_inum_t p_inum;
	uint64_t key;

	key = f_inum;
	b = psc_hashbkt_get(&pflfs_inumcol_hashtbl, &key);
	psc_hashbkt_lock(b);
	pfic = psc_hashtbl_search(&pflfs_inumcol_hashtbl, NULL,
	    NULL, &key);
	p_inum = pfic->pfic_pscfs_inum;
	if (del) {
		psc_atomic32_dec(&pfic->pfic_refcnt);
		if (psc_atomic32_read(&pfic->pfic_refcnt) == 0)
			psc_hashbkt_del_item(&pflfs_inumcol_hashtbl, b,
			    pfic);
		else
			pfic = NULL;
	}
	psc_hashbkt_put(&pflfs_inumcol_hashtbl, b);

	if (del && pfic)
		psc_pool_return(pflfs_inumcol_pool, pfic);

	return (p_inum);
}

#define COMPUTE_EXPIRE(tv, timeo)					\
	do {								\
		struct timeval _tv;					\
									\
		PFL_GETTIMEVAL(tv);					\
		_tv.tv_sec = (int)(timeo);				\
		_tv.tv_usec = 1000 * ((timeo) - (int)(timeo));		\
		timeradd((tv), &_tv, (tv));				\
	} while (0)

/* convert a 64-bit pscfs inum to a 32-bit system inum */
fuse_ino_t
pscfs_inum_pscfs2fuse(pscfs_inum_t p_inum, double timeo)
{
	struct pscfs_inumcol *pfic, *t;
	struct psc_hashbkt *b;
	uint64_t key;

	pfic = psc_pool_get(pflfs_inumcol_pool);

	key = (fuse_ino_t)p_inum;
	do {
		b = psc_hashbkt_get(&pflfs_inumcol_hashtbl, &key);
		psc_hashbkt_lock(b);
		t = psc_hashtbl_search(&pflfs_inumcol_hashtbl, NULL,
		    NULL, &key);
		if (t) {
			/*
			 * This faux inum is already in table.  If this
			 * is for the same real inum, reuse this faux
			 * inum; otherwise, fallback to a unique random
			 * value.
			 */
			if (t->pfic_pscfs_inum == p_inum) {
				psc_atomic32_inc(&t->pfic_refcnt);
				COMPUTE_EXPIRE(&t->pfic_extime, timeo);
				key = t->pfic_key;
				t = NULL;
			} else
				key = psc_random32();
		} else {
			memset(pfic, 0, sizeof(*pfic));
			psc_hashent_init(&pflfs_inumcol_hashtbl, pfic);
			pfic->pfic_pscfs_inum = p_inum;
			pfic->pfic_key = key;
			COMPUTE_EXPIRE(&pfic->pfic_extime, timeo);
			psc_atomic32_set(&pfic->pfic_refcnt, 1);
			psc_hashbkt_add_item(&pflfs_inumcol_hashtbl, b,
			    pfic);
			pfic = NULL;
		}
		psc_hashbkt_put(&pflfs_inumcol_hashtbl, b);
	} while (t);
	if (pfic)
		psc_pool_return(pflfs_inumcol_pool, pfic);
	return (key);
}
#endif

#define GETPFR(pfr, fsreq)						\
	do {								\
		static struct pfl_opstat *_opst;			\
		struct psc_thread *_thr;				\
		struct pfl_fsthr *_pft;					\
									\
		if (_opst == NULL)					\
			_opst = pfl_opstat_initf(OPSTF_BASE10,		\
			    "fs.handle.%s", __func__ +			\
			    strlen("pscfs_fuse_handle_"));		\
		pfl_opstat_incr(_opst);					\
									\
		(pfr) = psc_pool_get(pflfs_req_pool);			\
		memset((pfr), 0, sizeof(*(pfr)));			\
		PFL_GETTIMESPEC(&(pfr)->pfr_start);			\
		INIT_LISTENTRY(&(pfr)->pfr_lentry);			\
		INIT_SPINLOCK(&(pfr)->pfr_lock);			\
		(pfr)->pfr_ufsi_req = (fsreq);				\
		(pfr)->pfr_refcnt = 2;					\
		(pfr)->pfr_opname = __func__ +				\
		    strlen("pscfs_fuse_handle_");			\
		PFLOG_PFR(PLL_DEBUG, (pfr), "create");			\
									\
		_thr = pscthr_get();					\
		_pft = _thr->pscthr_private;				\
		_pft->pft_pfr = (pfr);					\
		(pfr)->pfr_thread = _thr;				\
									\
		if (fsreq)						\
			fuse_req_interrupt_func((fsreq),		\
			    pscfs_fuse_interrupt, (pfr));		\
									\
		pll_add(&pflfs_requests, (pfr));			\
	} while (0)

#define PFLOG_PFR(level, pfr, fmt, ...)					\
	psclog((level), "pfr@%p ref=%d " fmt, (pfr), (pfr)->pfr_refcnt,	\
	    ##__VA_ARGS__)

/*
 * An error from a module for an file system operation short circuits
 * the processing.  Success from any single module simply means
 * continuing to the next module.  The exception is the 'default' pscfs
 * module.
 */
#define FSOP(op, pfr, ...)						\
	do {								\
		int _mi, _prior_success = 0;				\
		struct pscfs *_m;					\
									\
		pflfs_modules_rdpin();					\
		DYNARRAY_FOREACH(_m, _mi, &pscfs_modules) {		\
			if (_m->pf_handle_ ##op == NULL)		\
				continue;				\
									\
			/*						\
			 * The last module is pscfs' builtin ENOTSUP	\
			 * failure layer.  If any previous module was	\
			 * successful, do not invoke this last module.	\
			 */						\
			if (_prior_success && _mi ==			\
			    psc_dynarray_len(&pscfs_modules) - 1) {	\
				(pfr)->pfr_refcnt--;			\
				PFLOG_PFR(PLL_DEBUG, (pfr), "decref");	\
				break;					\
			}						\
									\
			(pfr)->pfr_mod = _m;				\
			_m->pf_handle_ ##op ((pfr), ##__VA_ARGS__);	\
									\
			/* The module deferred reply. */		\
			if ((pfr)->pfr_refcnt > 1)			\
				break;					\
									\
			/* The module returned an error. */		\
			if ((pfr)->pfr_rc)				\
				break;					\
									\
			_prior_success = 1;				\
			(pfr)->pfr_refcnt++;				\
			PFLOG_PFR(PLL_DEBUG, pfr, "incref");		\
		}							\
		pfr_decref((pfr), 0);					\
	} while (0)

#define pfr_decref(pfr, rc)	_pfr_decref(PFL_CALLERINFO(), (pfr), (rc))

void
_pfr_decref(const struct pfl_callerinfo *pci, struct pscfs_req *pfr, int rc)
{
	spinlock_pci(pci, &pfr->pfr_lock);
	if (pfr->pfr_rc == 0 && rc)
		pfr->pfr_rc = rc;
	psc_assert(pfr->pfr_refcnt > 0);
	PFLOG_PFR(PLL_DEBUG, pfr, "decref");
	if (--pfr->pfr_refcnt) {
		freelock(&pfr->pfr_lock);
		return;
	}
	pll_remove(&pflfs_requests, pfr);
	PFLOG_PFR(PLL_DEBUG, pfr, "destroying");
	psc_pool_return(pflfs_req_pool, pfr);

	pflfs_modules_rdunpin();
}

void
pscfs_fuse_handle_access(fuse_req_t req, fuse_ino_t inum, int mask)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(access, pfr, INUM_FUSE2PSCFS(inum), mask);
}

void
pscfs_fuse_handle_release(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(release, pfr, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_releasedir(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(releasedir, pfr, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_create(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(create, pfr, INUM_FUSE2PSCFS(pinum), name, fi->flags,
	    mode);
}

void
pscfs_fuse_handle_flush(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(flush, pfr, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_fsync(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(fsync, pfr, datasync, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_fsyncdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(fsyncdir, pfr, datasync, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_getattr(fuse_req_t req, fuse_ino_t inum,
    __unusedx struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(getattr, pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_link(fuse_req_t req, fuse_ino_t c_inum,
    fuse_ino_t p_inum, const char *newname)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(link, pfr, INUM_FUSE2PSCFS(c_inum),
	    INUM_FUSE2PSCFS(p_inum), newname);
}

void
pscfs_fuse_handle_lookup(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(lookup, pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_mkdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(mkdir, pfr, INUM_FUSE2PSCFS(pinum), name, mode);
}

void
pscfs_fuse_handle_mknod(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, dev_t rdev)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(mknod, pfr, INUM_FUSE2PSCFS(pinum), name, mode, rdev);
}

void
pscfs_fuse_handle_open(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(open, pfr, INUM_FUSE2PSCFS(inum), fi->flags);
}

void
pscfs_fuse_handle_opendir(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(opendir, pfr, INUM_FUSE2PSCFS(inum), fi->flags);
}

void
pscfs_fuse_handle_read(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(read, pfr, size, off, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_readdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(readdir, pfr, size, off, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_readlink(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(readlink, pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_rename(fuse_req_t req, fuse_ino_t oldpinum,
    const char *oldname, fuse_ino_t newpinum, const char *newname)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(rename, pfr, INUM_FUSE2PSCFS(oldpinum), oldname,
	    INUM_FUSE2PSCFS(newpinum), newname);
}

void
pscfs_fuse_handle_rmdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(rmdir, pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_setattr(fuse_req_t req, fuse_ino_t inum,
    struct stat *stb, int fuse_to_set,
    __unusedx struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;
	int pfl_to_set = 0;

	if (fuse_to_set & FUSE_SET_ATTR_MODE)
		pfl_to_set |= PSCFS_SETATTRF_MODE;
	if (fuse_to_set & FUSE_SET_ATTR_UID)
		pfl_to_set |= PSCFS_SETATTRF_UID;
	if (fuse_to_set & FUSE_SET_ATTR_GID)
		pfl_to_set |= PSCFS_SETATTRF_GID;
	if (fuse_to_set & FUSE_SET_ATTR_SIZE)
		pfl_to_set |= PSCFS_SETATTRF_DATASIZE;
	if (fuse_to_set & FUSE_SET_ATTR_ATIME)
		pfl_to_set |= PSCFS_SETATTRF_ATIME;
	if (fuse_to_set & FUSE_SET_ATTR_MTIME)
		pfl_to_set |= PSCFS_SETATTRF_MTIME;
#ifdef FUSE_SET_ATTR_ATIME_NOW
	if (fuse_to_set & FUSE_SET_ATTR_ATIME_NOW)
		pfl_to_set |= PSCFS_SETATTRF_ATIME_NOW;
	if (fuse_to_set & FUSE_SET_ATTR_MTIME_NOW)
		pfl_to_set |= PSCFS_SETATTRF_MTIME_NOW;
#endif

	GETPFR(pfr, req);
	stb->st_ino = INUM_FUSE2PSCFS(stb->st_ino);
	FSOP(setattr, pfr, INUM_FUSE2PSCFS(inum), stb, pfl_to_set, fi ?
	    fusefi_to_pri(fi) : NULL);
}

void
pscfs_fuse_handle_statfs(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(statfs, pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_symlink(fuse_req_t req, const char *buf,
    fuse_ino_t pinum, const char *name)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(symlink, pfr, buf, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_destroy(__unusedx void *userdata)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, NULL);
	FSOP(destroy, pfr);
}

void
pscfs_fuse_handle_unlink(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(unlink, pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_write(fuse_req_t req, __unusedx fuse_ino_t ino,
    const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	pfr->pfr_ufsi_fhdata = fi;
	FSOP(write, pfr, buf, size, off, fusefi_to_pri(fi));
}

void
pscfs_fuse_handle_listxattr(fuse_req_t req, fuse_ino_t ino,
    size_t size)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(listxattr, pfr, size, INUM_FUSE2PSCFS(ino));
}

#ifdef __APPLE__
void
pscfs_fuse_handle_setxattr(fuse_req_t req, fuse_ino_t ino,
    const char *name, const char *value, size_t size,
    __unusedx int flags, __unusedx uint32_t position)
#else
void
pscfs_fuse_handle_setxattr(fuse_req_t req, fuse_ino_t ino,
    const char *name, const char *value, size_t size,
    __unusedx int flags)
#endif
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(setxattr, pfr, name, value, size, INUM_FUSE2PSCFS(ino));
}

#ifdef __APPLE__
void
pscfs_fuse_handle_getxattr(fuse_req_t req, fuse_ino_t ino,
    const char *name, size_t size, __unusedx uint32_t position)
#else
void
pscfs_fuse_handle_getxattr(fuse_req_t req, fuse_ino_t ino,
    const char *name, size_t size)
#endif
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(getxattr, pfr, name, size, INUM_FUSE2PSCFS(ino));
}

void
pscfs_fuse_handle_removexattr(fuse_req_t req, fuse_ino_t ino,
    const char *name)
{
	struct pscfs_req *pfr;

	GETPFR(pfr, req);
	FSOP(removexattr, pfr, name, INUM_FUSE2PSCFS(ino));
}

/* Begin file system call reply routines */

#define PFR_REPLY(func, pfr, ...)					\
	do {								\
		struct timespec t0, d;					\
		struct {						\
			void *p;					\
			uint64_t uniqid;				\
		} *r0p = (void *)(pfr)->pfr_ufsi_req;			\
		uint64_t u0 = r0p->uniqid;				\
									\
		fuse_reply_##func((pfr)->pfr_ufsi_req, ## __VA_ARGS__);	\
		PFL_GETTIMESPEC(&t0);					\
		timespecsub(&t0, &(pfr)->pfr_start, &d);		\
		t0.tv_sec = 0;						\
		t0.tv_nsec = 600000;					\
		if (timespeccmp(&d, &t0, >))				\
			psclog_diag(					\
			    "in for "PSCPRI_TIMESPEC"s uniqid=%"PRIu64,	\
			    PSCPRI_TIMESPEC_ARGS(&d), u0);		\
		(void)u0;						\
		pfr_decref((pfr), rc);					\
	} while (0)

void
pscfs_reply_access(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_release(struct pscfs_req *pfr, int rc)
{
	struct pflfs_filehandle *pfh;

	pfh = pfr_to_pfh(pfr);
	PFR_REPLY(err, pfr, rc);
	pll_remove(&pflfs_filehandles, pfh);
	psc_pool_return(pflfs_filehandle_pool, pfh);
}

void
pscfs_reply_releasedir(struct pscfs_req *pfr, int rc)
{
	struct pflfs_filehandle *pfh;

	pfh = pfr_to_pfh(pfr);
	PFR_REPLY(err, pfr, rc);
	pll_remove(&pflfs_filehandles, pfh);
	psc_pool_return(pflfs_filehandle_pool, pfh);
}

void
pscfs_reply_create(struct pscfs_req *pfr, pscfs_inum_t inum,
    pscfs_fgen_t gen, double entry_timeout, const struct stat *stb,
    double attr_timeout, void *data, int rflags, int rc)
{
	struct fuse_entry_param e;

	if (rc)
		PFR_REPLY(err, pfr, rc);
	else {
		struct fuse_file_info *fi;

		fi = pfr_to_fusefi(pfr);
		if (rflags & PSCFS_CREATEF_DIO)
			fi->direct_io = 1;
		fusefi_stash_pri(fi, data);
		e.entry_timeout = entry_timeout;
		e.ino = INUM_PSCFS2FUSE(inum, entry_timeout);
		if (e.ino) {
			e.attr_timeout = attr_timeout;
			memcpy(&e.attr, stb, sizeof(e.attr));
			e.attr.st_ino = e.ino;
			e.generation = gen;
		}
		PFR_REPLY(create, pfr, &e, fi);
	}
}

void
pscfs_reply_flush(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_fsync(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_fsyncdir(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_getattr(struct pscfs_req *pfr, struct stat *stb,
    double attr_timeout, int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else {
		stb->st_ino = INUM_PSCFS2FUSE(stb->st_ino,
		    attr_timeout);
		PFR_REPLY(attr, pfr, stb, attr_timeout);
	}
}

void
pscfs_reply_ioctl(struct pscfs_req *pfr)
{
	//PFR_REPLY(err, pfr, rc);
	pfr_decref(pfr, 0);
}

void
pscfs_reply_open(struct pscfs_req *pfr, void *data, int rflags, int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else {
		struct fuse_file_info *fi;

		fi = pfr_to_fusefi(pfr);
		if (rflags & PSCFS_OPENF_KEEPCACHE)
			fi->keep_cache = 1;
		if (rflags & PSCFS_OPENF_DIO)
			fi->direct_io = 1;

#ifdef HAVE_NO_FUSE_PRIVATE_MMAP
		/*
		 * FUSE direct_io does not work with mmap(MAP_SHARED),
		 * which is what the kernel uses under the hood when
		 * running executables, so disable it for this case.
		 *
		 * XXX provide some kind of knob to allow direct_io to
		 * be turned off, exposed to userland (per-process).
		 */
		if (c->fcmh_sstb.sst_mode & _S_IXUGO)
			fi->direct_io = 0;
#endif

		fusefi_stash_pri(fi, data);
		PFR_REPLY(open, pfr, fi);
	}
}

void *
pflfs_req_getfh(struct pscfs_req *pfr)
{
	struct fuse_file_info *fi;

	fi = pfr_to_fusefi(pfr);
	return (fusefi_to_pri(fi));
}

void
pscfs_reply_opendir(struct pscfs_req *pfr, void *data, int rflags, int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else {
		struct fuse_file_info *fi;

		fi = pfr_to_fusefi(pfr);
		if (rflags & PSCFS_OPENF_KEEPCACHE)
			fi->keep_cache = 1;
		if (rflags & PSCFS_OPENF_DIO)
			fi->direct_io = 1;
		fusefi_stash_pri(fi, data);
		PFR_REPLY(open, pfr, fi);
	}
}

void
pscfs_reply_read(struct pscfs_req *pfr, struct iovec *iov, int nio,
    int rc)
{
	if (rc) {
		pfl_opstat_incr(pfr->pfr_mod->pf_opst_read_err);
		PFR_REPLY(err, pfr, rc);
	} else {
		size_t size = 0;
		int i;

		for (i = 0; i < nio; i++)
			size += iov[i].iov_len;
		pfl_opstat_add(pfr->pfr_mod->pf_opst_read_reply, size);

		PFR_REPLY(iov, pfr, iov, nio);
	}
}

void
pscfs_reply_readdir(struct pscfs_req *pfr, void *buf, ssize_t len,
    int rc)
{
	struct pscfs_dirent *dirent;
	off_t off;

	if (rc)
		PFR_REPLY(err, pfr, rc);
	else {
		for (dirent = buf, off = 0; off < len;
		    off += PFL_DIRENT_SIZE(dirent->pfd_namelen),
		    dirent = PSC_AGP(buf, off))
			dirent->pfd_ino = INUM_PSCFS2FUSE(
			    dirent->pfd_ino, 8);
		PFR_REPLY(buf, pfr, buf, len);
	}
}

void
pscfs_reply_readlink(struct pscfs_req *pfr, void *buf, int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else
		PFR_REPLY(readlink, pfr, buf);
}

void
pscfs_reply_rename(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_rmdir(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_setattr(struct pscfs_req *pfr, struct stat *stb,
    double attr_timeout, int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else {
		stb->st_ino = INUM_PSCFS2FUSE(stb->st_ino, attr_timeout);
		PFR_REPLY(attr, pfr, stb, attr_timeout);
	}
}

void
pscfs_reply_statfs(struct pscfs_req *pfr, const struct statvfs *sfb,
    int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else
		PFR_REPLY(statfs, pfr, sfb);
}

void
pscfs_reply_destroy(struct pscfs_req *pfr)
{
	pfr_decref(pfr, 0);
}

void
pscfs_reply_unlink(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_write(struct pscfs_req *pfr, ssize_t len, int rc)
{
	if (rc) {
		pfl_opstat_incr(pfr->pfr_mod->pf_opst_write_err);
		PFR_REPLY(err, pfr, rc);
	} else {
		pfl_opstat_add(pfr->pfr_mod->pf_opst_write_reply, len);
		PFR_REPLY(write, pfr, len);
	}
}

void
pscfs_reply_listxattr(struct pscfs_req *pfr, void *buf, size_t len, int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else if (buf)
		PFR_REPLY(buf, pfr, buf, len);
	else
		PFR_REPLY(xattr, pfr, len);
}

void
pscfs_reply_setxattr(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_reply_getxattr(struct pscfs_req *pfr, void *buf, size_t len,
    int rc)
{
	if (rc)
		PFR_REPLY(err, pfr, rc);
	else if (buf)
		PFR_REPLY(buf, pfr, buf, len);
	else
		PFR_REPLY(xattr, pfr, len);
}

void
pscfs_reply_removexattr(struct pscfs_req *pfr, int rc)
{
	PFR_REPLY(err, pfr, rc);
}

void
pscfs_fuse_replygen_entry(struct pscfs_req *pfr, pscfs_inum_t inum,
    pscfs_fgen_t gen, double entry_timeout, const struct stat *stb,
    double attr_timeout, int rc)
{
	struct fuse_entry_param e;

	memset(&e, 0, sizeof(e));

	if (rc == ENOENT) {
		e.entry_timeout = entry_timeout;
		PFR_REPLY(entry, pfr, &e);
	} else if (rc) {
		PFR_REPLY(err, pfr, rc);
	} else {
		e.entry_timeout = entry_timeout;
		e.ino = INUM_PSCFS2FUSE(inum, entry_timeout);
		if (e.ino) {
			e.attr_timeout = attr_timeout;
			memcpy(&e.attr, stb, sizeof(e.attr));
			e.attr.st_ino = e.ino;
			e.generation = gen;
		}
		PFR_REPLY(entry, pfr, &e);
	}
}

struct fuse_lowlevel_ops pscfs_fuse_ops = {
	.access		= pscfs_fuse_handle_access,
	.create		= pscfs_fuse_handle_create,
	.destroy	= pscfs_fuse_handle_destroy,
	.flush		= pscfs_fuse_handle_flush,
	.fsync		= pscfs_fuse_handle_fsync,
	.fsyncdir	= pscfs_fuse_handle_fsyncdir,
	.getattr	= pscfs_fuse_handle_getattr,
	.link		= pscfs_fuse_handle_link,
	.lookup		= pscfs_fuse_handle_lookup,
	.mkdir		= pscfs_fuse_handle_mkdir,
	.mknod		= pscfs_fuse_handle_mknod,
	.open		= pscfs_fuse_handle_open,
	.opendir	= pscfs_fuse_handle_opendir,
	.read		= pscfs_fuse_handle_read,
	.readdir	= pscfs_fuse_handle_readdir,
	.readlink	= pscfs_fuse_handle_readlink,
	.release	= pscfs_fuse_handle_release,
	.releasedir	= pscfs_fuse_handle_releasedir,
	.rename		= pscfs_fuse_handle_rename,
	.rmdir		= pscfs_fuse_handle_rmdir,
	.setattr	= pscfs_fuse_handle_setattr,
	.statfs		= pscfs_fuse_handle_statfs,
	.symlink	= pscfs_fuse_handle_symlink,
	.unlink		= pscfs_fuse_handle_unlink,
	.write		= pscfs_fuse_handle_write,
	.listxattr	= pscfs_fuse_handle_listxattr,
	.setxattr	= pscfs_fuse_handle_setxattr,
	.getxattr	= pscfs_fuse_handle_getxattr,
	.removexattr	= pscfs_fuse_handle_removexattr
};

void
pscfs_mount(const char *mp, struct pscfs_args *pfa)
{
	struct fuse_chan *ch;
	char nameopt[BUFSIZ];
	int rc;

	if (pipe(newfs_fd) == -1)
		psc_fatal("pipe");

	pscfs_fuse_addfd(newfs_fd[0]);

	rc = snprintf(nameopt, sizeof(nameopt), "fsname=%s", mp);
	if (rc == -1)
		psc_fatal("snprintf: fsname=%s", mp);
	if (rc >= (int)sizeof(nameopt))
		psc_fatalx("snprintf: fsname=%s: too long", mp);

	pscfs_addarg(pfa, "-o");
	pscfs_addarg(pfa, nameopt);

	ch = fuse_mount(mp, &pfa->pfa_av);
	if (ch == NULL)
		/* this has triggered: inappropriate ioctl for device */
		psc_fatal("Is the mount point (%s) empty?", mp);

	fuse_session = fuse_lowlevel_new(&pfa->pfa_av, &pscfs_fuse_ops,
	    sizeof(pscfs_fuse_ops), NULL);

	if (fuse_session == NULL) {
		fuse_unmount(mp, ch);
		psc_fatal("fuse_lowlevel_new");
	}

	fuse_session_add_chan(fuse_session, ch);

	if (pscfs_fuse_newfs(mp, ch) != 0) {
		fuse_session_destroy(fuse_session);
		fuse_unmount(mp, ch);
		psc_fatal("fuse_session_add_chan");
	}

	psclog_info("FUSE version %d.%d", FUSE_MAJOR_VERSION,
	    FUSE_MINOR_VERSION);
}

#ifndef HAVE_FUSE_REQ_GETGROUPS
int
fuse_req_getgroups(__unusedx fuse_req_t req, __unusedx int ng,
    __unusedx gid_t *gv)
{
	return (-ENOSYS);
}
#endif

int
pscfs_getgroups(struct pscfs_req *pfr, gid_t *gv, int *ng)
{
	int rc = 0;

	*ng = fuse_req_getgroups(pfr->pfr_ufsi_req, NGROUPS_MAX, gv);
	if (*ng > NGROUPS_MAX) {
		psclog_error("fuse_req_getgroups returned "
		    "%d > NGROUPS_MAX (%d)", *ng, NGROUPS_MAX);
		*ng = NGROUPS_MAX;
	}
	if (*ng >= 0)
		return (0);
	if (*ng == -ENOSYS) {
		/* not supported; revert to getgrent(3) */
		*ng = NGROUPS_MAX;
		rc = pflsys_getusergroups(
		    fuse_req_ctx(pfr->pfr_ufsi_req)->uid,
		    fuse_req_ctx(pfr->pfr_ufsi_req)->gid, gv, ng);
	} else
		rc = abs(*ng);
	return (rc);
}

void *
pflfs_inval_getprivate(struct pscfs_req *pfr)
{
	struct fuse_chan *ch;

#ifdef HAVE_FUSE_REQ_GETCHANNEL
	ch = fuse_req_getchannel(pfr->pfr_ufsi_req);
#else
	(void)pfr;
	ch = NULL;
#endif
	return (ch);
}

int
pflfs_inval_inode(void *pri, pscfs_inum_t inum)
{
	int rc = -ENOTSUP;

#ifdef HAVE_FUSE_NOTIFY_INVAL
	rc = fuse_lowlevel_notify_inval_entry(pri, INUM_PSCFS2FUSE(inum,
	    0.0), 0, 0);
#else
	(void)pri;
	(void)inum;
#endif
	return (rc);
}

int
pscfs_notify_inval_entry(void *pri, pscfs_inum_t pinum,
    const char *name, size_t namelen)
{
	int rc;

#ifdef HAVE_FUSE_NOTIFY_INVAL
	rc = fuse_lowlevel_notify_inval_entry(pri,
	    INUM_PSCFS2FUSE(pinum, 0.0), name, namelen);
#else
	(void)pri;
	(void)pinum;
	(void)name;
	(void)namelen;
	rc = -ENOTSUP;
#endif
	return (rc);
}

void
pscfsop_access(struct pscfs_req *pfr, pscfs_inum_t inum,
    int accmode)
{
	(void)inum;
	(void)accmode;
	pscfs_reply_access(pfr, ENOTSUP);
}

void
pscfsop_release(struct pscfs_req *pfr, void *data)
{
	(void)data;
	pscfs_reply_release(pfr, 0);
}

void
pscfsop_releasedir(struct pscfs_req *pfr, void *data)
{
	(void)data;
	pscfs_reply_release(pfr, 0);
}

void
pscfsop_create(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, int oflags, mode_t mode)
{
	(void)pinum;
	(void)name;
	(void)oflags;
	(void)mode;
	pscfs_reply_create(pfr, 0, 0, 0, NULL, 0, NULL, 0, ENOTSUP);
}

void
pscfsop_flush(struct pscfs_req *pfr, void *data)
{
	(void)data;
	pscfs_reply_flush(pfr, 0);
}

void
pscfsop_fsync(struct pscfs_req *pfr, int datasync_only, void *data)
{
	(void)datasync_only;
	(void)data;
	pscfs_reply_fsync(pfr, 0);
}

void
pscfsop_fsyncdir(struct pscfs_req *pfr, int datasync_only, void *data)
{
	(void)datasync_only;
	(void)data;
	pscfs_reply_fsyncdir(pfr, 0);
}

void
pscfsop_getattr(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	(void)inum;
	pscfs_reply_getattr(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_getxattr(struct pscfs_req *pfr, const char *name, size_t size,
    pscfs_inum_t inum)
{
	(void)name;
	(void)size;
	(void)inum;
	pscfs_reply_getattr(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_link(struct pscfs_req *pfr, pscfs_inum_t c_inum,
    pscfs_inum_t p_inum, const char *newname)
{
	(void)c_inum;
	(void)p_inum;
	(void)newname;
	pscfs_reply_link(pfr, 0, 0, 0, NULL, 0, ENOTSUP);
}

void
pscfsop_lookup(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	(void)pinum;
	(void)name;
	pscfs_reply_lookup(pfr, 0, 0, 0, NULL, 0, ENOTSUP);
}

void
pscfsop_mkdir(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, mode_t mode)
{
	(void)pinum;
	(void)name;
	(void)mode;
	pscfs_reply_mkdir(pfr, 0, 0, 0, NULL, 0, ENOTSUP);
}

void
pscfsop_mknod(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, mode_t mode, dev_t rdev)
{
	(void)pinum;
	(void)name;
	(void)mode;
	(void)rdev;
	pscfs_reply_mknod(pfr, 0, 0, 0, NULL, 0, ENOTSUP);
}

void
pscfsop_open(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags)
{
	(void)inum;
	(void)oflags;
	pscfs_reply_open(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_opendir(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags)
{
	(void)inum;
	(void)oflags;
	pscfs_reply_opendir(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_read(struct pscfs_req *pfr, size_t size, off_t off, void *data)
{
	(void)off;
	(void)size;
	(void)data;
	pscfs_reply_read(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_readdir(struct pscfs_req *pfr, size_t size, off_t off,
    void *data)
{
	(void)size;
	(void)off;
	(void)data;
	pscfs_reply_readdir(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_readlink(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	(void)inum;
	pscfs_reply_readlink(pfr, NULL, ENOTSUP);
}

void
pscfsop_rename(struct pscfs_req *pfr, pscfs_inum_t opinum,
    const char *oldname, pscfs_inum_t npinum, const char *newname)
{
	(void)opinum;
	(void)npinum;
	(void)oldname;
	(void)newname;
	pscfs_reply_rename(pfr, ENOTSUP);
}

void
pscfsop_rmdir(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	(void)pinum;
	(void)name;
	pscfs_reply_rmdir(pfr, ENOTSUP);
}

void
pscfsop_setattr(struct pscfs_req *pfr, pscfs_inum_t inum,
    struct stat *stb, int to_set, void *data)
{
	(void)inum;
	(void)stb;
	(void)to_set;
	(void)data;
	pscfs_reply_setattr(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_statfs(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	(void)inum;
	pscfs_reply_statfs(pfr, NULL, ENOTSUP);
}

void
pscfsop_symlink(struct pscfs_req *pfr, const char *buf,
    pscfs_inum_t pinum, const char *name)
{
	(void)buf;
	(void)pinum;
	(void)name;
	pscfs_reply_symlink(pfr, 0, 0, 0, NULL, 0, ENOTSUP);
}

void
pscfsop_unlink(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	(void)pinum;
	(void)name;
	pscfs_reply_unlink(pfr, ENOTSUP);
}

void
pscfsop_write(struct pscfs_req *pfr, const void *buf, size_t size,
    off_t off, void *data)
{
	(void)buf;
	(void)size;
	(void)off;
	(void)data;
	pscfs_reply_write(pfr, 0, ENOTSUP);
}

void
pscfsop_listxattr(struct pscfs_req *pfr, size_t size, pscfs_inum_t inum)
{
	(void)size;
	(void)inum;
	pscfs_reply_listxattr(pfr, NULL, 0, ENOTSUP);
}

void
pscfsop_destroy(struct pscfs_req *pfr)
{
	(void)pfr;
	pscthr_killall();
	pfl_wkthr_killall();
}

void
pscfsop_setxattr(struct pscfs_req *pfr, const char *name,
    const void *value, size_t size, pscfs_inum_t inum)
{
	(void)name;
	(void)value;
	(void)size;
	(void)inum;
	pscfs_reply_setxattr(pfr, ENOTSUP);
}

void
pscfsop_removexattr(struct pscfs_req *pfr, const char *name,
    pscfs_inum_t inum)
{
	(void)name;
	(void)inum;
	pscfs_reply_removexattr(pfr, ENOTSUP);
}

struct pscfs pscfs_default_ops = {
	PSCFS_INIT,
	"default",
	pscfsop_access,
	pscfsop_release,
	pscfsop_releasedir,
	pscfsop_create,
	pscfsop_flush,
	pscfsop_fsync,
	pscfsop_fsyncdir,
	pscfsop_getattr,
	NULL,			/* ioctl */
	pscfsop_link,
	pscfsop_lookup,
	pscfsop_mkdir,
	pscfsop_mknod,
	pscfsop_open,
	pscfsop_opendir,
	pscfsop_read,
	pscfsop_readdir,
	pscfsop_readlink,
	pscfsop_rename,
	pscfsop_rmdir,
	pscfsop_setattr,
	pscfsop_statfs,
	pscfsop_symlink,
	pscfsop_unlink,
	pscfsop_destroy,
	pscfsop_write,
	pscfsop_listxattr,
	pscfsop_getxattr,
	pscfsop_setxattr,
	pscfsop_removexattr
};
