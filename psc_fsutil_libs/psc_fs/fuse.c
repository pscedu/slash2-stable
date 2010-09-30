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

#include <sys/poll.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fuse_lowlevel.h>

#include "pfl/fs.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

#ifdef __LP64__
#  include "pfl/hashtbl.h"
#endif

#define NUM_THREADS	32
#define MAX_FILESYSTEMS	5
#define MAX_FDS		(MAX_FILESYSTEMS + 1)

void	slash2fuse_listener_exit(void);
int	slash2fuse_listener_init(void);
int	slash2fuse_listener_start(void);
int	slash2fuse_newfs(const char *, struct fuse_chan *);

typedef struct {
	int			 fd;
	size_t			 bufsize;
	struct fuse_chan	*ch;
	struct fuse_session	*se;
	int			 mntlen;
} fuse_fs_info_t;

int		 exit_fuse_listener = 0;
int		 newfs_fd[2];
int		 nfds;
struct pollfd	 fds[MAX_FDS];
fuse_fs_info_t	 fsinfo[MAX_FDS];
char		*mountpoints[MAX_FDS];
pthread_t	 fuse_threads[NUM_THREADS];

int
pscfs_fuse_listener_init(void)
{
	if (pipe(newfs_fd) == -1) {
		perror("pipe");
		return (-1);
	}

	fds[0].fd = newfs_fd[0];
	fds[0].events = POLLIN;
	nfds = 1;

	return (0);
}

void
pscfs_fuse_listener_exit(void)
{
	close(newfs_fd[0]);
	close(newfs_fd[1]);
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
	psc_assert(fd_read_loop(fds[0].fd, &fs, sizeof(fuse_fs_info_t)) == 0);

	char *mntpoint = PSCALLOC(fs.mntlen + 1);
	if (mntpoint == NULL) {
		fprintf(stderr, "Warning: out of memory!\n");
		return;
	}

	psc_assert(fd_read_loop(fds[0].fd, mntpoint, fs.mntlen) == 0);

	mntpoint[fs.mntlen] = '\0';

	if (nfds == MAX_FDS) {
		fprintf(stderr, "Warning: filesystem limit (%i) "
		    "reached, unmounting..\n", MAX_FILESYSTEMS);
		fuse_unmount(mntpoint, fs.ch);
		PSCFREE(mntpoint);
		return;
	}

	psc_info("adding filesystem %i at mntpoint %s", nfds, mntpoint);

	fsinfo[nfds] = fs;
	mountpoints[nfds] = mntpoint;

	fds[nfds].fd = fs.fd;
	fds[nfds].events = POLLIN;
	fds[nfds].revents = 0;
	nfds++;
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
	close(fds[i].fd);
	fds[i].fd = -1;
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

	while (!exit_fuse_listener) {
		int i;
		int ret = poll(fds, nfds, 1000);
		if (ret == 0 || (ret == -1 && errno == EINTR))
			continue;

		if (ret == -1) {
			perror("poll");
			continue;
		}

		int oldfds = nfds;

		for (i = 0; i < oldfds; i++) {
			short rev = fds[i].revents;

			if (rev == 0)
				continue;

			fds[i].revents = 0;

			psc_assert((rev & POLLNVAL) == 0);

			if (!(rev & POLLIN) &&
			    !(rev & POLLERR) && !(rev & POLLHUP))
				continue;

			if (i == 0) {
				new_fs();
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
					destroy_fs(i);
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
		for (read_ptr = 0; read_ptr < nfds; read_ptr++) {
			if (fds[read_ptr].fd == -1)
				continue;
			if (read_ptr != write_ptr) {
				fds[write_ptr] = fds[read_ptr];
				fsinfo[write_ptr] = fsinfo[read_ptr];
				mountpoints[write_ptr] = mountpoints[read_ptr];
			}
			write_ptr++;
		}
		nfds = write_ptr;
	}

	spinlock(&lock);
	busy = 0;
	psc_waitq_wakeone(&wq);
	freelock(&lock);

	return (NULL);
}

int
pscfs_fuse_listener_start(void)
{
	int i;

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

	for (i = 1; i < nfds; i++) {
		if (fds[i].fd == -1)
			continue;

		fuse_session_exit(fsinfo[i].se);
		fuse_session_reset(fsinfo[i].se);
		fuse_unmount(mountpoints[i], fsinfo[i].ch);
		fuse_session_destroy(fsinfo[i].se);

		PSCFREE(mountpoints[i]);
	}

	return (1);
}

__static void
pscfs_fuse_getcred(struct pscfs_req *pfr, struct pscfs_cred *pfc)
{
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_fuse_req);

	pfc->pfc_uid = ctx->uid;
	pfc->pfc_gid = ctx->gid;
}

static mode_t
pscfs_fuse_getumask(struct pscfs_req *pfr)
{
#if FUSE_VERSION > FUSE_MAKE_VERSION(2,7)
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_fuse_req);

	return (ctx->umask);
#else
	(void)pfr;
	/* XXX read from /proc ? */
	return (0644);
#endif
}

