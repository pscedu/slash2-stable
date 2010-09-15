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

#include "pfl/hashtbl.h"
#include "psc_util/log.h"

#ifndef HAVE_POSIX_MEMALIGN
# include "pfl/compat/posix_memalign.h"
#endif

/* aliases for common usage */
#define PSCALLOC(sz)		psc_alloc((sz), 0)
#define TRY_PSCALLOC(sz)	psc_alloc((sz), PAF_CANFAIL)

#define PSC_REALLOC(p, sz)	psc_realloc((p), (sz), 0)
#define PSC_TRY_REALLOC(p, sz)	psc_realloc((p), (sz), PAF_CANFAIL)

struct psc_memalloc {
	void			*pma_allocbase;		/* guarded alloc region */
	void			*pma_userbase;		/* user alloc region */
#ifndef __LP64__
	long			_pma_pad;
#endif
	void			*pma_guardbase;		/* user alloc region */
	size_t			 pma_userlen;
	struct psc_hashent	 pma_hentry;
};

#if PFL_DEBUG > 1

/*
 * Memory guard legend:
 *	X - no access (PROT_NONE)
 *	U - user region
 *	G - guard byte, value checked in free()
 *	< - left fill
 *	> - right fill
 *
 * If PFL_DEBUG > 2, the layout looks like this ("guard after"):
 *
 *	 <----- PAGE -----> <----- PAGE ----->
 *	+-----------+------+------------------+
 *	| GGGGGGGG> | UUUU | XXXXXXXXXXXXXXXX |
 *	+-----------+------+------------------+
 *
 * Otherwise, the layout looks like this ("guard before"):
 *
 *	 <----- PAGE -----> <----- PAGE ----->
 *	+------------------+------+-----------+
 *	| XXXXXXXXXXXXXXXX | UUUU | <GGGGGGGG |
 *	+------------------+------+-----------+
 *
 * If PAF_ALIGN was specified, case #2 must always be used.  The guard
 * checks should hopefully be enough to catch out-of-range memory
 * access.
 *
 * When a region of memory is freed, the page(s) constituting the area
 * are mapped PROT_NONE.
 */

#define PFL_MEMGUARD_MAGIC	0x7a
#endif

#define psc_free(p, flags, ...)						\
	do {								\
		if (((flags) & PAF_NOLOG) == 0)				\
			psc_debugs(PSS_MEM, "free(%p)", (p));		\
		_psc_free((p), (flags), ##__VA_ARGS__);			\
		(p) = NULL;						\
	} while (0)

#define PSCFREE(p)		psc_free((p), 0)

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

#define psc_alloc(sz, fl)	(_PSC_REALLOC(NULL, (sz), (fl)))
#define psc_realloc(p, sz, fl)	(_PSC_REALLOC((p),  (sz), (fl)))

#define PFL_ALLOC_OBJ(p)	((p) = PSCALLOC(sizeof(*(p)))

/* allocation flags */
#define PAF_CANFAIL		(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN		(1 << 1)	/* align to physmem page size */
#define PAF_NOREAP		(1 << 2)	/* don't reap pools if mem unavail */
#define PAF_LOCK		(1 << 3)	/* lock mem regions as unswappable */
#define PAF_NOZERO		(1 << 4)	/* don't force memory zeroing */
#define PAF_NOLOG		(1 << 5)	/* don't psclog this allocation */
#define PAF_NOGUARD		(1 << 6)	/* do not use memory guards */

void	 *psc_calloc(size_t, size_t, int);
void	 _psc_free(void *, int, ...);
void	*_psc_realloc(void *, size_t, int);
char	 *psc_strdup(const char *);

void	  psc_memallocs_init(void);

extern int	psc_pagesize;

#endif /* _PFL_ALLOC_H_ */
