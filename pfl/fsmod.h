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

#ifndef _PFL_FSMOD_H_
#define _PFL_FSMOD_H_

#include "pfl/fs.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/time.h"

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

#  define PSCFS_ARGS_INIT(n, av)	{ FUSE_ARGS_INIT((n), (av)) }

void	pscfs_fuse_replygen_entry(struct pscfs_req *, pscfs_inum_t,
	    pscfs_fgen_t, double, const struct stat *, double, int);

#elif defined(HAVE_DOKAN)
#else
#  error no filesystem in userspace API available
#endif

#endif /* _PFL_FSMOD_H_ */
