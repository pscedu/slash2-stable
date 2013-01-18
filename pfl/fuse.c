/* $Id$ */
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Ricardo Correia.
 * Use is subject to license terms.
 */

#include <sys/param.h>
#include <sys/poll.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

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

#include "pfl/fs.h"
#include "pfl/fsmod.h"
#include "pfl/sys.h"
#include "psc_util/alloc.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlsvr.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/pool.h"
#include "psc_util/random.h"
#include "psc_util/waitq.h"

#ifdef __LP64__
#  include "pfl/hashtbl.h"
#endif

#define NUM_THREADS			32
#define MAX_FILESYSTEMS			5
#define MAX_FDS				(MAX_FILESYSTEMS + 1)

#define fi_getdata(fi)			((void *)(unsigned long)(fi)->fh)
#define fi_setdata(fi, data)		((fi)->fh = (unsigned long)(data))

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
	struct psc_hashent	 pfic_hentry;
};

static struct psc_poolmaster	 pscfs_inumcol_poolmaster;
static struct psc_poolmgr	*pscfs_inumcol_pool;
static struct psc_hashtbl	 pscfs_inumcol_hashtbl;
#endif

typedef struct {
	int			 fd;
	size_t			 bufsize;
	struct fuse_chan	*ch;
	struct fuse_session	*se;
	int			 mntlen;
} fuse_fs_info_t;

int				 pscfs_exit_fuse_listener;
int				 newfs_fd[2];
int				 pscfs_nfds;
struct pollfd			 pscfs_fds[MAX_FDS];
static fuse_fs_info_t		 fsinfo[MAX_FDS];
static char			*mountpoints[MAX_FDS];
static pthread_t		 fuse_threads[NUM_THREADS];
struct fuse_session		*fuse_session;
double				 pscfs_entry_timeout;
double				 pscfs_attr_timeout;
struct psc_poolmaster		 pscfs_req_poolmaster;
struct psc_poolmgr		*pscfs_req_pool;

#ifdef HAVE_NO_POLL_DEV
fd_set				*pscfs_fdset;
fd_set				*pscfs_fdset_rd;
size_t				 pscfs_fdset_size;
#endif

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

	if (write(newfs_fd[1], &info, sizeof(info)) != sizeof(info)) {
		perror("Warning (while writing fsinfo to newfs_fd)");
		return (-1);
	}

	if (write(newfs_fd[1], mntpoint, info.mntlen) != info.mntlen) {
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
	pscfs_fds[pscfs_nfds].fd = fd;
	pscfs_fds[pscfs_nfds].events = POLLIN;
	pscfs_nfds++;
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
	 */
	psc_assert(fd_read_loop(pscfs_fds[0].fd, &fs, sizeof(fuse_fs_info_t)) == 0);

	char *mntpoint = PSCALLOC(fs.mntlen + 1);
	if (mntpoint == NULL) {
		fprintf(stderr, "Warning: out of memory!\n");
		return;
	}

	psc_assert(fd_read_loop(pscfs_fds[0].fd, mntpoint, fs.mntlen) == 0);

	mntpoint[fs.mntlen] = '\0';

	if (pscfs_nfds == MAX_FDS) {
		fprintf(stderr, "Warning: filesystem limit (%i) "
		    "reached, unmounting..\n", MAX_FILESYSTEMS);
		fuse_unmount(mntpoint, fs.ch);
		PSCFREE(mntpoint);
		return;
	}

	psclog_info("adding filesystem %i at mntpoint %s", pscfs_nfds, mntpoint);

	fsinfo[pscfs_nfds] = fs;
	mountpoints[pscfs_nfds] = mntpoint;
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
	fuse_session_reset(fsinfo[i].se);
	fuse_session_destroy(fsinfo[i].se);
	close(pscfs_fds[i].fd);
	pscfs_fds[i].fd = -1;

#ifdef HAVE_NO_POLL_DEV
	FD_CLR(pscfs_fds[i].fd, pscfs_fdset);
#endif

	PSCFREE(mountpoints[i]);
}

