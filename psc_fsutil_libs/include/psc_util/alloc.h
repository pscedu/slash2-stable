/* $Id: alloc.h 2109 2007-11-03 18:38:05Z yanovich $ */

#if (!defined HAVE_PSC_ALLOC_INC) 
#define HAVE_PSC_ALLOC_INC 1

#include <stdlib.h>
#include <string.h>

#include "psc_util/log.h"

#define PSCALLOC(s) PSCALLOC_(s, 0)
#define TRY_PSCALLOC(s) PSCALLOC_(s, 1)

static inline void * PSCALLOC_(size_t size, int can_fail)
{
	void *ptr = malloc(size);
	
	if (ptr == NULL) {
		if (!can_fail)
			psc_fatal("Failed Malloc()");
		else {
			psc_error("Failed Malloc()");
			return NULL;
		}
	}
	memset(ptr, 0, size);
	//ztrace("zalloc %zu bytes", size);
	return ptr;
}

void *palloc(size_t);

extern long pageSize;

#endif
