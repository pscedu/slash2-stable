/* $Id$ */

#ifndef HAVE_PSC_ALLOC_INC
#define HAVE_PSC_ALLOC_INC

#include <stdlib.h>
#include <string.h>

#include "psc_util/log.h"

#define PSCALLOC(s)	PSCALLOC_(s, 0)
#define TRY_PSCALLOC(s)	PSCALLOC_(s, 1)

static inline void *
PSCALLOC_(size_t size, int can_fail)
{
	void *ptr = malloc(size);

	if (ptr == NULL) {
		if (!can_fail)
			psc_fatal("malloc()");
		else {
			psc_error("malloc()");
			return NULL;
		}
	}
	memset(ptr, 0, size);
	return ptr;
}

void *palloc(size_t);

extern long pscPageSize;

#endif /* HAVE_PSC_ALLOC_INC */
