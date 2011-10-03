/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2010-2011, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/types.h>

#include <nnpfs/nnpfs_message.h>

#include <stdio.h>

int pscfs_nnpfs_kernelfd;

int
pscfs_nnpfs_opendev(const char *fn)
{
	pscfs_nnpfs_kernelfd = open(fd, O_RDWR);
}

int
pscfs_main(void)
{
	int i;

#ifndef __LP64__
	struct pscfs_inumcol *pfic;

#define INUMCOL_SZ (4096 - 1)
	psc_poolmaster_init(&pscfs_inumcol_poolmaster, struct pscfs_inumcol,
	    pfic_lentry, PPMF_AUTO, INUMCOL_SZ, INUMCOL_SZ / 2, INUMCOL_SZ * 2,
	    NULL, NULL, NULL, "inumcol");
	pscfs_inumcol_pool = psc_poolmaster_getmgr(&pscfs_inumcol_poolmaster);
	psc_hashtbl_init(&pscfs_inumcol_hashtbl, 0, struct pscfs_inumcol,
	    pfic_key, pfic_hentry, INUMCOL_SZ * 4, NULL, "inumcol");

	pfic = psc_pool_get(pscfs_inumcol_pool);
	memset(pfic, 0, sizeof(*pfic));
	psc_hashent_init(&pscfs_inumcol_hashtbl, pfic);
	pfic->pfic_pscfs_inum = 1;
	pfic->pfic_key = 1;
	psc_atomic32_set(&pfic->pfic_refcnt, 1);
	psc_hashtbl_add_item(&pscfs_inumcol_hashtbl, pfic);
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

	for (i = 1; i < nfds; i++) {
		if (fds[i].fd == -1)
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

void
pscfs_getcreds(struct pscfs_req *pfr, struct pscfs_cred *pfc)
{
	const struct fuse_ctx *ctx = fuse_req_ctx(pfr->pfr_fuse_req);

	pfc->pfc_uid = ctx->uid;
	pfc->pfc_gid = ctx->gid;
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
	return (0644);
}

#if 0
int
pscfs_inum_reclaim(struct psc_poolmgr *m)
{
	time_t now;
	int nrel = 0;

	now = time(NULL);
	PSC_HASHTBL_FOREACH_BUCKET(t, b) {
		psc_hashbkt_lock(b);
		PSC_HASHBKT_FOREACH_ENTRY(t, b, pfic) {
			if (pfic->pfic_extime &&
			    now > pfic->pfic_extime &&
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

fuse_ino_t
pscfs_inum_pscfs2fuse(pscfs_inum_t p_inum, int timeo)
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
			 * inum; otherwise, fallback to a unique
			 * random value.
			 */
			if (t->pfic_pscfs_inum == p_inum) {
				psc_atomic32_inc(&t->pfic_refcnt);
				t->pfic_extime = time(NULL) + timeo;
				key = t->pfic_key;
				t = NULL;
			} else
				key = psc_random32();
		} else {
			memset(pfic, 0, sizeof(*pfic));
			psc_hashent_init(&pscfs_inumcol_hashtbl, pfic);
			pfic->pfic_pscfs_inum = p_inum;
			pfic->pfic_key = key;
			pfic->pfic_extime = time(NULL) + timeo;
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
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, access);
	pscfs.pf_handle_access(&pfr, INUM_FUSE2PSCFS(inum), mask);
}

void
pscfs_fuse_handle_close(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, close);
	pscfs.pf_handle_close(&pfr, fi_getdata(fi));
}

void
pscfs_fuse_handle_closedir(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, closedir);
	pscfs.pf_handle_closedir(&pfr, fi_getdata(fi));
}

void
pscfs_fuse_handle_create(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	RETIFNOTSUP(&pfr, create, 0, 0, 0, NULL, 0, NULL, 0);
	pscfs.pf_handle_create(&pfr, INUM_FUSE2PSCFS(pinum), name,
	    fi->flags, mode);
}

void
pscfs_fuse_handle_flush(fuse_req_t req, __unusedx fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, flush);
	pscfs.pf_handle_flush(&pfr, fi_getdata(fi));
}

void
pscfs_fuse_handle_fsync(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, fsync);
	pscfs.pf_handle_fsync(&pfr, datasync, fi_getdata(fi));
}

void
pscfs_fuse_handle_fsyncdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    int datasync, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, fsyncdir);
	pscfs.pf_handle_fsyncdir(&pfr, datasync, fi_getdata(fi));
}

void
pscfs_fuse_handle_getattr(fuse_req_t req, fuse_ino_t inum,
    __unusedx struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, getattr, NULL, 0);
	pscfs.pf_handle_getattr(&pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_link(fuse_req_t req, fuse_ino_t c_inum,
    fuse_ino_t p_inum, const char *newname)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, link, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_link(&pfr, INUM_FUSE2PSCFS(c_inum),
	    INUM_FUSE2PSCFS(p_inum), newname);
}

