/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_ALLOC_H_
#define _PFL_ALLOC_H_

#include <sys/types.h>

#include <stdlib.h>

/* aliases for common usage */
#define PSCALLOC(sz)		psc_alloc((sz), 0)
#define TRY_PSCALLOC(sz)	psc_alloc((sz), PAF_CANFAIL)

#define PSC_REALLOC(p, sz)	psc_realloc((p), (sz), 0)
#define PSC_TRY_REALLOC(p, sz)	psc_realloc((p), (sz), PAF_CANFAIL)

#define PSCFREE(p)							\
	do {								\
		psc_traces(PSS_MEM, "freeing %p", (p));			\
		free(p);						\
		(p) = NULL;						\
	} while (0)

/* Free without logging */
#define psc_free_nl(p)							\
	do {								\
		free(p);						\
		(p) = NULL;						\
	} while (0)

#define _PSC_REALLOC(oldp, sz, fl)					\
	{								\
		void *__p;						\
									\
		__p = _psc_realloc((oldp), (sz), (fl));			\
		if (((fl) & PAF_NOLOG) == 0) {				\
			if (oldp)					\
				psc_traces(PSS_MEM,			\
				    "realloc %p->%p sz=%zu fl=%d",	\
				    (oldp), __p, (size_t)(sz), (fl));	\
			else						\
				psc_traces(PSS_MEM,			\
				    "alloc %p sz=%zu fl=%d", __p,	\
				    (size_t)(sz), (fl));		\
		}							\
		__p;							\
	}

#define psc_alloc(sz, fl)	(_PSC_REALLOC(NULL, (sz), (fl)))
#define psc_realloc(p, sz, fl)	(_PSC_REALLOC((p), (sz), (fl)))

/* allocation flags */
#define PAF_CANFAIL	(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN	(1 << 1)	/* align to physmem page size */
#define PAF_NOREAP	(1 << 2)	/* don't reap pools if mem unavail */
#define PAF_LOCK	(1 << 3)	/* lock mem regions as unswappable */
#define PAF_NOZERO	(1 << 4)	/* don't force memory zeroing */
#define PAF_NOLOG	(1 << 5)	/* don't psclog this allocation */

void	*_psc_realloc(void *, size_t, int);
void	*psc_calloc(size_t, size_t, int);
void	 psc_freel(void *, size_t);
void	 psc_freen(void *);
void	 psc_freenl(void *, size_t);

extern long pscPageSize;

#endif /* _PFL_ALLOC_H_ */
