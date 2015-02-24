/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
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
#include "pfl/list.h"
#include "pfl/lockedlist.h"

#define pfl_opstat_add(opst, n)		psc_atomic64_add(&(opst)->opst_intv, (n))

#define	pfl_opstat_incr(opst, ...)	pfl_opstat_add((opst), 1)

#define	OPSTATF_ADD(flags, name, n)					\
	do {								\
		static struct pfl_opstat *_opst;			\
									\
		if (_opst == NULL)					\
			_opst = pfl_opstat_initf((flags), (name));	\
		pfl_opstat_add(_opst, (n));				\
	} while (0)

#define	OPSTAT_INCR(name)	OPSTATF_ADD(OPSTF_BASE10, (name), 1)
#define	OPSTAT_ADD(name, n)	OPSTATF_ADD(OPSTF_BASE10, (name), (n))

#define	OPSTAT2_ADD(name, n)	OPSTATF_ADD(0, (name), (n))

struct pfl_opstat {
	char			*opst_name;
	int			 opst_flags;

	uint64_t		 opst_lifetime;	/* lifetime accumulator */
	psc_atomic64_t		 opst_intv;	/* current instantaneous interval accumulator */
	uint64_t		 opst_last;	/* last second interval accumulator */
	double			 opst_avg;	/* 10-second average */
};

#define OPSTF_BASE10		(1 << 0)

struct pfl_iostats_rw {
	struct pfl_opstat	*wr;
	struct pfl_opstat	*rd;
};

/* graduated I/O stats */
struct pfl_iostats_grad {
	int64_t			 size;
	struct pfl_iostats_rw	 rw;
};

#define pfl_opstat_init(name, ...)					\
	pfl_opstat_initf(0, (name), ## __VA_ARGS__)

void	pfl_opstat_destroy(struct pfl_opstat *);
struct pfl_opstat *
	pfl_opstat_initf(int, const char *, ...);

void	pfl_iostats_grad_init(struct pfl_iostats_grad *, int, const char *);

extern struct psc_dynarray	pfl_opstats;
extern struct psc_spinlock	pfl_opstats_lock;

#endif /* _PFL_OPSTATS_H_ */
