/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2010-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_FS_H_
#define _PFL_FS_H_

#include <sys/param.h>

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#include "pfl/dynarray.h"
#include "pfl/list.h"
#include "pfl/multiwait.h"

struct iovec;
struct stat;
struct statvfs;
struct timespec;

struct psc_thread;

struct pscfs_args;
struct pscfs_req;

struct pflfs_filehandle;

struct psc_ctlmsghdr;
struct psc_ctlmsg_param;
struct psc_ctlop;

typedef uint64_t pscfs_inum_t;
typedef uint64_t pscfs_fgen_t;

#define	PSCFS_ATTR_TIMEOUT	10
#define	PSCFS_ENTRY_TIMEOUT	10

struct pscfs_creds {
	uid_t			pcr_uid;
	gid_t			pcr_gid;
	gid_t			pcr_gidv[NGROUPS_MAX];
	int			pcr_ngid;
};

struct pscfs_dirent {
	uint64_t		pfd_ino;
	uint64_t		pfd_off;
	uint32_t		pfd_namelen;
	uint32_t		pfd_type;
	char			pfd_name[0];
};

#define PFL_DIRENT_NAME_OFFSET	offsetof(struct pscfs_dirent, pfd_name)
#define PFL_DIRENT_ALIGN(x)	(((x) + sizeof(uint64_t) - 1) &		\
				    ~(sizeof(uint64_t) - 1))
#define PFL_DIRENT_SIZE(len)	PFL_DIRENT_ALIGN(			\
				    PFL_DIRENT_NAME_OFFSET + (len))

/* userland file system fills these in */
struct pscfs {
	struct pfl_opstat	*pf_opst_read_err;
	struct pfl_opstat	*pf_opst_read_reply;
	struct pfl_opstat	*pf_opst_write_reply;
	struct pfl_opstat	*pf_opst_write_err;
	void			*pf_private;
	struct psc_dynarray	 pf_opts;

	void			 (*pf_filehandle_freeze)(struct pflfs_filehandle *);
	void			 (*pf_filehandle_thaw)(struct pflfs_filehandle *);

	void			*(*pf_thr_init)(struct psc_thread *);
	void			 (*pf_thr_destroy)(void *);

	const char		*pf_name;

	void	(*pf_handle_access)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_release)(struct pscfs_req *, void *);
	void	(*pf_handle_releasedir)(struct pscfs_req *, void *);
	void	(*pf_handle_create)(struct pscfs_req *, pscfs_inum_t, const char *, int, mode_t);
	void	(*pf_handle_flush)(struct pscfs_req *, void *);
	void	(*pf_handle_fsync)(struct pscfs_req *, int, void *);
	void	(*pf_handle_fsyncdir)(struct pscfs_req *, int, void *);
	void	(*pf_handle_getattr)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_ioctl)(struct pscfs_req *);
	void	(*pf_handle_link)(struct pscfs_req *, pscfs_inum_t, pscfs_inum_t, const char *);
	void	(*pf_handle_lookup)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_mkdir)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t);
	void	(*pf_handle_mknod)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t, dev_t);
	void	(*pf_handle_open)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_opendir)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_read)(struct pscfs_req *, size_t, off_t, void *);
	void	(*pf_handle_readdir)(struct pscfs_req *, size_t, off_t, void *);
	void	(*pf_handle_readlink)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_rename)(struct pscfs_req *, pscfs_inum_t, const char *, pscfs_inum_t, const char *);
	void	(*pf_handle_rmdir)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_setattr)(struct pscfs_req *, pscfs_inum_t, struct stat *, int, void *);
	void	(*pf_handle_statfs)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_symlink)(struct pscfs_req *, const char *, pscfs_inum_t, const char *);
	void	(*pf_handle_unlink)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_destroy)(struct pscfs_req *);
	void	(*pf_handle_write)(struct pscfs_req *, const void *, size_t, off_t, void *);
	void	(*pf_handle_listxattr)(struct pscfs_req *, size_t, pscfs_inum_t);
	void	(*pf_handle_getxattr)(struct pscfs_req *, const char *, size_t, pscfs_inum_t);
	void	(*pf_handle_setxattr)(struct pscfs_req *, const char *, const void *, size_t, pscfs_inum_t);
	void	(*pf_handle_removexattr)(struct pscfs_req *, const char *, pscfs_inum_t);
};

#define PSCFS_INIT							\
/* opst_read_err */	NULL,						\
/* opst_read_reply */	NULL,						\
/* opst_write_reply */	NULL,						\
/* opst_write_err */	NULL,						\
/* private */		NULL,						\
/* opts */		DYNARRAY_INIT,					\
/* filehandle_freeze */	NULL,						\
/* filehandle_thaw */	NULL,						\
/* thr_init */		NULL,						\
/* thr_destroy */	NULL

struct pscfs_clientctx {
	pid_t		pfcc_pid;
};

struct pflfs_filehandle {
	struct psc_listentry		 pfh_lentry;
	void				*pfh_ufsi_fhdata; // userland FS interface
	void				*pfh_mod_data;	// XXX per-module private data
};

struct pscfs_req {
	struct pscfs			*pfr_mod;
	struct psc_spinlock		 pfr_lock;
	void				*pfr_ufsi_req;	// userland FS interface
	void				*pfr_ufsi_fhdata; // userland FS interface
	struct pscfs_clientctx		 pfr_clientctx;
	struct psc_listentry		 pfr_lentry;
	struct timespec			 pfr_start;
	int				 pfr_retries;
	int				 pfr_interrupted; // XXX flags
	struct psc_thread		*pfr_thread;
	int				 pfr_refcnt;
	int				 pfr_rc;
	const char			*pfr_opname;
};

