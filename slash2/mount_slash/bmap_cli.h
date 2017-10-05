/* $Id$ */
/*
 * %GPL_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License contained in the file
 * `COPYING-GPL' at the top of this distribution or at
 * https://www.gnu.org/licenses/gpl-2.0.html for more details.
 * ---------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _SLASH_BMAP_CLI_H_
#define _SLASH_BMAP_CLI_H_

#include "pfl/lock.h"
#include "pfl/rpc.h"

#include "bmap.h"
#include "pgcache.h"
#include "slashrpc.h"

/*
 * Private data associated with a bmap used by a SLASH2 client.
 */
struct bmap_cli_info {
	struct bmap_pagecache	 bci_bmpc;		/* must be first */
	struct srt_bmapdesc	 bci_sbd;		/* open bmap descriptor */
	struct timespec		 bci_etime;		/* current expire time */
	struct pfl_rwlock	 bci_rwlock;		/* page tree rwlock */
	int			 bci_flush_rc;		/* flush error */
	int			 bci_nreassigns;	/* number of reassigns */
	sl_ios_id_t		 bci_prev_sliods[SL_MAX_IOSREASSIGN];
	struct psc_listentry	 bci_lentry;		/* bmap flushq */
	uint8_t			 bci_repls[SL_REPLICA_NBYTES];
};



/* mount_slash specific bcm_flags: _BMAPF_SHIFT	= (1 <<  9) */

#define BMAPF_LEASEEXTREQ	(_BMAPF_SHIFT << 0)	/* lease request RPC in prog */
#define BMAPF_REASSIGNREQ	(_BMAPF_SHIFT << 1)	/* lease re-assign */
#define BMAPF_LEASEEXPIRE	(_BMAPF_SHIFT << 2)	/* lease has expired */
#define BMAPF_LEASEEXTEND	(_BMAPF_SHIFT << 3)	/* lease will be extend */

#define BMAPF_SCHED		(_BMAPF_SHIFT << 3)	/* bmap flush in progress */
#define BMAPF_BENCH		(_BMAPF_SHIFT << 4)	/* generated by benchmarker */
#define BMAPF_FLUSHQ		(_BMAPF_SHIFT << 5)	/* bmap is on writer flushq */
#define BMAPF_TIMEOQ		(_BMAPF_SHIFT << 6)	/* on timeout queue */


/* XXX change horribly named flags */
#define BMAP_CLI_MAX_LEASE	60			/* seconds */
#define BMAP_CLI_EXTREQSECS	20
#define BMAP_CLI_TIMEO_INC	1

static __inline struct bmap_cli_info *
bmap_2_bci(struct bmap *b)
{
	return (bmap_get_pri(b));
}

#define bmap_2_bci_const(b)	((const struct bmap_cli_info *)bmap_get_pri_const(b))

#define bmap_2_bmpc(b)		(&bmap_2_bci(b)->bci_bmpc)
#define bmap_2_restbl(b)	bmap_2_bci(b)->bci_repls
#define bmap_2_sbd(b)		(&bmap_2_bci(b)->bci_sbd)
#define bmap_2_ios(b)		bmap_2_sbd(b)->sbd_ios

#define bmpc_2_bmap(bmpc)	(((struct bmap *)bmpc) - 1)

void	 msl_bmap_cache_rls(struct bmap *);
int	 msl_bmap_lease_extend(struct bmap *, int);
void	 msl_bmap_lease_reassign(struct bmap *);

void	 bmap_biorq_expire(struct bmap *);

void	 msbwatchthr_main(struct psc_thread *);
void	 msbreleasethr_main(struct psc_thread *);

int      msl_bmap_reap(struct psc_poolmgr *);


extern struct timespec msl_bmap_max_lease;
extern struct timespec msl_bmap_timeo_inc;

extern int slc_bmap_max_cache;

static __inline struct bmap *
bci_2_bmap(struct bmap_cli_info *bci)
{
	struct bmap *b;

	psc_assert(bci);
	b = (void *)bci;
	return (b - 1);
}

#endif /* _SLASH_BMAP_CLI_H_ */
