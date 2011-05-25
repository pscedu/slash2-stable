/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>

#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fuse.h>
#include <fuse_opt.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/stat.h"
#include "pfl/str.h"
#include "pfl/time.h"
#include "psc_ds/list.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/pool.h"
#include "psc_util/rlimit.h"
#include "psc_util/timerthr.h"

#include "ctl_vesuvifs.h"
#include "inode.h"
#include "mount_vesuvifs.h"
#include "slab.h"

#define ffi_getvfh(fi)		((void *)(unsigned long)(fi)->fh)
#define ffi_setvfh(fi, vfh)	((fi)->fh = (uint64_t)(unsigned long)(vfh))

struct vfh {
	int			 vfh_decr_rlim_fd;
	int			 vfh_oflags;
	struct psc_listentry	 vfh_lentry;
	struct vinode		*vfh_inode;
};

vinum_t                  vesuvifs_root_id;

struct psc_vbitmap	 mvfsthr_uniqidmap = VBITMAP_INIT_AUTO;
psc_spinlock_t		 mvfsthr_uniqidmap_lock = SPINLOCK_INIT;

int			 unmount_first;		/* umount(2) before mounting */
char			 mountpoint[PATH_MAX];
char			 backfs[PATH_MAX];
char			 ctlsockfn[] = VPATH_CTLSOCK;
const char		*noncanon_mp;		/* noncanonical mountpoint */
const char		*progname;

struct psc_poolmaster	 slab_pool_master;
struct psc_poolmgr	*slab_pool;

struct psc_poolmaster	 vfh_pool_master;
struct psc_poolmgr	*vfh_pool;

struct psc_poolmaster	 ino_pool_master;
struct psc_poolmgr	*ino_pool;

struct psc_lockedlist	 psc_mlists;
struct psc_lockedlist	 psc_meters;
struct psclist_head	 pscrpc_all_services;
psc_spinlock_t		 pscrpc_all_services_lock;

int
translate_path(const char *relpath, char fn[PATH_MAX])
{
	int rc;

	rc = snprintf(fn, PATH_MAX, "%s%s", backfs, relpath);
	if (rc == -1)
		return (-errno);
	if (rc >= PATH_MAX)
		return (-ENAMETOOLONG);
	return (0);
}

void
mvfsthr_teardown(void *arg)
{
	struct mvfs_thread *mft = arg;

	spinlock(&mvfsthr_uniqidmap_lock);
	psc_vbitmap_unset(&mvfsthr_uniqidmap, mft->mft_uniqid);
	psc_vbitmap_setnextpos(&mvfsthr_uniqidmap, 0);
	freelock(&mvfsthr_uniqidmap_lock);
}

void
mvfsthr_ensure(void)
{
	struct mvfs_thread *mft;
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL) {
		size_t id;

		spinlock(&mvfsthr_uniqidmap_lock);
		if (psc_vbitmap_next(&mvfsthr_uniqidmap, &id) == -1)
			psc_fatal("psc_vbitmap_next");
		freelock(&mvfsthr_uniqidmap_lock);

		thr = pscthr_init(MVTHRT_FS, 0, NULL,
		    mvfsthr_teardown, sizeof(*mft), "mvfsthr%02zu", id);
		mft = thr->pscthr_private;
		mft->mft_uniqid = id;
		pscthr_setready(thr);
	}
	psc_assert(thr->pscthr_type == MVTHRT_FS);
}

int
vesufsop_access(const char *rp, int mask)
{
	char fn[PATH_MAX];
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rp, fn);
	if (rc)
		return (rc);
	if (access(fn, mask) == -1)
		return (-errno);
	return (0);
}

int
vesufsop_chmod(const char *rp, mode_t mode)
{
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = VINODE_LOADFN(rp, &vi);
	if (rc)
		return (rc);
	vi->vino_stb.st_mode = mode;
	VINODE_DECREF(vi);
	return (rc);
}

int
vesufsop_chown(const char *rp, uid_t uid, gid_t gid)
{
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = VINODE_LOADFN(rp, &vi);
	if (rc)
		return (rc);
	vi->vino_stb.st_uid = uid;
	vi->vino_stb.st_gid = gid;
	VINODE_DECREF(vi);
	return (rc);
}

int
vesuvifs_open_common(const char *rp, mode_t mode,
    struct fuse_file_info *fi, int in_create)
{
	char fn[PATH_MAX];
	struct vinode *vi;
	struct vfh *vfh;
	struct stat stb;
	int fd, rc;