struct pfl_fsthr {
	struct pscfs_req		*pft_pfr;
	char				 pft_uprog[128];
	void				*pft_private;	// XXX make per-module
};

void	pscfs_addarg(struct pscfs_args *, const char *);
void	pscfs_freeargs(struct pscfs_args *);

struct pscfs_clientctx *
	pscfs_getclientctx(struct pscfs_req *);
void	pscfs_getcreds(struct pscfs_req *, struct pscfs_creds *);
int	pscfs_getgroups(struct pscfs_req *, gid_t *, int *);
mode_t	pscfs_getumask(struct pscfs_req *);

int	pscfs_setdebug(int);
int	pscfs_getdebug(int *);

int	pscfs_main(int, const char *);
void	pscfs_mount(const char *, struct pscfs_args *);

void	pscfs_reply_access(struct pscfs_req *, int);
void	pscfs_reply_release(struct pscfs_req *, int);
void	pscfs_reply_releasedir(struct pscfs_req *, int);
void	pscfs_reply_create(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, double, const struct stat *, double, void *, int, int);
void	pscfs_reply_flush(struct pscfs_req *, int);
void	pscfs_reply_fsync(struct pscfs_req *, int);
void	pscfs_reply_fsyncdir(struct pscfs_req *, int);
void	pscfs_reply_getattr(struct pscfs_req *, struct stat *, double, int);
void	pscfs_reply_init(struct pscfs_req *);
void	pscfs_reply_ioctl(struct pscfs_req *);
void	pscfs_reply_link(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_lookup(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_mkdir(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_mknod(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, double, const struct stat *, double, int);
void	pscfs_reply_open(struct pscfs_req *, void *, int, int);
void	pscfs_reply_opendir(struct pscfs_req *, void *, int, int);
void	pscfs_reply_read(struct pscfs_req *, struct iovec *, int, int);
void	pscfs_reply_readdir(struct pscfs_req *, void *, ssize_t, int);
void	pscfs_reply_readlink(struct pscfs_req *, void *, int);
void	pscfs_reply_rename(struct pscfs_req *, int);
void	pscfs_reply_rmdir(struct pscfs_req *, int);
void	pscfs_reply_setattr(struct pscfs_req *, struct stat *, double, int);
void	pscfs_reply_statfs(struct pscfs_req *, const struct statvfs *, int);
void	pscfs_reply_symlink(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, const struct stat *, int, int);
void	pscfs_reply_umount(struct pscfs_req *);
void	pscfs_reply_unlink(struct pscfs_req *, int);
void	pscfs_reply_write(struct pscfs_req *, ssize_t, int);

void	pscfs_reply_listxattr(struct pscfs_req *, void *, size_t, int);
void	pscfs_reply_setxattr(struct pscfs_req *, int);
void	pscfs_reply_getxattr(struct pscfs_req *, void *, size_t, int);
void	pscfs_reply_removexattr(struct pscfs_req *, int);

void	*pflfs_inval_getprivate(struct pscfs_req *);
int	pflfs_inval_inode(void *, pscfs_inum_t);
int	pscfs_notify_inval_entry(void *, pscfs_inum_t, const char *, size_t);

#define PSCFS_CREATEF_DIO		(1 << 0)

#define PSCFS_OPENF_DIO			(1 << 0)
#define PSCFS_OPENF_KEEPCACHE		(1 << 1)

/* setattr() to_set mask flags */
#define PSCFS_SETATTRF_MODE		(1 <<  0)	/* chmod */
#define	PSCFS_SETATTRF_UID		(1 <<  1)	/* chown */
#define	PSCFS_SETATTRF_GID		(1 <<  2)	/* chgrp */
#define	PSCFS_SETATTRF_DATASIZE		(1 <<  3)	/* file data truncate */
#define	PSCFS_SETATTRF_ATIME		(1 <<  4)	/* utimes */
#define	PSCFS_SETATTRF_MTIME		(1 <<  5)	/* utimes */
#define	PSCFS_SETATTRF_CTIME		(1 <<  6)	/* utimes */
#define	PSCFS_SETATTRF_ATIME_NOW	(1 <<  7)	/* utimes */
#define	PSCFS_SETATTRF_MTIME_NOW	(1 <<  8)	/* utimes */
#define	_PSCFS_SETATTRF_LAST		(1 <<  9)

#define PSCFS_SETATTRF_ALL		(~0)

int	pscfs_ctlparam(int, struct psc_ctlmsghdr *,
	    struct psc_ctlmsg_param *, char **, int);

#define PFLFS_MOD_POS_LAST		(-1)

void	pflfs_module_add(int, struct pscfs *);
struct pscfs *
	pflfs_module_remove(int);

void	pflfs_module_destroy(struct pscfs *);
void	pflfs_module_init(struct pscfs *, const char *);

void	pflfs_modules_rdpin(void);
void	pflfs_modules_rdunpin(void);
void	pflfs_modules_wrpin(void);
void	pflfs_modules_wrunpin(void);

void	_pflfs_module_init_threads(struct pscfs *);

void	*pfl_fsthr_getpri(struct psc_thread *);
void	 pfl_fsthr_setpri(struct psc_thread *, void *);

void	*pflfs_req_getfh(struct pscfs_req *);
int	 pflfs_req_multiwait_rel(struct pscfs_req *, void *,
	    const struct timespec *);
int	 pflfs_req_sleep_rel(struct pscfs_req *,
	    const struct timespec *);

void	 pflfs_register_ctlops(struct psc_ctlop *);

extern struct psc_dynarray		pscfs_modules;
extern struct psc_lockedlist		pflfs_filehandles;
extern struct psc_lockedlist		pflfs_requests;

extern double				pscfs_entry_timeout;
extern double				pscfs_attr_timeout;

#endif /* _PFL_FS_H_ */
