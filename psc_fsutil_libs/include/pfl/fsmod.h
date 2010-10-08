/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_FSMOD_H_
#define _PFL_FSMOD_H_

#ifdef HAVE_FUSE
#  include <fuse_lowlevel.h>

#  define pscfs_reply_link		pscfs_fuse_replygen_entry
#  define pscfs_reply_lookup		pscfs_fuse_replygen_entry
#  define pscfs_reply_mkdir		pscfs_fuse_replygen_entry
#  define pscfs_reply_symlink		pscfs_fuse_replygen_entry

struct pscfs_args {
	struct fuse_args		 pfa_av;
};

struct pscfs_req {
	fuse_req_t			 pfr_fuse_req;
	struct fuse_file_info		*pfr_fuse_fi;
};

#  define PSCFS_ARGS_INIT(n, av)	{ FUSE_ARGS_INIT((n), (av)) }

void	pscfs_fuse_replygen_entry(struct pscfs_req *, pscfs_inum_t,
	    pscfs_fgen_t, int, const struct stat *, int, int);

#elif defined(HAVE_NNPFS)
#elif defined(HAVE_DOKAN)
#else
#  error no filesystem in userspace API available
#endif

#endif /* _PFL_FSMOD_H_ */
