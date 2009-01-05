/* $Id$ */

#ifndef _PFL_ALLOC_H_
#define _PFL_ALLOC_H_

#include <sys/types.h>

#include <stdlib.h>

/* aliases for common usage */
#define PSCALLOC(sz)		psc_alloc((sz), 0)
#define TRY_PSCALLOC(sz)	psc_alloc((sz), PAF_CANFAIL)

#define PSC_REALLOC(p, sz)	psc_realloc((p), (sz), 0)
#define PSC_TRY_REALLOC(p, sz)	psc_realloc((p), (sz), PAF_CANFAIL)

#define PSCFREE(p)		free(p)

#define psc_alloc(sz, fl)	psc_realloc(NULL, (sz), (fl))

/* allocation flags */
#define PAF_CANFAIL	(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN	(1 << 1)	/* align to physmem page size */
#define PAF_POOLREAP	(1 << 2)	/* reap pools if mem unavail */
#define PAF_LOCK	(1 << 3)	/* lock mem regions as unswappable */
#define PAF_NOZERO	(1 << 4)	/* don't force memory zeroing */

void	*psc_realloc(void *, size_t, int);
void	*psc_calloc(size_t, size_t, int);
void	 psc_freel(void *, size_t);

extern long pscPageSize;

#endif /* _PFL_ALLOC_H_ */