	mvfsthr_ensure();

	rc = translate_path(rp, fn);
	if (rc)
		return (rc);

	/* XXX adjust rlim */
	fd = open(fn, fi->flags, mode);
	if (fd == -1)
		return (-errno);
	if (fstat(fd, &stb) == -1) {
		rc = -errno;
		goto out;
	}
	if (in_create) {
		vi = VINODE_LOAD(stb.st_ino, &stb, VILF_CREATE);
		memcpy(&vi->vino_stb, &stb, sizeof(stb));
	} else {
		vi = VINODE_LOAD(stb.st_ino, NULL, 0);
		psc_assert(vi);
	}

	vfh = psc_pool_get(vfh_pool);
	memset(vfh, 0, sizeof(*vfh));
	INIT_LISTENTRY(&vfh->vfh_lentry);
	vfh->vfh_oflags = fi->flags;
	vfh->vfh_inode = vi;
	ffi_setvfh(fi, vfh);
//	if (fi->flags & O_TRUNC)
//		vesuvifs_truncate(vi, 0);
	fi->direct_io = 1;
//	fi->keep_cache = 1;
	VINODE_ULOCK(vi);

 out:
	close(fd);
	return (rc);
}

int
vesufsop_create(const char *rp, mode_t mode, struct fuse_file_info *fi)
{
	return (vesuvifs_open_common(rp, mode, fi, 1));
}

int
vesufsop_getattr(const char *rp, struct stat *stb)
{
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = VINODE_LOADFN(rp, &vi);
	if (rc)
		return (rc);
	*stb = vi->vino_stb;

	if (VINODE_ISROOT(vi)) {
		if (vesuvifs_root_id &&
		    vesuvifs_root_id != stb->st_ino)
			abort();
	}
	VINODE_DECREF(vi);
	return (rc);
}

int
vesufsop_fgetattr(__unusedx const char *path, struct stat *stb,
    struct fuse_file_info *fi)
{
	struct vinode *vi;
	struct vfh *vfh;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;
	VINODE_LOCK(vi);
	*stb = vi->vino_stb;
	VINODE_ULOCK(vi);
	return (0);
}

int
vesufsop_fsync(__unusedx const char *path, __unusedx int data,
    __unusedx struct fuse_file_info *fi)
{
	return (0);
}

int
vesufsop_flush(__unusedx const char *path,
    __unusedx struct fuse_file_info *fi)
{
	return (0);
}

void
vesuvifs_truncate(struct vinode *vi, off_t size)
{
	struct vslab *vs, *next, q;
	int pgoff;

	vi->vino_stb.st_size = size;
	q.vs_pageno = size / VPAGESIZE;
	vs = SPLAY_FIND(slabtree, &vi->vino_slabtree, &q);
	if (vs == NULL)
		return;

	pgoff = size % VPAGESIZE;

	if (pgoff) {
		/* zero truncated region in case file grows in future */
		memset(vs->vs_buf + pgoff, 0, VPAGESIZE - pgoff);

		vs = SPLAY_NEXT(slabtree, &vi->vino_slabtree, vs);
	}
	/* prune all full pages after offset */
	for (; vs; vs = next) {
		next = SPLAY_NEXT(slabtree, &vi->vino_slabtree, vs);
		SPLAY_REMOVE(slabtree, &vi->vino_slabtree, vs);
		INIT_LISTENTRY(&vs->vs_lentry);
//		psc_pool_return(slab_pool, vs);
		vi->vino_stb.st_blocks -= VBLOCKSPERPAGE;
		free(vs);
	}
}

int
vesufsop_ftruncate(__unusedx const char *path, off_t size,
    struct fuse_file_info *fi)
{
	struct vinode *vi;
	struct vfh *vfh;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;
	VINODE_LOCK(vi);
	vesuvifs_truncate(vi, size);
	VINODE_ULOCK(vi);
	return (0);
}

int
vesufsop_link(const char *rfrom, const char *rto)
{
	char tofn[PATH_MAX], fromfn[PATH_MAX];
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rfrom, fromfn);
	if (rc)
		return (rc);
	rc = translate_path(rto, tofn);
	if (rc)
		return (rc);
	if (link(fromfn, tofn) == -1)
		return (-errno);

	rc = VINODE_LOADXFN(tofn, &vi);
	if (rc)
		return (-rc);
	VINODE_DECREF(vi);
	return (0);
}

