/* $Id$ */

#ifndef __PFL_ALLOC_H__
#define __PFL_ALLOC_H__

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "psc_util/log.h"

#define PSCALLOC(s)	_PSCALLOC((s), 0)
#define TRY_PSCALLOC(s)	_PSCALLOC((s), 1)

static inline void *
_PSCALLOC(size_t size, int can_fail)
{
	void *ptr = malloc(size);

	if (ptr == NULL) {
		if (!can_fail)
			psc_fatal("malloc");
		else {
			psc_error("malloc");
			return (NULL);
		}
	}
	memset(ptr, 0, size);
	return (ptr);
}

static inline void *
PSC_CALLOC(size_t num, size_t size)
{
	if (num && SIZE_MAX / num < size) {
		errno = ENOMEM;
		return (NULL);
	}
	return (PSCALLOC(size * num));
}

#define PSCFREE(p) free(p)

void *palloc(size_t);

extern long pscPageSize;

#endif /* __PFL_ALLOC_H__ */