#ifdef __LP64__
#  define INUM_FUSE2PSCFS(inum, del)	(inum)
#  define INUM_ADD_PSCFS2FUSE(inum)	(inum)
#else
#  define INUM_FUSE2PSCFS(inum, del)	pscfs_inum_fuse2pscfs((inum), (del))
#  define INUM_ADD_PSCFS2FUSE(inum)	pscfs_inum_fuse2pscfs(inum)

struct psc_hashtbl pscfs_inumcol_hashtbl;

struct pscfs_fuse_inumcol {
	pscfs_inum_t		pfic_pscfs_inum;
	uint64_t		pfic_key;		/* fuse inum */
	psc_atomic32_t		pfic_refcnt;
	struct psc_hashent	pfic_hentry;
}

pscfs_inum_t
pscfs_inum_fuse2pscfs(fuse_ino_t f_inum, int del)
{
	struct pscfs_fuse_inumcol *pfic;
	pscfs_inum_t p_inum;
	uint64_t key;

	key = f_inum;
	b = psc_hashbkt_get(&pscfs_inumcol_hashtbl, &n);
	psc_hashbkt_lock(b);
	pfic = psc_hashtbl_search(&pscfs_inumcol_hashtbl, NULL,
	    NULL, &key);
	p_inum = pfic->pfic_pscfs_inum;
	if (del) {
		psc_atomic32_dec(&pfic->pfic_refcnt);
		if (pfic->pfic_refcnt == 0)
			psc_hashbkt_del_item(&pscfs_inum_hashtbl,
			    b, pfic);
		else
			pfic = NULL;
	}
	psc_hashbkt_unlock(b);

	if (del && pfic)
		psc_pool_return(pscfs_inumcol_pool, p);

	return (p_inum);
}

fuse_ino_t
pscfs_inum_add_pscfs2fuse(pscfs_ino_t p_inum)
{
	struct pscfs_fuse_inumcol *pfic, *t;
	fuse_ino_t f_inum;
	uint64_t key;

	pfic = psc_pool_get(pscfs_inumcol_pool);

	key = (fuse_ino_t)p_inum;
	do {
		b = psc_hashbkt_get(&pscfs_inumcol_hashtbl, &key);
		psc_hashbkt_lock(b);
		t = psc_hashtbl_search(&pscfs_inumcol_hashtbl, NULL,
		    NULL, &n);
		if (t) {
			/*
			 * This faux inum is already in table.  If this
			 * is for the same real inum, reuse this faux
			 * inum; otherwise, fallback to a unique
			 * random value.
			 */
			if (t->pfic_pscfs_inum == p_inum) {
				psc_atomic32_inc(&t->pfic_refcnt);
				key = t->pfic_key;
				t = NULL;
			} else
				key = psc_random32();
		} else {
			psc_hashent_init(&pscfs_inumcol_hashtbl, pfic);
			pfic->pfic_pscfs_inum = p_inum;
			pfic->pfic_key = key;
			psc_atomic32_set(&pfic->pfic_refcnt, 1);
			psc_hashbkt_add_item(&pscfs_inumcol_hashtbl,
			    b, pfic);
			pfic = NULL;
		}
		psc_hashbkt_unlock(b);
	} while (t);
	if (pfic)
		psc_pool_return(pscfs_inumcol_pool, pfic);
	return (key);
}
#endif

