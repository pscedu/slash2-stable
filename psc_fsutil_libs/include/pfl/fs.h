/* $Id$ */

typedef uint64_t pscfs_inum_t;

struct pscfs_req {
	union {
		fuse_req_t req;
	};
};

struct pscfs_fhinfo {
	union {
		struct fuse_file_info finfo;
	};
};

struct pscfs_cred {
	uint32_t	pfc_uid;
	uint32_t	pfc_gid;
};

#define PSCFS_SETATTRF_MODE	(1 << 0)
#define	PSCFS_SETATTRF_UID	(1 << 1)
#define	PSCFS_SETATTRF_GID	(1 << 2)
#define	PSCFS_SETATTRF_SIZE	(1 << 3)
#define	PSCFS_SETATTRF_ATIME	(1 << 4)
#define	PSCFS_SETATTRF_MTIME	(1 << 5)

struct pscfs {
	void	(*pf_getcred)(struct pscfs_req *, struct pscfs_cred *);
	mode_t	(*pf_getumask)(struct pscfs_req *);

	void	(*pf_reply_access)(struct pscfs_req *, pscfs_inum_t, int);
	void	(*pf_reply_close)(struct pscfs_req *, pscfs_inum_t, struct pscfs_fhinfo *);
	void	(*pf_reply_closedir)(struct pscfs_req *, pscfs_inum_t, struct pscfs_fhinfo *);
	void	(*pf_reply_create)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t, struct pscfs_fhinfo *);
	void	(*pf_reply_flush)(struct pscfs_req *, pscfs_inum_t, struct pscfs_fhinfo *);
	void	(*pf_reply_fsync)(struct pscfs_req *, pscfs_inum_t, int, struct pscfs_fhinfo *);
	void	(*pf_reply_fsyncdir)(struct pscfs_req *, pscfs_inum_t, int, struct pscfs_fhinfo *);
	void	(*pf_reply_getattr)(struct pscfs_req *, pscfs_inum_t, struct pscfs_fhinfo *);
	void	(*pf_reply_ioctl)(struct pscfs_req *);
	void	(*pf_reply_link)(struct pscfs_req *, pscfs_inum_t, pscfs_inum_t, const char *);
	void	(*pf_reply_lookup)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_reply_mkdir)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t);
	void	(*pf_reply_mknod)(struct pscfs_req *, pscfs_inum_t, const char *, mode_t, dev_t);
	void	(*pf_reply_open)(struct pscfs_req *, pscfs_inum_t, struct pscfs_fhinfo *);
	void	(*pf_reply_opendir)(struct pscfs_req *, pscfs_inum_t, struct pscfs_fhinfo *);
	void	(*pf_reply_read)(struct pscfs_req *, size_t, off_t, struct pscfs_fhinfo *);
	void	(*pf_reply_readdir)(struct pscfs_req *, pscfs_inum_t, size_t, off_t, struct pscfs_fhinfo *);
	void	(*pf_reply_readlink)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_reply_rename)(struct pscfs_req *, pscfs_inum_t, const char *, pscfs_inum_t, const char *);
	void	(*pf_reply_rmdir)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_reply_setattr)(struct pscfs_req *, pscfs_inum_t, struct stat *, int, struct pscfs_fhinfo *);
	void	(*pf_reply_statfs)(struct pscfs_req *, pscfs_inum_t);
	void	(*pf_reply_symlink)(struct pscfs_req *, const char *, pscfs_inum_t, const char *);
	void	(*pf_reply_unlink)(struct pscfs_req *, pscfs_inum_t, const char *);
	void	(*pf_reply_write)(struct pscfs_req *, const void *, size_t, off_t, struct pscfs_fhinfo *);

	void	(*pf_handle_access)(struct pscfs_req *, int);
	void	(*pf_handle_close)(struct pscfs_req *, int);
	void	(*pf_handle_closedir)(struct pscfs_req *, int);
	void	(*pf_handle_create)(struct pscfs_req *, struct stat *, struct pscfs_fhinfo *, int);
	void	(*pf_handle_flush)(struct pscfs_req *);
	void	(*pf_handle_fsync)(struct pscfs_req *);
	void	(*pf_handle_fsyncdir)(struct pscfs_req *);
	void	(*pf_handle_getattr)(struct pscfs_req *);
	void	(*pf_handle_init)(struct pscfs_req *);
	void	(*pf_handle_ioctl)(struct pscfs_req *);
	void	(*pf_handle_link)(struct pscfs_req *);
	void	(*pf_handle_lookup)(struct pscfs_req *);
	void	(*pf_handle_mkdir)(struct pscfs_req *);
	void	(*pf_handle_mknod)(struct pscfs_req *);
	void	(*pf_handle_open)(struct pscfs_req *);
	void	(*pf_handle_opendir)(struct pscfs_req *);
	void	(*pf_handle_read)(struct pscfs_req *, void *, int);
	void	(*pf_handle_readdir)(struct pscfs_req *, void *, int);
	void	(*pf_handle_readlink)(struct pscfs_req *);
	void	(*pf_handle_rename)(struct pscfs_req *);
	void	(*pf_handle_rmdir)(struct pscfs_req *);
	void	(*pf_handle_setattr)(struct pscfs_req *);
	void	(*pf_handle_statfs)(struct pscfs_req *);
	void	(*pf_handle_symlink)(struct pscfs_req *);
	void	(*pf_handle_umount)(struct pscfs_req *);
	void	(*pf_handle_unlink)(struct pscfs_req *);
	void	(*pf_handle_write)(struct pscfs_req *);
};

#ifdef HAVE_FUSE
#  include "pfl/fs/fuse.h"
#  define PSCFS_OPS PSCFS_FUSE_OPS
#elif defined(HAVE_NNPFS)
#  include "pfl/fs/nnpfs.h"
#  define PSCFS_OPS PSCFS_NNPFS_OPS
#elif defined(HAVE_DOKAN)
#  include "pfl/fs/dokan.h"
#  define PSCFS_OPS PSCFS_DOKAN_OPS
#else
#  error no filesystem in userspace API available
#endif

extern struct pscfs pscfs;

#endif /* _PFL_FS_H_ */
