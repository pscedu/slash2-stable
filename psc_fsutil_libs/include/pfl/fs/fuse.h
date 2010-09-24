/* $Id$ */



#define PSCFS_FUSE_OPS							\
	pscfs_fuse_getcred,						\
	pscfs_fuse_getumask,						\
									\
	pscfs_fuse_reply_access,					\
	pscfs_fuse_reply_close,						\
	pscfs_fuse_reply_closedir,					\
	pscfs_fuse_reply_create,					\
	pscfs_fuse_reply_flush,						\
	pscfs_fuse_reply_fsync,						\
	pscfs_fuse_reply_fsyncdir,					\
	pscfs_fuse_reply_getattr,					\
	pscfs_fuse_reply_ioctl,						\
	pscfs_fuse_reply_link,						\
	pscfs_fuse_reply_lookup,					\
	pscfs_fuse_reply_mkdir,						\
	pscfs_fuse_reply_mknod,						\
	pscfs_fuse_reply_open,						\
	pscfs_fuse_reply_opendir,					\
	pscfs_fuse_reply_read,						\
	pscfs_fuse_reply_readdir,					\
	pscfs_fuse_reply_readlink,					\
	pscfs_fuse_reply_rename,					\
	pscfs_fuse_reply_rmdir,						\
	pscfs_fuse_reply_setattr,					\
	pscfs_fuse_reply_statfs,					\
	pscfs_fuse_reply_symlink,					\
	pscfs_fuse_reply_umount,					\
	pscfs_fuse_reply_unlink,					\
	pscfs_fuse_reply_write