void
pscfs_fuse_handle_access(fuse_req_t req, fuse_ino_t inum, int mask)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_access(&pfr, INUM_FUSE2PSCFS(inum), mask);
}

void
pscfs_fuse_handle_close(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_close(&pfr, &pfi);
}

void
pscfs_fuse_handle_closedir(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_closedir(&pfr, &pfi);
}

void
pscfs_fuse_handle_create(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_create(&pfr, INUM_FUSE2PSCFS(pinum), name, mode,
	    &pfi);
}

void
pscfs_fuse_handle_flush(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_flush(&pfr, &pfi);
}

void
pscfs_fuse_handle_fsync(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_fsync(&pfr, datasync, &pfi);
}

void
pscfs_fuse_handle_fsyncdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_fsync(&pfr, datasync, &pfi);
}

void
pscfs_fuse_handle_getattr(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_getattr(&pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_link(fuse_req_t req, fuse_ino_t c_inum,
    fuse_ino_t p_inum, const char *newname)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_link(&pfr, INUM_FUSE2PSCFS(c_inum),
	    INUM_FUSE2PSCFS(p_inum), newname);
}

void
pscfs_fuse_handle_lookup(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_lookup(&pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_mkdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_mkdir(&pfr, INUM_FUSE2PSCFS(pinum), name, mode);
}

void
pscfs_fuse_handle_mknod(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, dev_t rdev)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_mknod(&pfr, INUM_FUSE2PSCFS(pinum), name, mode,
	    rdev);
}

void
pscfs_fuse_handle_open(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_open(&pfr, INUM_FUSE2PSCFS(inum), &pfi);
}

void
pscfs_fuse_handle_opendir(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_opendir(&pfr, INUM_FUSE2PSCFS(inum), &pfi);
}

void
pscfs_fuse_handle_read(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_read(&pfr, size, off, &pfi);
}

void
pscfs_fuse_handle_readdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_readdir(&pfr, size, off, &pfi);
}

