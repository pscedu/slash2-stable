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

#ifndef _MOUNT_GLAZE_H_
#define _MOUNT_GLAZE_H_

#include <sys/types.h>
#include <sys/stat.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/tree.h"
#include "psc_util/lock.h"
#include "psc_util/thread.h"

#include "slab.h"

struct vslab;

/* mount_glaze thread types */
enum {
	MGTHRT_CTL,			/* control processor */
	MGTHRT_CTLAC,			/* control acceptor */
	MGTHRT_FS,			/* file system syscall handler workers */
	MGTHRT_FSMGR,			/* pscfs manager */
	MGTHRT_TIOS			/* timer iostats updater */
};

struct pscfs_fh {
	struct vinode		*pf_vinode;
	struct slabtree		 pf_slabtree;
};

struct mgfs_thread {
	size_t			 mft_uniqid;
};

PSCTHR_MKCAST(mgfsthr, mgfs_thread, MGTHRT_FS)

void	 mgctlthr_spawn(void);

extern char			 ctlsockfn[];
extern char			 mountpoint[];

#endif /* _MOUNT_GLAZE_H_ */