static void *
pscfs_fuse_listener_loop(__unusedx void *arg)
{
	static psc_spinlock_t lock = SPINLOCK_INIT;
	static struct psc_waitq wq = PSC_WAITQ_INIT;
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
		int ret = poll(pscfs_fds, pscfs_nfds, 1000);
		if (ret == 0 || (ret == -1 && errno == EINTR))
			continue;

		if (ret == -1) {
			perror("poll");
			continue;
		}
#endif

		int oldfds = pscfs_nfds;

		for (i = 0; i < oldfds; i++) {
#ifdef HAVE_NO_POLL_DEV
			if (!FD_ISSET(pscfs_fds[i].fd, pscfs_fdset_rd))
				continue;
#else
			short rev = pscfs_fds[i].revents;

			if (rev == 0)
				continue;

			pscfs_fds[i].revents = 0;

			psc_assert((rev & POLLNVAL) == 0);

			if (!(rev & POLLIN) &&
			    !(rev & POLLERR) && !(rev & POLLHUP))
				continue;
#endif

			if (i == 0) {
				pscfs_fuse_new();
			} else {
				/* Handle request */

				if (fsinfo[i].bufsize > bufsize) {
					char *new_buf = realloc(buf, fsinfo[i].bufsize);
					if (new_buf == NULL) {
						fprintf(stderr, "Warning: out of memory!\n");
						continue;
					}
					buf = new_buf;
					bufsize = fsinfo[i].bufsize;
				}

				int res = fuse_chan_recv(&fsinfo[i].ch,
				    buf, fsinfo[i].bufsize);
				if (res == -1 || fuse_session_exited(fsinfo[i].se)) {
					pscfs_fuse_destroy(i);
					continue;
				}

				if (res == 0)
					continue;

				struct fuse_session *se = fsinfo[i].se;
				struct fuse_chan *ch = fsinfo[i].ch;

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
		for (read_ptr = 0; read_ptr < pscfs_nfds; read_ptr++) {
			if (pscfs_fds[read_ptr].fd == -1)
				continue;
			if (read_ptr != write_ptr) {
				pscfs_fds[write_ptr] = pscfs_fds[read_ptr];
				fsinfo[write_ptr] = fsinfo[read_ptr];
				mountpoints[write_ptr] = mountpoints[read_ptr];
			}
			write_ptr++;
		}
		pscfs_nfds = write_ptr;
	}

	spinlock(&lock);
	busy = 0;
	psc_waitq_wakeone(&wq);
	freelock(&lock);

	return (NULL);
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

int
pscfs_main(void)
{
	int i;

#ifndef __LP64__
	struct pscfs_inumcol *pfic;

#define INUMCOL_SZ (4096 - 1)
	psc_poolmaster_init(&pscfs_inumcol_poolmaster, struct
	    pscfs_inumcol, pfic_lentry, PPMF_AUTO, INUMCOL_SZ,
	    INUMCOL_SZ / 2, INUMCOL_SZ * 2, NULL, NULL, NULL,
	    "inumcol");
	pscfs_inumcol_pool = psc_poolmaster_getmgr(&pscfs_inumcol_poolmaster);
	psc_hashtbl_init(&pscfs_inumcol_hashtbl, 0,
	    struct pscfs_inumcol, pfic_key, pfic_hentry, INUMCOL_SZ * 4,
	    NULL, "inumcol");

	pfic = psc_pool_get(pscfs_inumcol_pool);
	memset(pfic, 0, sizeof(*pfic));
	psc_hashent_init(&pscfs_inumcol_hashtbl, pfic);
	pfic->pfic_pscfs_inum = 1;
	pfic->pfic_key = 1;
	psc_atomic32_set(&pfic->pfic_refcnt, 1);
	psc_hashtbl_add_item(&pscfs_inumcol_hashtbl, pfic);
#endif

	psc_poolmaster_init(&pscfs_req_poolmaster, struct pscfs_req,
	    pfr_lentry, PPMF_AUTO, 64, 64, 1024, NULL, NULL, NULL,
	    "fsrq");
	pscfs_req_pool = psc_poolmaster_getmgr(&pscfs_req_poolmaster);

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

	for (i = 0; i < NUM_THREADS; i++)
		psc_assert(pthread_create(&fuse_threads[i], NULL,
		    pscfs_fuse_listener_loop, NULL) == 0);

	for (i = 0; i < NUM_THREADS; i++) {
		int ret = pthread_join(fuse_threads[i], NULL);
		if (ret != 0)
			fprintf(stderr, "Warning: pthread_join() on "
			    "thread %i returned %i\n", i, ret);
	}

#ifdef DEBUG
	fprintf(stderr, "Exiting...\n");
#endif

	for (i = 1; i < pscfs_nfds; i++) {
		if (pscfs_fds[i].fd == -1)
			continue;

		fuse_session_exit(fsinfo[i].se);
		fuse_session_reset(fsinfo[i].se);
		fuse_unmount(mountpoints[i], fsinfo[i].ch);
		fuse_session_destroy(fsinfo[i].se);

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
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_fuse_req);
	struct pscfs_clientctx *pfcc = &pfr->pfr_clientctx;

	pfcc->pfcc_pid = ctx->pid;
	return (pfcc);
}

void
pscfs_getcreds(struct pscfs_req *pfr, struct pscfs_creds *pcr)
{
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_fuse_req);

	pcr->pcr_uid = ctx->uid;
	if (pscfs_getgroups(pfr, pcr->pcr_gidv, &pcr->pcr_ngid)) {
		pcr->pcr_gid = ctx->gid;
		pcr->pcr_ngid = 1;
	}
}

mode_t
pscfs_getumask(struct pscfs_req *pfr)
{
#if FUSE_VERSION > FUSE_MAKE_VERSION(2,7)
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_fuse_req);

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
pscfs_inum_t
pscfs_inum_fuse2pscfs(fuse_ino_t f_inum, int del)
{
	struct pscfs_inumcol *pfic;
	struct psc_hashbkt *b;
	pscfs_inum_t p_inum;
	uint64_t key;

	key = f_inum;
	b = psc_hashbkt_get(&pscfs_inumcol_hashtbl, &key);
	psc_hashbkt_lock(b);
	pfic = psc_hashtbl_search(&pscfs_inumcol_hashtbl, NULL,
	    NULL, &key);
	p_inum = pfic->pfic_pscfs_inum;
	if (del) {
		psc_atomic32_dec(&pfic->pfic_refcnt);
		if (psc_atomic32_read(&pfic->pfic_refcnt) == 0)
			psc_hashbkt_del_item(&pscfs_inumcol_hashtbl, b,
			    pfic);
		else
			pfic = NULL;
	}
	psc_hashbkt_unlock(b);

	if (del && pfic)
		psc_pool_return(pscfs_inumcol_pool, pfic);

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

fuse_ino_t
pscfs_inum_pscfs2fuse(pscfs_inum_t p_inum, double timeo)
{
	struct pscfs_inumcol *pfic, *t;
	struct psc_hashbkt *b;
	uint64_t key;

	pfic = psc_pool_get(pscfs_inumcol_pool);

	key = (fuse_ino_t)p_inum;
	do {
		b = psc_hashbkt_get(&pscfs_inumcol_hashtbl, &key);
		psc_hashbkt_lock(b);
		t = psc_hashtbl_search(&pscfs_inumcol_hashtbl, NULL,
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
			psc_hashent_init(&pscfs_inumcol_hashtbl, pfic);
			pfic->pfic_pscfs_inum = p_inum;
			pfic->pfic_key = key;
			COMPUTE_EXPIRE(&pfic->pfic_extime, timeo);
			psc_atomic32_set(&pfic->pfic_refcnt, 1);
			psc_hashbkt_add_item(&pscfs_inumcol_hashtbl, b,
			    pfic);
			pfic = NULL;
		}
		psc_hashbkt_unlock(b);
	} while (t);
	if (pfic)
		psc_pool_return(pscfs_inumcol_pool, pfic);
	return (key);
}
#endif

#define RETIFNOTSUP(pfr, call, ...)					\
	do {								\
		if (pscfs.pf_handle_ ## call == NULL) {			\
			pscfs_reply_ ## call((pfr), ## __VA_ARGS__,	\
			    ENOTSUP);					\
			return;						\
		}							\
	} while (0)

void
pscfs_fuse_handle_access(fuse_req_t req, fuse_ino_t inum, int mask)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, access);
	pscfs.pf_handle_access(pfr, INUM_FUSE2PSCFS(inum), mask);
}