void
pscfs_fuse_handle_readlink(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_readlink(&pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_rename(fuse_req_t req, fuse_ino_t oldpinum,
    const char *oldname, fuse_ino_t newpinum, const char *newname)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_rename(&pfr, INUM_FUSE2PSCFS(oldpinum), oldname,
	    INUM_FUSE2PSCFS(newpinum), newname);
}

void
pscfs_fuse_handle_rmdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_rmdir(&pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_setattr(fuse_req_t req, fuse_ino_t inum,
    struct stat *stb, int fuse_to_set, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;
	int pfl_to_set = 0;

	if (fuse_to_set & FUSE_SET_ATTR_MODE)
		pfl_to_set |= SETATTR_MASKF_MODE;
	if (fuse_to_set & FUSE_SET_ATTR_UID)
		pfl_to_set |= SETATTR_MASKF_UID;
	if (fuse_to_set & FUSE_SET_ATTR_GID)
		pfl_to_set |= SETATTR_MASKF_GID;
	if (fuse_to_set & FUSE_SET_ATTR_SIZE)
		pfl_to_set |= SETATTR_MASKF_DATASIZE;
	if (fuse_to_set & FUSE_SET_ATTR_ATIME)
		pfl_to_set |= SETATTR_MASKF_ATIME;
	if (fuse_to_set & FUSE_SET_ATTR_MTIME)
		pfl_to_set |= SETATTR_MASKF_MTIME;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_setattr(&pfr, INUM_FUSE2PSCFS(inum), stb,
	    pfl_to_set);
}

void
pscfs_fuse_handle_statfs(fuse_req_t req, __unusedx fuse_ino_t inum)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_statfs(&pfr);
}

void
pscfs_fuse_handle_symlink(fuse_req_t req, const char *buf,
    fuse_ino_t pinum, const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_symlink(&pfr, buf, INUM_FUSE2PSCFS(pinum),
	    name);
}

void
pscfs_fuse_handle_umount(__unusedx void *userdata)
{
	pscfs.pf_handle_umount();
}

void
pscfs_fuse_handle_unlink(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pscfs.pf_handle_unlink(&pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_write(fuse_req_t req, __unusedx fuse_ino_t ino,
    const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	pscfs.pf_handle_write(&pfr, buf, size, off, &pfi);
}

void
pscfs_fuse_reply_access(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_close(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_closedir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_create(struct pscfs_req *pfr, pscfs_inum_t inum,
    pscfs_fgen_t gen, int entry_timeout, struct stat *stb,
    int attr_timeout, void *data, int rc);
{
	struct fuse_entry_param e;

	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		pfr->pfr_fuse_fi->fh = data;
		e->entry_timeout = entry_timeout;
		e->ino = INUM_PSCFS2FUSE(inum);
		if (e->ino) {
			e->attr_timeout = attr_timeout;
			memcpy(&e->attr, stb, sizeof(e->attr));
			e->generation = gen;
		}
		fuse_reply_create(pfr->pfr_fuse_req, &e, pfr->pfr_fuse_fi);
	}
}

void
pscfs_fuse_reply_flush(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_fsync(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_fsyncdir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_getattr(struct pscfs_req *pfr, struct stat *stb,
    int timeout, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_attr(pfr->pfr_fuse_req, stb, timeout);
}

void
pscfs_fuse_reply_ioctl(struct pscfs_req *pfr)
{
}

void
pscfs_fuse_replygen_entry(struct pscfs_req *pfr, pscfs_inum_t inum,
    pscfs_fgen_t gen, int entry_timeout, struct stat *stb,
    int attr_timeout, int rc)
{
	struct fuse_entry_param e;

	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		e->entry_timeout = entry_timeout;
		e->ino = INUM_PSCFS2FUSE(inum);
		if (e->ino) {
			e->attr_timeout = attr_timeout;
			memcpy(&e->attr, stb, sizeof(e->attr));
			e->generation = gen;
		}
		fuse_reply_entry(pfr->pfr_fuse_req, &e);
	}
}

void
pscfs_fuse_reply_mknod(struct pscfs_req *pfr)
{
	fuse_reply_err(pfr->pfr_fuse_req, ENOTSUP);
}

void
pscfs_fuse_reply_open(struct pscfs_req *pfr, void *data, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		pfr->pfr_fuse_fi->fh = data;
		fuse_reply_open(pfr->pfr_fuse_req, pfr->pfr_fuse_fi);
	}
}

void
pscfs_fuse_reply_opendir(struct pscfs_req *pfr, void *data, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else {
		pfr->pfr_fuse_fi->fh = data;
		fuse_reply_open(pfr->pfr_fuse_req, pfr->pfr_fuse_fi);
	}
}

void
pscfs_fuse_reply_read(struct pscfs_req *pfr, void *buf, ssize_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);
}

void
pscfs_fuse_reply_readdir(struct pscfs_req *pfr, void *buf, ssize_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);
}

void
pscfs_fuse_reply_readlink(struct pscfs_req *pfr, void *buf, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_readlink(pfr->pfr_fuse_req, buf);
}

void
pscfs_fuse_reply_rename(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_rmdir(struct pscfs_req *pfr)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_setattr(struct pscfs_req *pfr, struct stat *stb,
    int timeout, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_attr(pfr->pfr_fuse_req, stb, timeout);
}

void
pscfs_fuse_reply_statfs(struct pscfs_req *pfr, struct statvfs *sfb,
    int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_statfs(pfr->pfr_fuse_req, sfb);
}

void
pscfs_fuse_reply_umount(struct pscfs_req *pfr)
{
}

void
pscfs_fuse_reply_unlink(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_fuse_reply_write(struct pscfs_req *pfr, ssize_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_write(pfr->pfr_fuse_req, len);
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
	.write		= pscfs_fuse_handle_write
};
