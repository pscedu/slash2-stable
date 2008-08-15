/* $Id$ */

#ifndef __PFL_ALLOC_H__
#define __PFL_ALLOC_H__

#include <sys/types.h>

#include <stdlib.h>

/* aliases for common usage */
#define PSCALLOC(s)	psc_alloc((s), 0)
#define TRY_PSCALLOC(s)	psc_alloc((s), PAF_CANFAIL)
#define PSCFREE(p)	free(p)

/* allocation flags */
#define PAF_CANFAIL	(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN	(1 << 1)	/* align to physmem page size */
#define PAF_NOREAP	(1 << 2)	/* don't reap mem pools if mem unavail */
#define PAF_LOCK	(1 << 3)	/* lock mem regions as unswappable */

void	*psc_alloc(size_t, int);
void	*psc_calloc(size_t, size_t);
void	 psc_freel(void *, size_t);

extern long pscPageSize;

#endif /* __PFL_ALLOC_H__ */