void
pscfs_fuse_handle_close(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, close);
	pscfs.pf_handle_close(pfr, fi_getdata(fi));
}

void
pscfs_fuse_handle_closedir(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, closedir);
	pscfs.pf_handle_closedir(pfr, fi_getdata(fi));
}

void
pscfs_fuse_handle_create(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	pfr->pfr_fuse_fi = fi;
	RETIFNOTSUP(pfr, create, 0, 0, 0, NULL, 0, NULL, 0);
	pscfs.pf_handle_create(pfr, INUM_FUSE2PSCFS(pinum), name,
	    fi->flags, mode);
}

void
pscfs_fuse_handle_flush(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, flush);
	pscfs.pf_handle_flush(pfr, fi_getdata(fi));
}

void
pscfs_fuse_handle_fsync(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, fsync);
	pscfs.pf_handle_fsync(pfr, datasync, fi_getdata(fi));
}

void
pscfs_fuse_handle_fsyncdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, fsyncdir);
	pscfs.pf_handle_fsyncdir(pfr, datasync, fi_getdata(fi));
}

void
pscfs_fuse_handle_getattr(fuse_req_t req, fuse_ino_t inum,
    __unusedx struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, getattr, NULL, 0);
	pscfs.pf_handle_getattr(pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_link(fuse_req_t req, fuse_ino_t c_inum,
    fuse_ino_t p_inum, const char *newname)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, link, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_link(pfr, INUM_FUSE2PSCFS(c_inum),
	    INUM_FUSE2PSCFS(p_inum), newname);
}