int
vesufsop_mkdir(const char *rp, mode_t mode)
{
	char fn[PATH_MAX];
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rp, fn);
	if (rc)
		return (rc);
	if (mkdir(fn, mode) == -1)
		return (-errno);

	rc = VINODE_LOADXFN(fn, &vi);
	if (rc)
		return (-rc);
	VINODE_DECREF(vi);
	return (0);
}

int
vesufsop_mknod(const char *rp, mode_t mode, dev_t dev)
{
	char fn[PATH_MAX];
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rp, fn);
	if (rc)
		return (rc);
	if (mknod(fn, mode, dev) == -1)
		return (-errno);

	rc = VINODE_LOADXFN(fn, &vi);
	if (rc)
		return (-rc);
	VINODE_DECREF(vi);
	return (0);
}

int
vesufsop_open(const char *rp, struct fuse_file_info *fi)
{
	return (vesuvifs_open_common(rp, 0, fi, 0));
}

int
vesufsop_opendir(const char *rp, struct fuse_file_info *fi)
{
	char fn[PATH_MAX];
	struct vinode *vi;
	struct vfh *vfh;
	struct stat stb;
	int fd, rc;

	mvfsthr_ensure();

	rc = translate_path(rp, fn);
	if (rc)
		return (rc);

	fd = open(fn, O_RDONLY | O_DIRECTORY);
	if (fd == -1)
		return (-errno);
	if (fstat(fd, &stb) == -1) {
		rc = -errno;
		goto out;
	}

	vi = VINODE_LOAD(stb.st_ino, &stb, VILF_CREATE);
	if (vi->vino_dirfd == -1) {
		vi->vino_dirfd = fd;
		memcpy(&vi->vino_stb, &stb, sizeof(stb));
	} else
		close(fd);
	VINODE_ULOCK(vi);

	vfh = psc_pool_get(vfh_pool);
	memset(vfh, 0, sizeof(*vfh));
	INIT_LISTENTRY(&vfh->vfh_lentry);
	vfh->vfh_inode = vi;
	vfh->vfh_oflags = fi->flags;
	ffi_setvfh(fi, vfh);

	if (psc_rlim_adj(RLIMIT_NOFILE, 1) == 0)
		vfh->vfh_decr_rlim_fd = 1;

 out:
	return (rc);
}

int
vesufsop_read(__unusedx const char *path, char *buf, size_t size,
    off_t offset, struct fuse_file_info *fi)
{
	size_t nb, pgoff, left, tbytesr;
	struct vinode *vi;
	struct vslab *vs;
	struct vfh *vfh;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;
	VINODE_LOCK(vi);

	if (offset >= vi->vino_stb.st_size) {
		VINODE_ULOCK(vi);
		return (0);
	}
	if ((off_t)size + offset > vi->vino_stb.st_size)
		size = vi->vino_stb.st_size - offset;
	left = size;

	pgoff = offset % VPAGESIZE;
	for (tbytesr = 0; left; left -= nb) {
		nb = MIN(left, VPAGESIZE - pgoff);

		DEBUG_VINODE(PLL_WARN, vi, "sz=%#zx off=%"PSCPRIxOFFT
		    " left=%#zx nb=%#zx pgoff=%#zx",
		    size, offset, left, nb, pgoff);

		vs = slab_get(vi, (offset + tbytesr) / VPAGESIZE);
		if (vs)
			memcpy(buf, vs->vs_buf + pgoff, nb);
		else
			memset(buf, 0, nb);
		buf += nb;
		tbytesr += nb;
		pgoff = 0;
	}

	VINODE_ULOCK(vi);
	return (size);
}

int
vesufsop_readdir(__unusedx const char *path, void *dbuf,
    fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	char buf[BUFSIZ];
	struct vinode *vi, *tvi;
	struct dirent *d;
	struct vfh *vfh;
	ssize_t nb, off;
	struct stat stb;
	int rc;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;

	rc = lseek(vi->vino_dirfd, offset, SEEK_SET);
	if (rc == -1)
		return (-errno);

	nb = getdirentries(vi->vino_dirfd, buf, sizeof(buf),
	    &offset);
	if (nb == -1)
		return (-errno);
	if (nb == 0)
		return (0);

	for (off = 0; off < nb; off += d->d_reclen) {
		d = (void *)(off + buf);

		if (d->d_reclen <= 0)
			return (-EINVAL);
		if (!d->d_fileno)
			continue;

		if (VINODE_ISROOT(vi) && !strcmp(d->d_name, "..")) {
			char *p, fn[PATH_MAX];

			if (stat(backfs, &stb) == -1)
				return (-errno);

			rc = filler(dbuf, "..", &stb, d->d_off);
		} else {
			tvi = VINODE_LOAD(d->d_ino, NULL, 0);
			psc_assert(tvi);
			rc = filler(dbuf, d->d_name, &tvi->vino_stb,
			    d->d_off);

			VINODE_DECREF(tvi);
		}

		if (rc)
			break;
	}
	return (0);
}

