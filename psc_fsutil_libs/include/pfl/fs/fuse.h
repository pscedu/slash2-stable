/* $Id$ */

#define PSCFS_FUSE_OPS							\
		pscfs_fuse_getcred,					\
		pscfs_fuse_getumask,					\
									\
		pscfs_fuse_reply_access,				\
		pscfs_fuse_reply_close,					\
		pscfs_fuse_reply_closedir,				\
		pscfs_fuse_reply_create,				\
		pscfs_fuse_reply_flush,					\
		pscfs_fuse_reply_fsync,					\
		pscfs_fuse_reply_fsyncdir,				\
		pscfs_fuse_reply_getattr,				\
		pscfs_fuse_reply_ioctl,					\
/* link */	pscfs_fuse_replygen_entry,				\
/* lookup */	pscfs_fuse_replygen_entry,				\
/* mkdir */	pscfs_fuse_replygen_entry,				\
		pscfs_fuse_reply_mknod,					\
		pscfs_fuse_reply_open,					\
		pscfs_fuse_reply_opendir,				\
		pscfs_fuse_reply_read,					\
		pscfs_fuse_reply_readdir,				\
		pscfs_fuse_reply_readlink,				\
		pscfs_fuse_reply_rename,				\
		pscfs_fuse_reply_rmdir,					\
		pscfs_fuse_reply_setattr,				\
		pscfs_fuse_reply_statfs,				\
/* symlink */	pscfs_fuse_replygen_entry,				\
		pscfs_fuse_reply_umount,				\
		pscfs_fuse_reply_unlink,				\
		pscfs_fuse_reply_write

void pscfs_fuse_reply_access(struct pscfs_req *, int);
void pscfs_fuse_reply_close(struct pscfs_req *, int);
void pscfs_fuse_reply_closedir(struct pscfs_req *, int);
void pscfs_fuse_reply_create(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, struct stat *, int, void *, int);;
void pscfs_fuse_reply_flush(struct pscfs_req *, int);
void pscfs_fuse_reply_fsync(struct pscfs_req *, int);
void pscfs_fuse_reply_fsyncdir(struct pscfs_req *, int);
void pscfs_fuse_reply_getattr(struct pscfs_req *, struct stat *, int, int);
void pscfs_fuse_reply_ioctl(struct pscfs_req *);
void pscfs_fuse_reply_mknod(struct pscfs_req *);
void pscfs_fuse_reply_open(struct pscfs_req *, void *, int);
void pscfs_fuse_reply_opendir(struct pscfs_req *, void *, int);
void pscfs_fuse_reply_read(struct pscfs_req *, void *, ssize_t, int)
void pscfs_fuse_reply_readdir(struct pscfs_req *, void *, ssize_t, int);
void pscfs_fuse_reply_readlink(struct pscfs_req *, void *, int);
void pscfs_fuse_reply_rename(struct pscfs_req *, int)
void pscfs_fuse_reply_rmdir(struct pscfs_req *)
void pscfs_fuse_reply_setattr(struct pscfs_req *, struct stat *, int, int);
void pscfs_fuse_reply_statfs(struct pscfs_req *, struct statvfs *, int);
void pscfs_fuse_reply_umount(struct pscfs_req *);
void pscfs_fuse_reply_unlink(struct pscfs_req *, int);
void pscfs_fuse_reply_write(struct pscfs_req *, ssize_t, int);

void pscfs_fuse_replygen_entry(struct pscfs_req *, pscfs_inum_t, pscfs_fgen_t, int, struct stat *, int, int);