void
pscfs_fuse_handle_lookup(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, lookup, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_lookup(pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_mkdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, mkdir, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_mkdir(pfr, INUM_FUSE2PSCFS(pinum), name, mode);
}

void
pscfs_fuse_handle_mknod(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, dev_t rdev)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, mknod, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_mknod(pfr, INUM_FUSE2PSCFS(pinum), name, mode,
	    rdev);
}

void
pscfs_fuse_handle_open(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	pfr->pfr_fuse_fi = fi;
	RETIFNOTSUP(pfr, open, NULL, 0);
	pscfs.pf_handle_open(pfr, INUM_FUSE2PSCFS(inum), fi->flags);
}

void
pscfs_fuse_handle_opendir(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	pfr->pfr_fuse_fi = fi;
	RETIFNOTSUP(pfr, opendir, NULL, 0);
	pscfs.pf_handle_opendir(pfr, INUM_FUSE2PSCFS(inum), fi->flags);
}

void
pscfs_fuse_handle_read(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	pfr->pfr_fuse_fi = fi;
	pfr->pfr_buf = PSCALLOC(size);
	RETIFNOTSUP(pfr, read, NULL, 0);
	pscfs.pf_handle_read(pfr, size, off, fi_getdata(fi));
}

void
pscfs_fuse_handle_readdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, readdir, NULL, 0);
	pscfs.pf_handle_readdir(pfr, size, off, fi_getdata(fi));
}

void
pscfs_fuse_handle_readlink(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, readlink, NULL);
	pscfs.pf_handle_readlink(pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_rename(fuse_req_t req, fuse_ino_t oldpinum,
    const char *oldname, fuse_ino_t newpinum, const char *newname)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, rename);
	pscfs.pf_handle_rename(pfr, INUM_FUSE2PSCFS(oldpinum), oldname,
	    INUM_FUSE2PSCFS(newpinum), newname);
}

void
pscfs_fuse_handle_rmdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, rmdir);
	pscfs.pf_handle_rmdir(pfr, INUM_FUSE2PSCFS(pinum), name);
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

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, setattr, NULL, 0);
	stb->st_ino = INUM_FUSE2PSCFS(stb->st_ino);
	pscfs.pf_handle_setattr(pfr, INUM_FUSE2PSCFS(inum), stb,
	    pfl_to_set, fi ? fi_getdata(fi) : NULL);
}

void
pscfs_fuse_handle_statfs(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, statfs, NULL);
	pscfs.pf_handle_statfs(pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_symlink(fuse_req_t req, const char *buf,
    fuse_ino_t pinum, const char *name)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, symlink, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_symlink(pfr, buf, INUM_FUSE2PSCFS(pinum),
	    name);
}

void
pscfs_fuse_handle_umount(__unusedx void *userdata)
{
	if (pscfs.pf_handle_umount)
		pscfs.pf_handle_umount();
}