int
vesufsop_readlink(const char *rp, char *buf, size_t size)
{
	char fn[PATH_MAX];
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rp, fn);
	if (rc)
		return (rc);
	if (readlink(fn, buf, size) == -1)
		return (-errno);
	return (0);
}

int
vesufsop_release(__unusedx const char *path, struct fuse_file_info *fi)
{
	struct vinode *vi;
	struct vfh *vfh;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;
	VINODE_DECREF(vi);
	psc_pool_return(vfh_pool, vfh);
	return (0);
}

int
vesufsop_releasedir(__unusedx const char *path,
    struct fuse_file_info *fi)
{
	struct vinode *vi;
	struct vfh *vfh;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;
	VINODE_LOCK(vi);
	if (vi->vino_refcnt == 1) {
		close(vi->vino_dirfd);
		vi->vino_dirfd = -1;
	}
	VINODE_DECREF(vi);

	if (vfh->vfh_decr_rlim_fd)
		psc_rlim_adj(RLIMIT_NOFILE, -1);

	psc_pool_return(vfh_pool, vfh);
	return (0);
}

int
vesufsop_rename(const char *rfrom, const char *rto)
{
	char tofn[PATH_MAX], fromfn[PATH_MAX];
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rfrom, fromfn);
	if (rc)
		return (rc);
	rc = translate_path(rto, tofn);
	if (rc)
		return (rc);
	if (rename(fromfn, tofn) == -1)
		return (-errno);
	return (0);
}

int
vesufsop_rmdir(const char *rp)
{
	struct vinode *vi;
	char fn[PATH_MAX];
	int rc;

	mvfsthr_ensure();
	rc = VINODE_LOADFNBUF(rp, &vi, fn);
	if (rc)
		return (rc);

	if (rmdir(fn) == -1)
		rc = -errno;
	else
		vi->vino_flags |= VINODE_DELETED;
	VINODE_DECREF(vi);
	return (0);
}

int
vesufsop_statfs(__unusedx const char *rp, struct statvfs *sfb)
{
	mvfsthr_ensure();

	memset(sfb, 0, sizeof(*sfb));
	sfb->f_bsize = VPAGESIZE;
	sfb->f_namemax = NAME_MAX;
	sfb->f_frsize = VPAGESIZE;
//	sfb->f_blocks = VPAGESIZE * n_alloc_pages;
//	sfb->f_bfree = mem_per_node * nnodes / VPAGESIZE;
//	sfb->f_bavail = sfb->f_bfree;
//	unsigned long  f_fsid;     /* file system ID */
//	unsigned long  f_flag;     /* mount flags */
	return (0);
}

int
vesufsop_symlink(const char *rfrom, const char *rto)
{
	char tofn[PATH_MAX];
	struct stat stb;
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = translate_path(rto, tofn);
	if (rc)
		return (rc);
	if (symlink(rfrom, tofn) == -1)
		return (-errno);

	rc = lstat(tofn, &stb);
	if (rc)
		return (-errno);

	vi = VINODE_LOAD(stb.st_ino, &stb, VILF_CREATE);
	VINODE_DECREF(vi);
	return (rc);
}

int
vesufsop_truncate(const char *rp, off_t size)
{
	struct vinode *vi;
	int rc;

	mvfsthr_ensure();

	rc = VINODE_LOADFN(rp, &vi);
	if (vi) {
		vesuvifs_truncate(vi, size);
		VINODE_DECREF(vi);
	}
	return (rc);
}

int
vesufsop_unlink(const char *rp)
{
	struct vinode *vi;
	char fn[PATH_MAX];
	int rc;

	mvfsthr_ensure();

	rc = VINODE_LOADFNBUF(rp, &vi, fn);
	if (rc)
		return (rc);
	if (unlink(fn) == -1)
		rc = -errno;
	else
		vi->vino_stb.st_nlink--;
	VINODE_DECREF(vi);
	return (rc);
}

