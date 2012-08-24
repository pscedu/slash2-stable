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

#ifndef _PFL_FSMOD_H_
#define _PFL_FSMOD_H_

#include "pfl/fs.h"
#include "psc_ds/list.h"

#ifdef HAVE_FUSE
#  include <fuse_lowlevel.h>

#  define pscfs_reply_link		pscfs_fuse_replygen_entry
#  define pscfs_reply_lookup		pscfs_fuse_replygen_entry
#  define pscfs_reply_mkdir		pscfs_fuse_replygen_entry
#  define pscfs_reply_mknod		pscfs_fuse_replygen_entry
#  define pscfs_reply_symlink		pscfs_fuse_replygen_entry

struct pscfs_args {
	struct fuse_args		 pfa_av;
};

struct pscfs_req {
	fuse_req_t			 pfr_fuse_req;
	struct fuse_file_info		*pfr_fuse_fi;
	struct pscfs_clientctx		 pfr_clientctx;
	struct psc_listentry		 pfr_lentry;
	char				*pfr_buf;
	void				*pfr_info;		/* points to struct msl_fsrqinfo */
};

#  define PSCFS_ARGS_INIT(n, av)	{ FUSE_ARGS_INIT((n), (av)) }

void	pscfs_fuse_replygen_entry(struct pscfs_req *, pscfs_inum_t,
	    pscfs_fgen_t, double, const struct stat *, double, int);

extern double pscfs_entry_timeout;
extern double pscfs_attr_timeout;

#elif defined(HAVE_DOKAN)
#else
#  error no filesystem in userspace API available
#endif

#endif /* _PFL_FSMOD_H_ */