void
pscfs_fuse_handle_lookup(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, lookup, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_lookup(&pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_mkdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, mkdir, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_mkdir(&pfr, INUM_FUSE2PSCFS(pinum), name, mode);
}

void
pscfs_fuse_handle_mknod(fuse_req_t req, fuse_ino_t pinum,
    const char *name, mode_t mode, dev_t rdev)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, mknod, 0, 0, 0, NULL, 0);
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
	RETIFNOTSUP(&pfr, open, NULL, 0);
	pscfs.pf_handle_open(&pfr, INUM_FUSE2PSCFS(inum), fi->flags);
}

void
pscfs_fuse_handle_opendir(fuse_req_t req, fuse_ino_t inum,
    struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	pfr.pfr_fuse_fi = fi;
	RETIFNOTSUP(&pfr, opendir, NULL, 0);
	pscfs.pf_handle_opendir(&pfr, INUM_FUSE2PSCFS(inum), fi->flags);
}

void
pscfs_fuse_handle_read(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, read, NULL, 0);
	pscfs.pf_handle_read(&pfr, size, off, fi_getdata(fi));
}

void
pscfs_fuse_handle_readdir(fuse_req_t req, __unusedx fuse_ino_t inum,
    size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, readdir, NULL, 0);
	pscfs.pf_handle_readdir(&pfr, size, off, fi_getdata(fi));
}

void
pscfs_fuse_handle_readlink(fuse_req_t req, fuse_ino_t inum)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, readlink, NULL);
	pscfs.pf_handle_readlink(&pfr, INUM_FUSE2PSCFS(inum));
}

void
pscfs_fuse_handle_rename(fuse_req_t req, fuse_ino_t oldpinum,
    const char *oldname, fuse_ino_t newpinum, const char *newname)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, rename);
	pscfs.pf_handle_rename(&pfr, INUM_FUSE2PSCFS(oldpinum), oldname,
	    INUM_FUSE2PSCFS(newpinum), newname);
}

void
pscfs_fuse_handle_rmdir(fuse_req_t req, fuse_ino_t pinum,
    const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, rmdir);
	pscfs.pf_handle_rmdir(&pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_setattr(fuse_req_t req, fuse_ino_t inum,
    struct stat *stb, int fuse_to_set,
    __unusedx struct fuse_file_info *fi)
{
	struct pscfs_req pfr;
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

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, setattr, NULL, 0);
	stb->st_ino = INUM_FUSE2PSCFS(stb->st_ino);
	pscfs.pf_handle_setattr(&pfr, INUM_FUSE2PSCFS(inum), stb,
	    pfl_to_set, fi ? fi_getdata(fi) : NULL);
}

void
pscfs_fuse_handle_statfs(fuse_req_t req, __unusedx fuse_ino_t inum)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, statfs, NULL);
	pscfs.pf_handle_statfs(&pfr);
}

void
pscfs_fuse_handle_symlink(fuse_req_t req, const char *buf,
    fuse_ino_t pinum, const char *name)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, symlink, 0, 0, 0, NULL, 0);
	pscfs.pf_handle_symlink(&pfr, buf, INUM_FUSE2PSCFS(pinum),
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
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, unlink);
	pscfs.pf_handle_unlink(&pfr, INUM_FUSE2PSCFS(pinum), name);
}

void
pscfs_fuse_handle_write(fuse_req_t req, __unusedx fuse_ino_t ino,
    const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct pscfs_req pfr;

	pfr.pfr_fuse_req = req;
	RETIFNOTSUP(&pfr, write, 0);
	pscfs.pf_handle_write(&pfr, buf, size, off, fi_getdata(fi));
}

/* Begin system call reply routines */

void
pscfs_reply_access(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_reply_close(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_reply_closedir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
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
}

void
pscfs_reply_flush(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_reply_fsync(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_reply_fsyncdir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
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
}

void
pscfs_reply_ioctl(struct pscfs_req *pfr)
{
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
		fi_setdata(pfr->pfr_fuse_fi, data);
		fuse_reply_open(pfr->pfr_fuse_req, pfr->pfr_fuse_fi);
	}
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
}

void
pscfs_reply_read(struct pscfs_req *pfr, void *buf, ssize_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_buf(pfr->pfr_fuse_req, buf, len);
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
}

void
pscfs_reply_readlink(struct pscfs_req *pfr, void *buf, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_readlink(pfr->pfr_fuse_req, buf);
}

void
pscfs_reply_rename(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_reply_rmdir(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
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
}

void
pscfs_reply_statfs(struct pscfs_req *pfr, struct statvfs *sfb,
    int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_statfs(pfr->pfr_fuse_req, sfb);
}

void
pscfs_reply_umount(struct pscfs_req *pfr)
{
}

void
pscfs_reply_unlink(struct pscfs_req *pfr, int rc)
{
	fuse_reply_err(pfr->pfr_fuse_req, rc);
}

void
pscfs_reply_write(struct pscfs_req *pfr, ssize_t len, int rc)
{
	if (rc)
		fuse_reply_err(pfr->pfr_fuse_req, rc);
	else
		fuse_reply_write(pfr->pfr_fuse_req, len);
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

void
pscfs_mount(const char *mp, struct pscfs_args *pfa)
{
	struct fuse_chan *ch;
	char nameopt[BUFSIZ];
	int rc;

	if (pipe(newfs_fd) == -1)
		psc_fatal("pipe");

	fds[0].fd = newfs_fd[0];
	fds[0].events = POLLIN;
	nfds = 1;

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