int
vesufsop_utimens(const char *rp, const struct timespec ts[2])
{
	struct vinode *vi;
	char fn[PATH_MAX];
	int rc;
	struct utimbuf utb = {ts[0].tv_sec, ts[1].tv_sec};

	mvfsthr_ensure();

	rc = VINODE_LOADFNBUF(rp, &vi, fn);
	if (vi) {
		PFL_STB_ATIME_SET(ts[0].tv_sec, ts[0].tv_nsec, &vi->vino_stb);
		PFL_STB_MTIME_SET(ts[1].tv_sec, ts[1].tv_nsec, &vi->vino_stb);
		VINODE_DECREF(vi);
	}

	return (utime(fn, &utb));
}

int
vesufsop_write(__unusedx const char *path, const char *buf, size_t size,
    off_t offset, struct fuse_file_info *fi)
{
	size_t left, nb, pgoff, tbytesw;
	struct timespec ts;
	struct vinode *vi;
	struct vslab *vs;
	struct vfh *vfh;

	mvfsthr_ensure();

	vfh = ffi_getvfh(fi);
	vi = vfh->vfh_inode;
	VINODE_LOCK(vi);

	pgoff = offset % VPAGESIZE;
	for (tbytesw = 0, left = size; left; left -= nb) {
		vs = slab_load(vi, (offset + tbytesw) / VPAGESIZE);
		tbytesw += nb = MIN(left, VPAGESIZE - pgoff);
		memcpy(vs->vs_buf + pgoff, buf, nb);
		buf += nb;
		pgoff = 0;
	}

	/* update file size */
	if ((off_t)size + offset > vi->vino_stb.st_size)
		vi->vino_stb.st_size = size + offset;

	PFL_GETTIMESPEC(&ts);
	PFL_STB_MTIME_SET(ts.tv_sec, ts.tv_nsec, &vi->vino_stb);
	VINODE_ULOCK(vi);
	return (size);
}

void
unmountfs(const char *mp)
{
	char buf[BUFSIZ];
	int rc;

	rc = snprintf(buf, sizeof(buf), "umount %s", mp);
	if (rc == -1)
		psc_fatal("snprintf: umount %s", mp);
	if (rc >= (int)sizeof(buf))
		psc_fatalx("snprintf: umount %s: too long", mp);
	if (system(buf) == -1)
		psc_warn("system(%s)", buf);
}

void
unmount_mp(void)
{
	unmountfs(mountpoint);
}

void *
vesufsop_init(__unusedx struct fuse_conn_info *conn)
{
	mvfsthr_ensure();

	/* Try to unmount the mountpoint if we die. */
	atexit(unmount_mp);

#define VINO_TBLSZ 4095
	psc_hashtbl_init(&vinode_hashtbl, 0, struct vinode, vino_inum,
	    vino_hentry, VINO_TBLSZ, NULL, "vinode");

	psc_poolmaster_init(&slab_pool_master, struct vslab, vs_lentry,
	    PPMF_AUTO | PPMF_NOLOCK, 8, 8, 0, NULL, NULL, NULL,
	    "slab");
	slab_pool = psc_poolmaster_getmgr(&slab_pool_master);

	psc_poolmaster_init(&vfh_pool_master, struct vfh, vfh_lentry,
	    PPMF_AUTO | PPMF_NOLOCK, 8, 8, 0, NULL, NULL, NULL,
	    "vfh");
	vfh_pool = psc_poolmaster_getmgr(&vfh_pool_master);

	psc_poolmaster_init(&ino_pool_master, struct vinode, vino_lentry,
	    PPMF_AUTO | PPMF_NOLOCK, 8, 8, 0, NULL, NULL, NULL,
	    "ino");
	ino_pool = psc_poolmaster_getmgr(&ino_pool_master);

	/* Start up service threads. */
	psc_tiosthr_spawn(MVTHRT_TIOS, "mvtiosthr");
	mvctlthr_spawn();

	return (NULL);
}

