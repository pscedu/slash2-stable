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
#include <sys/mman.h>

#include <stdint.h>
#include <stdlib.h>

#include "psc_util/log.h"

#ifdef PFL_DEBUG
#include "pfl/hashtbl.h"
#endif

#ifndef HAVE_POSIX_MEMALIGN
# include "pfl/compat/posix_memalign.h"
#endif

/* aliases for common usage */
#define PSCALLOC(sz)		psc_alloc((sz), 0)
#define TRY_PSCALLOC(sz)	psc_alloc((sz), PAF_CANFAIL)

#define PSC_REALLOC(p, sz)	psc_realloc((p), (sz), 0)
#define PSC_TRY_REALLOC(p, sz)	psc_realloc((p), (sz), PAF_CANFAIL)

#ifdef PFL_DEBUG
struct psc_memalloc {
	void			*pma_start_base;
	void			*pma_end_base;
	void			*pma_base;
	int			 pma_total_size;
	int			 pma_offset;
	struct psc_hashent	 pma_hentry;
};
#endif

#define psc_free_nolog(p)						\
	do {								\
		_psc_free_guards(p);					\
		(p) = NULL;						\
	} while (0)

#define PSCFREE(p)							\
	do {								\
		psc_debugs(PSS_MEM, "free(%p)", (p));			\
		psc_free_nolog(p);					\
	} while (0)

#define _PSC_REALLOC(oldp, sz, fl)					\
	{								\
		void *_p;						\
									\
		_p = _psc_realloc((oldp), (sz), (fl));			\
		if (((fl) & PAF_NOLOG) == 0) {				\
			if (oldp)					\
				psc_debugs(PSS_MEM, "realloc(%p)=%p "	\
				    "sz=%zu fl=%d", (oldp), _p,		\
				    (size_t)(sz), (fl));		\
			else						\
				psc_debugs(PSS_MEM, "alloc()=%p "	\
				    "sz=%zu fl=%d", _p, (size_t)(sz),	\
				    (fl));				\
		}							\
		_p;							\
	}

#define psc_free_mlocked(p, size)					\
	do {								\
		if ((p) && munlock((p), (size)) == -1)			\
			psc_fatal("munlock %p", (p));			\
		PSCFREE(p);						\
	} while (0)

#define psc_free_aligned(p)	PSCFREE(p)

#define psc_free_mlocked_aligned(p, size)				\
	do {								\
		if ((p) && munlock((p), (size)) == -1)			\
			psc_fatal("munlock %p", (p));			\
		PSCFREE(p);						\
	} while (0)

#define psc_alloc(sz, fl)	(_PSC_REALLOC(NULL, (sz), (fl)))
#define psc_realloc(p, sz, fl)	(_PSC_REALLOC((p),  (sz), (fl)))

/* allocation flags */
#define PAF_CANFAIL	(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN	(1 << 1)	/* align to physmem page size */
#define PAF_NOREAP	(1 << 2)	/* don't reap pools if mem unavail */
#define PAF_LOCK	(1 << 3)	/* lock mem regions as unswappable */
#define PAF_NOZERO	(1 << 4)	/* don't force memory zeroing */
#define PAF_NOLOG	(1 << 5)	/* don't psclog this allocation */
#define PAF_NOGUARD	(1 << 6)	/* do not allow mlock(2) guards (internal) */

void	 *psc_calloc(size_t, size_t, int);
void	 _psc_free_guards(void *);
void	*_psc_realloc(void *, size_t, int);
char	 *psc_strdup(const char *);

void	  psc_memallocs_init(void);

extern long			psc_pagesize;

#ifdef DEBUG
extern struct psc_hashtbl	psc_memallocs;
#endif

#endif /* _PFL_ALLOC_H_ */
