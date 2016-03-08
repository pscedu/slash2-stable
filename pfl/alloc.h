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

#ifndef _PFL_ALLOC_H_
#define _PFL_ALLOC_H_

#include <sys/types.h>
#include <sys/mman.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/log.h"

#ifndef HAVE_POSIX_MEMALIGN
# include "pfl/compat/posix_memalign.h"
#endif

/*
 * The -DPFL_DEBUG define operates the following way:
 *
 *	+-----------+-------------------------------------------------+
 *	|           |                      Feature                    |
 *	| PFL_DEBUG +--------+-------------+-------------+------------+
 *	|           | checks |   linger    | prot-before | prot-after |
 *	+-----------+--------+-------------+-------------+------------+
 *	| 0	    |        |             |             |            |
 *	| 1	    |   X    |             |             |            |
 *	| 2	    |   X    | if LINGERSZ |      X      |            |
 *	| 3	    |   X    | if LINGERSZ |             |      X     |
 *	+-----------+--------+-------------+-------------+------------+
 */

/* aliases for common usage */
#define PSCALLOC(sz)		psc_alloc((sz), 0)
#define TRY_PSCALLOC(sz)	psc_alloc((sz), PAF_CANFAIL)

#define PSC_REALLOC(p, sz)	psc_realloc((p), (sz), 0)
#define PSC_TRY_REALLOC(p, sz)	psc_realloc((p), (sz), PAF_CANFAIL)

#define PFL_MEMGUARD_LINGERSZ	10000	/* # freed allocs to keep around */

#if PFL_DEBUG > 1 && PFL_MEMGUARD_LINGERSZ
#  define _PFL_FREE		_psc_lru_userfree
#else
#  define _PFL_FREE		_psc_free
#endif

#define psc_free(p, flags, ...)						\
	do {								\
		if (((flags) & PAF_NOLOG) == 0)				\
			psclogs_debug(PSS_MEM, "free(%p)", (p));	\
		if (p) {						\
			_PFL_FREE((p), (flags), ##__VA_ARGS__);		\
			(p) = NULL;					\
		}							\
	} while (0)

#define PSCFREE(p)		psc_free((p), 0)

#define _PSC_REALLOC(oldp, sz, fl)					\
	{								\
		void *_p;						\
									\
		if ((oldp) && (sz) == 0) {				\
			psc_assert(((fl) & PAF_LOCK) == 0);		\
			if (((fl) & PAF_NOLOG) == 0)			\
				psclogs_debug(PSS_MEM,			\
				    "realloc(%p) to zero (free)",	\
				    (void *)(oldp));			\
			_PFL_FREE((oldp), (fl));			\
			_p = NULL;					\
		} else {						\
			_p = _psc_realloc((oldp), (sz), (fl));		\
			if (((fl) & PAF_NOLOG) == 0) {			\
				if (oldp)				\
					psclogs_debug(PSS_MEM,		\
					    "realloc(%p)=%p sz=%zu "	\
					    "fl=%d", (void *)(oldp),	\
					    _p, (size_t)(sz), (fl));	\
				else					\
					psclogs_debug(PSS_MEM,		\
					    "alloc()=%p sz=%zu fl=%d",	\
					    _p, (size_t)(sz), (fl));	\
			}						\
		}							\
		_p;							\
	}

#define psc_alloc(sz, fl)	(_PSC_REALLOC(NULL, (sz), (fl)))
#define psc_realloc(p, sz, fl)	(_PSC_REALLOC((p),  (sz), (fl)))

#define PFL_ALLOC_OBJ(p)	((p) = PSCALLOC(sizeof(*(p))))

/* allocation flags */
#define PAF_CANFAIL		(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN		(1 << 1)	/* align to physmem page size */
#define PAF_NOREAP		(1 << 2)	/* don't reap pools if mem unavail */
#define PAF_LOCK		(1 << 3)	/* mlock(2) mem regions as unswappable */
#define PAF_NOZERO		(1 << 4)	/* don't force memory zeroing */
#define PAF_NOLOG		(1 << 5)	/* don't psclog this allocation */
#define PAF_NOGUARD		(1 << 6)	/* do not use memory guards */

void	 *psc_calloc(size_t, size_t, int);
void	 _psc_free(void *, int, ...);
void	 _psc_lru_userfree(void *, int, ...);
void	*_psc_realloc(void *, size_t, int);
void	*pfl_memdup(const void *, size_t);

void	  psc_memallocs_init(void);

extern int	psc_pagesize;

#endif /* _PFL_ALLOC_H_ */