struct fuse_operations vesuops = {
	.access		= vesufsop_access,
	.chmod		= vesufsop_chmod,
	.chown		= vesufsop_chown,
	.create		= vesufsop_create,
	.fgetattr	= vesufsop_fgetattr,
	.flush		= vesufsop_flush,
	.fsync		= vesufsop_fsync,
	.ftruncate	= vesufsop_ftruncate,
	.getattr	= vesufsop_getattr,
	.init		= vesufsop_init,
	.link		= vesufsop_link,
	.mkdir		= vesufsop_mkdir,
	.mknod		= vesufsop_mknod,
	.open		= vesufsop_open,
	.opendir	= vesufsop_opendir,
	.read		= vesufsop_read,
	.readdir	= vesufsop_readdir,
	.readlink	= vesufsop_readlink,
	.release	= vesufsop_release,
	.releasedir	= vesufsop_releasedir,
	.rename		= vesufsop_rename,
	.rmdir		= vesufsop_rmdir,
	.statfs		= vesufsop_statfs,
	.symlink	= vesufsop_symlink,
	.truncate	= vesufsop_truncate,
	.unlink		= vesufsop_unlink,
	.utimens	= vesufsop_utimens,
	.write		= vesufsop_write
};

struct fuse_context *
psclog_get_fuse_context(void)
{
	return (fuse_get_context());
}

enum {
	MV_OPT_ALLOWDIRECT,
	MV_OPT_CTLSOCK,
	MV_OPT_UNMOUNT,
	MV_OPT_WAIT,
	MV_OPT_USAGE
};

struct fuse_opt mvopts[] = {
	FUSE_OPT_KEY("-S ", MV_OPT_CTLSOCK),
	FUSE_OPT_KEY("-U", MV_OPT_UNMOUNT),
	FUSE_OPT_KEY("-?", MV_OPT_USAGE),
	FUSE_OPT_END
};

__dead void
usage(void)
{
	char *argv[] = { (char *)progname, "-ho", NULL };

	fprintf(stderr,
	    "usage: %s [-U] [-S ctlsock] backfs node\n\n"
	    "VesuviFS options:\n"
	    "    -S ctlsock             specify alternate control socket\n"
	    "    -U                     umount(2) the mountpoint path before mounting\n\n",
	    progname);
	fuse_main(2, argv, &vesuops, NULL);
	exit(1);
}

int
proc_opt(__unusedx void *data, const char *arg, int c,
    __unusedx struct fuse_args *outargs)
{
	static int fuse_optind;

	switch (c) {
	case FUSE_OPT_KEY_OPT:
		return (1);
	case FUSE_OPT_KEY_NONOPT:
		switch (fuse_optind++) {
		case 0:
			if (strlcpy(backfs, arg, sizeof(backfs)) >=
			    sizeof(backfs)) {
				errno = ENAMETOOLONG;
				err(1, "backfs");
			}
			break;
		case 1:
			noncanon_mp = arg;
			return (1);
		default:
			usage();
		}
		break;
	case MV_OPT_UNMOUNT:
		unmount_first = 1;
		break;
	case MV_OPT_CTLSOCK:
		strlcpy(ctlsockfn, arg + 2, PATH_MAX);
		break;
	default:
		usage();
	}
	return (0);
}

int
main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct stat stb;
	int ents = 0;
	DIR *dir;

	progname = argv[0];

	pfl_init();

	pscthr_init(MVTHRT_FSMGR, 0, NULL, NULL, 0, "mvfsmgrthr");

	if (fuse_opt_parse(&args, NULL, mvopts, proc_opt))
		usage();

	if (fuse_opt_insert_arg(&args, 1, "-oallow_other") == -1)
		psc_fatal("fuse_opt_insert_arg");
	if (fuse_opt_insert_arg(&args, 1, "-odirect_io") == -1)
		psc_fatal("fuse_opt_insert_arg");
	if (fuse_opt_insert_arg(&args, 1, "-f") == -1)
		psc_fatal("fuse_opt_insert_arg");
	if (unmount_first)
		unmountfs(noncanon_mp);
	if (realpath(noncanon_mp, mountpoint) == NULL)
		psc_fatal("realpath %s", noncanon_mp);
	if (access(backfs, X_OK))
		err(1, "access");

	if (stat(backfs, &stb) == -1)
		psc_fatal("stat of backfs failed");

	if (!S_ISDIR(stb.st_mode)) {
		errno = ENOTDIR;
		psc_fatal("%s", backfs);
	}
	vesuvifs_root_id = stb.st_ino;

	dir = opendir(backfs);
	if (!dir)
		psc_fatal("%s", backfs);

	while (readdir(dir))
		ents++;
	if (ents != 2) {
		errno = ENOTEMPTY;
		psc_fatal("%s", backfs);
	}
	closedir(dir);

	return (fuse_main(args.argc, args.argv, &vesuops, NULL));
}
