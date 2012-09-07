/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_IOSTATS_H_
#define _PFL_IOSTATS_H_

#include <sys/time.h>

#include <stdint.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/atomic.h"

#define IST_NAME_MAX	40
#define IST_NINTV	2

struct psc_iostats {
	char			ist_name[IST_NAME_MAX];
	struct psclist_head	ist_lentry;
	int			ist_flags;

	uint64_t		ist_len_total;			/* lifetime acculumator */

	struct psc_iostatv {
		struct timeval	istv_lastv;			/* time of last accumulation */
		psc_atomic64_t	istv_cur_len;			/* current accumulator */

		struct timeval	istv_intv_dur;			/* duration of accumulation */
		uint64_t	istv_intv_len;			/* length of accumulation */

	}			ist_intv[IST_NINTV];
};

#define PISTF_BASE10		(1 << 0)

#define psc_iostats_calcrate(len, tv)					\
	((len) / (((tv)->tv_sec * UINT64_C(1000000) + (tv)->tv_usec) * 1e-6))

#define psc_iostats_getintvrate(ist, n)					\
	psc_iostats_calcrate((ist)->ist_intv[n].istv_intv_len,		\
	    &(ist)->ist_intv[n].istv_intv_dur)

#define psc_iostats_getintvdur(ist, n)					\
	((ist)->ist_intv[n].istv_intv_dur.tv_sec +			\
	    (ist)->ist_intv[n].istv_intv_dur.tv_usec * 1e-6)

#define psc_iostats_intv_add(ist, amt)					\
	psc_atomic64_add(&(ist)->ist_intv[0].istv_cur_len, (amt))

#define psc_iostats_remove(ist)		pll_remove(&psc_iostats, (ist))

#define psc_iostats_init(ist, name, ...)				\
	psc_iostats_initf((ist), 0, (name), ## __VA_ARGS__)

void psc_iostats_initf(struct psc_iostats *, int, const char *, ...);
void psc_iostats_rename(struct psc_iostats *, const char *, ...);

extern struct psc_lockedlist	psc_iostats;
extern int			psc_iostat_intvs[];

struct opstat {
	char			*name;
	int64_t			 value;
};

struct slash2_client_opstats {

	struct opstat bmap_retrieve;
	struct opstat bmap_lease_ext;

	struct opstat flush_attr;
	struct opstat fsync;

	struct opstat getxattr;

	struct opstat listxattr;

	struct opstat offline_retry;

	struct opstat read;
	struct opstat read_ahead;
	struct opstat read_rpc_launch;
	struct opstat readdir;
	struct opstat readdir_retry;
	struct opstat removexattr;
	struct opstat rename;

	struct opstat setattr;
	struct opstat setxattr;

	struct opstat write;

	struct opstat eof_of_opstat;
};

#define	OPSTATS_INC(op)		msl_opstats.op.value++
#define	OPSTATS_SET(op, val)	msl_opstats.op.value = val

extern struct slash2_client_opstats msl_opstats;

#endif /* _PFL_IOSTATS_H_ */