void
pscfs_fuse_handle_unlink(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	RETIFNOTSUP(pfr, unlink);
	pscfs.pf_handle_unlink(pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_write(fuse_req_t req, __unusedx fuse_ino_t ino,
    const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;
	pfr->pfr_fuse_fi = fi;

	RETIFNOTSUP(pfr, write, 0);
	pscfs.pf_handle_write(pfr, buf, size, off, fi_getdata(fi));
}

void
pscfs_fuse_handle_listxattr(fuse_req_t req, fuse_ino_t ino,
    size_t size)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;

	RETIFNOTSUP(pfr, listxattr, NULL, 0);
	pscfs.pf_handle_listxattr(pfr, size, INUM_FUSE2PSCFS(ino));
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

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;

	RETIFNOTSUP(pfr, setxattr);
	pscfs.pf_handle_setxattr(pfr, name, value, size,
	    INUM_FUSE2PSCFS(ino));
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

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;

	RETIFNOTSUP(pfr, getxattr, NULL, 0);
	pscfs.pf_handle_getxattr(pfr, name, size, INUM_FUSE2PSCFS(ino));
}

void
pscfs_fuse_handle_removexattr(fuse_req_t req, fuse_ino_t ino,
    const char *name)
{
	struct pscfs_req *pfr;

	pfr = psc_pool_get(pscfs_req_pool);
	pfr->pfr_fuse_req = req;

	RETIFNOTSUP(pfr, removexattr);
	pscfs.pf_handle_removexattr(pfr, name, INUM_FUSE2PSCFS(ino));
}

/* Begin system call reply routines */

void
pscfs_reply_access(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_close(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_closedir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_create(struct pscfs_req *pfr, pscfs_inum_t inum,
    pscfs_fgen_t gen, double entry_timeout, const struct stat *stb,
    double attr_timeout, void *data, int rflags, int rc)
{
	struct fuse_entry_param e;

	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		if (rflags & PSCFS_CREATEF_DIO)
			pfr->pfr_fuse_fi->direct_io = 1;
		fi_setdata(pfr->pfr_fuse_fi, data);
		e.entry_timeout = entry_timeout;
		e.ino = INUM_PSCFS2FUSE(inum, entry_timeout);
		if (e.ino) {
			e.attr_timeout = attr_timeout;
			memcpy(&e.attr, stb, sizeof(e.attr));
			e.attr.st_ino = e.ino;
			e.generation = gen;
		}
		fuse_reply_create(pfr->pfr_fuse_req, &e, pfr->pfr_fuse_fi);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_flush(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_fsync(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_fsyncdir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_getattr(struct pscfs_req *pfr, struct stat *stb,
    double attr_timeout, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		stb->st_ino = INUM_PSCFS2FUSE(stb->st_ino, attr_timeout);
		fuse_reply_attr(pfr->pfr_fuse_req, stb, attr_timeout);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_ioctl(struct pscfs_req *pfr)
{
//	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_open(struct pscfs_req *pfr, void *data, int rflags, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		if (rflags & PSCFS_OPENF_KEEPCACHE)
			pfr->pfr_fuse_fi->keep_cache = 1;
		if (rflags & PSCFS_OPENF_DIO)
			pfr->pfr_fuse_fi->direct_io = 1;

#ifdef HAVE_NO_FUSE_PRIVATE_MMAP
		/*
		 * FUSE direct_io does not work with mmap(), which is
		 * what the kernel uses under the hood when running
		 * executables, so disable it for this case.
		 *
		 * XXX provide some kind of knob to allow direct_io to
		 * be turned off, exposed to userland (mz/per-process).
		 */
		if ((c->fcmh_sstb.sst_mode &
		    (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
			pfr->pfr_fuse_fi->direct_io = 0;
#endif

		fi_setdata(pfr->pfr_fuse_fi, data);
		fuse_reply_open(pfr->pfr_fuse_req, pfr->pfr_fuse_fi);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_opendir(struct pscfs_req *pfr, void *data, int rflags, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		if (rflags & PSCFS_OPENF_KEEPCACHE)
			pfr->pfr_fuse_fi->keep_cache = 1;
		if (rflags & PSCFS_OPENF_DIO)
			pfr->pfr_fuse_fi->direct_io = 1;
		fi_setdata(pfr->pfr_fuse_fi, data);
		fuse_reply_open(pfr->pfr_fuse_req, pfr->pfr_fuse_fi);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_read(struct pscfs_req *pfr, void *buf, ssize_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);

	psc_assert(buf == pfr->pfr_buf);
	PSCFREE(pfr->pfr_info);
	PSCFREE(pfr->pfr_buf);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_readdir(struct pscfs_req *pfr, void *buf, ssize_t len, int rc)
{
	struct pscfs_dirent *dirent;
	off_t off;

	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		for (dirent = buf, off = 0; off < len;
		    off += PFL_DIRENT_SIZE(dirent->pfd_namelen),
		    dirent = (void *)((char *)buf + off))
			dirent->pfd_ino = INUM_PSCFS2FUSE(dirent->pfd_ino, 8);
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_readlink(struct pscfs_req *pfr, void *buf, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_readlink(pfr->pfr_fuse_req, buf);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_rename(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_rmdir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_setattr(struct pscfs_req *pfr, struct stat *stb,
    double attr_timeout, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		stb->st_ino = INUM_PSCFS2FUSE(stb->st_ino, attr_timeout);
		fuse_reply_attr(pfr->pfr_fuse_req, stb, attr_timeout);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_statfs(struct pscfs_req *pfr, const struct statvfs *sfb,
    int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_statfs(pfr->pfr_fuse_req, sfb);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_umount(struct pscfs_req *pfr)
{
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_unlink(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_write(struct pscfs_req *pfr, ssize_t len, int rc)
{
	PSCFREE(pfr->pfr_info);

	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_write(pfr->pfr_fuse_req, len);

	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_listxattr(struct pscfs_req *pfr, void *buf, size_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else if (buf)
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);
	else
		fuse_reply_xattr(pfr->pfr_fuse_req, len);

	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_setxattr(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_getxattr(struct pscfs_req *pfr, void *buf, size_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else if (buf)
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);
	else
		fuse_reply_xattr(pfr->pfr_fuse_req, len);

	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_reply_removexattr(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
	psc_pool_return(pscfs_req_pool, pfr);
}

void
pscfs_fuse_replygen_entry(struct pscfs_req *pfr, pscfs_inum_t inum,
    pscfs_fgen_t gen, double entry_timeout, const struct stat *stb,
    double attr_timeout, int rc)
{
	struct fuse_entry_param e;

	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		e.entry_timeout = entry_timeout;
		e.ino = INUM_PSCFS2FUSE(inum, entry_timeout);
		if (e.ino) {
			e.attr_timeout = attr_timeout;
			memcpy(&e.attr, stb, sizeof(e.attr));
			e.attr.st_ino = e.ino;
			e.generation = gen;
		}
		fuse_reply_entry(pfr->pfr_fuse_req, &e);
	}
	psc_pool_return(pscfs_req_pool, pfr);
}

struct fuse_lowlevel_ops pscfs_fuse_ops = {
	.access		= pscfs_fuse_handle_access,
	.create		= pscfs_fuse_handle_create,
	.destroy	= pscfs_fuse_handle_umount,
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
	.release	= pscfs_fuse_handle_close,
	.releasedir	= pscfs_fuse_handle_closedir,
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
		psc_fatal("fuse_mount");

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

	*ng = fuse_req_getgroups(pfr->pfr_fuse_req, NGROUPS_MAX, gv);
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
		    fuse_req_ctx(pfr->pfr_fuse_req)->uid,
		    fuse_req_ctx(pfr->pfr_fuse_req)->gid, gv, ng);
	} else
		rc = abs(*ng);
	return (rc);
}

int
pscfs_notify_inval_entry(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, size_t namelen)
{
	int rc;

#ifdef HAVE_FUSE_REQ_GETCHANNEL
	rc = fuse_lowlevel_notify_inval_entry(fuse_req_getchannel(
	    pfr->pfr_fuse_req), INUM_PSCFS2FUSE(pinum, 0.0), name,
	    namelen);
#else
	(void)pfr;
	(void)pinum;
	(void)name;
	(void)namelen;
	rc = -ENOTSUP;
#endif
	return (rc);
}
