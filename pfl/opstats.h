/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Operation statistics: for various types of counters of system operation.
 * Every second, each registered opstat is recomputed to measure
 * instantaneous operation rates.  10-second weighted averages are also
 * computed.
 */

#ifndef _PFL_OPSTATS_H_
#define _PFL_OPSTATS_H_

#include <sys/time.h>

#include <stdint.h>

#include "pfl/atomic.h"
#include "pfl/bsearch.h"
#include "pfl/list.h"
#include "pfl/lockedlist.h"

struct pfl_opstat {
	int			 opst_flags;

	psc_atomic64_t		 opst_lifetime;	/* lifetime accumulator */
	/*
	 * Unlike the above lifetime counter, the following four fields
	 * are maintained by the timer thread. See pfl_opstimerthr_main() 
	 * for more details.
	 */
	int64_t			 opst_last;	/* last second lifetime value */
	int64_t			 opst_intv;	/* last second counter */
	double			 opst_avg;	/* running average */
	double			 opst_max;	/* max running average */

	char			 opst_name[0];
};

#define OPSTF_BASE10		(1 << 0)	/* use base-10 numbering instead of default of base-2 */
#define OPSTF_EXCL		(1 << 1)	/* like O_EXCL: when creating, opstat must not exist  */

#define pfl_opstat_add(opst, n)	psc_atomic64_add(&(opst)->opst_lifetime, (n))
#define	pfl_opstat_incr(opst)	pfl_opstat_add((opst), 1)

#define pfl_opstat_dec(opst, n)	psc_atomic64_sub(&(opst)->opst_lifetime, (n))
#define	pfl_opstat_decr(opst)	pfl_opstat_dec((opst), 1)

/*
 * This API explicitly disallows printf-like args as it caches the
 * opstat for speed and thus cannot do so with variadic names.
 */
#define	OPSTATF_ADD(flags, name, n)					\
	do {								\
		static struct pfl_opstat *_opst;			\
									\
		if (_opst == NULL)					\
			_opst = pfl_opstat_initf((flags), (name));	\
		pfl_opstat_add(_opst, (n));				\
	} while (0)

#define	OPSTATF_SUB(flags, name, n)					\
	do {								\
		static struct pfl_opstat *_opst;			\
									\
		if (_opst == NULL)					\
			_opst = pfl_opstat_initf((flags), (name));	\
		pfl_opstat_dec(_opst, (n));				\
	} while (0)

#define	OPSTAT_INCR(name)	OPSTATF_ADD(OPSTF_BASE10, (name), 1)
#define	OPSTAT_ADD(name, n)	OPSTATF_ADD(OPSTF_BASE10, (name), (n))

#define	OPSTAT_DECR(name)	OPSTATF_SUB(OPSTF_BASE10, (name), 1)
#define	OPSTAT_SUB(name, n)	OPSTATF_SUB(OPSTF_BASE10, (name), (n))

#define	OPSTAT2_ADD(name, n)	OPSTATF_ADD(0, (name), (n))

/* read/write counters */
struct pfl_iostats_rw {
	struct pfl_opstat	*wr;
	struct pfl_opstat	*rd;
};

struct pfl_opstat_bucket {
	int64_t			 ob_lower_bound;
	struct pfl_opstat	*ob_opst;
};

/* graduated opstats (i.e. bucketized by some criteria) */
struct pfl_opstats_grad {
	int			 og_nbuckets;
	struct pfl_opstat_bucket
				*og_buckets;
};

#define pfl_opstat_init(name, ...)					\
	pfl_opstat_initf(0, (name), ## __VA_ARGS__)

void	pfl_opstat_destroy(struct pfl_opstat *);
void	pfl_opstat_destroy_pos(int);
struct pfl_opstat *
	pfl_opstat_initf(int, const char *, ...);

void	pfl_opstats_grad_init(struct pfl_opstats_grad *, int, int64_t *,
	    int, const char *, ...);
void	pfl_opstats_grad_destroy(struct pfl_opstats_grad *);

#define pfl_opstats_grad_get(og, val)					\
	(&(og)->og_buckets[bsearch_floor((void *)(uintptr_t)(val),	\
	    (og)->og_buckets, (og)->og_nbuckets,			\
	    sizeof((og)->og_buckets[0]), pfl_opstats_grad_cmp)])

#define pfl_opstats_grad_incr(og, criteria)				\
	pfl_opstat_incr(pfl_opstats_grad_get((og), (criteria))->ob_opst)

extern int			pfl_opstats_sum;
extern struct psc_dynarray	pfl_opstats;
extern struct psc_spinlock	pfl_opstats_lock;

static __inline int
pfl_opstats_grad_cmp(const void *key, const void *item)
{
	const struct pfl_opstat_bucket *ob = item;
	uintptr_t keyval = (uintptr_t)key;

	return (CMP((int64_t)keyval, ob->ob_lower_bound));
}

#endif /* _PFL_OPSTATS_H_ */
