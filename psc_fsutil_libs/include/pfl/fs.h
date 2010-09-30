/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_FS_H_
#define _PFL_FS_H_

struct stat;
struct statvfs;

struct pscfs_req;

typedef uint64_t pscfs_inum_t;
typedef uint64_t pscfs_fgen_t;

struct pscfs_cred {
	uint32_t	pfc_uid;
	uint32_t	pfc_gid;
};

/* user fills these in */
struct pscfs {
	void	(*pf_handle_access)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_handle_close)(struct pscfs_req *, pscfs_inum_t, void *);
	void	(*pf_handle_closedir)(struct pscfs_req *, pscfs_inum_t, void * *);
	void	(*pf_handle_create)(struct pscfs_req *, pscfs_inum_t, const char *, int, mode_t);
	void	(*pf_handle_flush)(struct pscfs_req *, pscfs_inum_t, void *);
	void	(*pf_handle_fsync)(struct pscfs_req *, pscfs_inum_t, int, void *);
	void	(*pf_handle_fsyncdir)(struct pscfs_req *, pscfs_inum_t, int, void *);
	void	(*pf_handle_getattr)(struct pscfs_req *, pscfs_inum_t, void *);
	void	(*pf_handle_ioctl)(struct pscfs_req *);
	void	(*pf_handle_link)(struct pscfs_req *, pscfs_inum_t, pscfs_inum_t, const char *);
	void	(*pf_handle_lookup)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_mkdir)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t);
	void	(*pf_handle_mknod)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t, dev_t);
	void	(*pf_handle_open)(struct pscfs_req *, pscfs_inum_t, void *);
	void	(*pf_handle_opendir)(struct pscfs_req *, pscfs_inum_t, void *);
	void	(*pf_handle_read)(struct pscfs_req *, size_t, off_t, void *);
	void	(*pf_handle_readdir)(struct pscfs_req *, pscfs_inum_t, size_t, off_t, void *);
	void	(*pf_handle_readlink)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_rename)(struct pscfs_req *, pscfs_inum_t, const char *, pscfs_inum_t, const char *);
	void	(*pf_handle_rmdir)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_setattr)(struct pscfs_req *, pscfs_inum_t, struct stat *, int, void *);
	void	(*pf_handle_statfs)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_handle_symlink)(struct pscfs_req *, const char *, pscfs_inum_t, const char *);
	void	(*pf_handle_unlink)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_handle_umount)(struct pscfs_req *);
	void	(*pf_handle_write)(struct pscfs_req *, const void *, size_t, off_t, void *);
};

void	pscfs_addarg(struct pscfs_args *, const char *);
void	pscfs_freeargs(struct pscfs_args *);
void	pscfs_mount(const char *, struct pscfs_args *);

void	pscfs_getcreds(struct pscfs_req *, struct pscfs_cred *);
mode_t	pscfs_getumask(struct pscfs_req *);

int	pscfs_setdebug(int);
int	pscfs_getdebug(int *);

void	pscfs_mount(const char *, struct pscfs_args *);

void	pscfs_reply_access(struct pscfs_req *, int);
void	pscfs_reply_close(struct pscfs_req *, int);
void	pscfs_reply_closedir(struct pscfs_req *, int);
void	pscfs_reply_create(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, struct stat *, int, void *, int);
void	pscfs_reply_flush(struct pscfs_req *, int);
void	pscfs_reply_fsync(struct pscfs_req *, int);
void	pscfs_reply_fsyncdir(struct pscfs_req *, int);
void	pscfs_reply_getattr(struct pscfs_req *, struct stat *, int, int);
void	pscfs_reply_init(struct pscfs_req *);
void	pscfs_reply_ioctl(struct pscfs_req *);
void	pscfs_reply_link(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, struct stat *, int, int);
void	pscfs_reply_lookup(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, struct stat *, int, int);
void	pscfs_reply_mkdir(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, struct stat *, int, int);
void	pscfs_reply_mknod(struct pscfs_req *, struct stat *, int);
void	pscfs_reply_open(struct pscfs_req *, void *, int);
void	pscfs_reply_opendir(struct pscfs_req *, void *, int);
void	pscfs_reply_read(struct pscfs_req *, void *, ssize_t, int);
void	pscfs_reply_readdir(struct pscfs_req *, void *, ssize_t, int);
void	pscfs_reply_readlink(struct pscfs_req *, void *, int);
void	pscfs_reply_rename(struct pscfs_req *, int);
void	pscfs_reply_rmdir(struct pscfs_req *, int);
void	pscfs_reply_setattr(struct pscfs_req *, struct stat *, int, int);
void	pscfs_reply_statfs(struct pscfs_req *, struct statvfs *, int);
void	pscfs_reply_symlink(struct pscfs_req *, struct stat *, int);
void	pscfs_reply_umount(struct pscfs_req *);
void	pscfs_reply_unlink(struct pscfs_req *, int);
void	pscfs_reply_write(struct pscfs_req *, ssize_t, int);

#define PSCFS_CREATEF_DIO	(1 << 0)

#define PSCFS_OPENF_DIO		(1 << 0)
#define PSCFS_OPENF_KEEPCACHE	(1 << 1)

#define PSCFS_SETATTRF_MODE	(1 << 0)
#define	PSCFS_SETATTRF_UID	(1 << 1)
#define	PSCFS_SETATTRF_GID	(1 << 2)
#define	PSCFS_SETATTRF_SIZE	(1 << 3)
#define	PSCFS_SETATTRF_ATIME	(1 << 4)
#define	PSCFS_SETATTRF_MTIME	(1 << 5)

#ifdef HAVE_FUSE
#  define pscfs_reply_link	pscfs_fuse_replygen_entry
#  define pscfs_reply_lookup	pscfs_fuse_replygen_entry
#  define pscfs_reply_mkdir	pscfs_fuse_replygen_entry
#  define pscfs_reply_symlink	pscfs_fuse_replygen_entry

struct pscfs_args {
	struct fuse_args av;
};

struct pscfs_req {
	fuse_req_t		 pfr_fuse_req;
	struct fuse_file_info	*pfr_fuse_fi;
};

#define PSCFS_ARGS_INIT(n, av)	{ FUSE_ARGS_INIT((n), (av)) }

void	pscfs_replygen_entry(struct pscfs_req *, pscfs_inum_t,
	    pscfs_fgen_t, int, struct stat *, int, int);

#elif defined(HAVE_NNPFS)
#elif defined(HAVE_DOKAN)
#else
#  error no filesystem in userspace API available
#endif

extern struct pscfs pscfs;

#endif /* _PFL_FS_H_ */
