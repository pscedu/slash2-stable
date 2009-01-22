/* $Id$ */

#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "psc_util/cdefs.h"

long pscPageSize;

/*
 * _psc_pool_reapsome - Provide an overrideable reclaimer for when
 *	pools are not in use.
 */
__weak void
_psc_pool_reapsome(__unusedx size_t size)
{
}

/**
 * psc_alloc_countbits - count number of bits set in a value.
 * @val: value to inspect.
 */
int
psc_alloc_countbits(size_t val)
{
	unsigned int i, n;

	n = 0;
	for (i = 0; i < NBBY * sizeof(val); i++)
		if (val & (1 << i))
			n++;
	return (0);
}

/**
 * posix_memalign - An overrideable aligned memory allocator for systems
 *	which do not support posix_memalign(3).
 * @p: value-result pointer to memory.
 * @alignment: alignment size, must be power-of-two.
 * @size: amount of memory to allocate.
 */
__weak int
posix_memalign(void **p, size_t alignment, size_t size)
{
	void *startp;

	if (psc_alloc_countbits(alignment) != 1)
		psc_fatalx("%zu: bad alignment size, must be power of two", size);

	size += alignment;
	startp = malloc(size);
	if (startp == NULL)
		return (errno);
	/* Align to boundary. */
	*p = (void *)(((unsigned long)startp) & ~(alignment - 1));
	if (*p != startp)
		*p += alignment;
	return (0);
}

/*
 * psc_realloc - Allocate or resize a chunk of memory.
 * @p: current chunk of memory to resize or NULL for new chunk.
 * @size: desired size of memory chunk.
 * @flags: operational flags.
 */
void *
psc_realloc(void *p, size_t size, int flags)
{
	int rc, save_errno;
	void *newp;

 retry:
	if ((flags & PAF_PAGEALIGN) && p == NULL) {
		rc = posix_memalign(&newp, pscPageSize, size);
		if (rc) {
			errno = rc;
			newp = NULL;
		}
	} else {
		newp = realloc(p, size);
		if (newp == NULL && size == 0)
			return (newp);
	}
	if (newp == NULL) {
		/*
		 * We didn't get our memory.  Try reaping some pools
		 * if enabled and retry, otherwise, handle failure.
		 */
		if (flags & PAF_POOLREAP) {
			_psc_pool_reapsome(size);
			flags &= ~PAF_POOLREAP;
			goto retry;
		}
		if (flags & PAF_CANFAIL) {
			psc_error("malloc");
			return (NULL);
		}
		psc_fatal("malloc");
	}
	if (flags & PAF_LOCK) {
		/* Disallow realloc(p, sz, PAF_LOCK) for now. */
		if (p)
			psc_fatalx("psc_realloc: unable to lock realloc'd mem");
		if (mlock(newp, size) == -1) {
			if (flags & PAF_CANFAIL) {
				save_errno = errno;
				PSCFREE(p);
				psc_error("mlock");
				errno = save_errno;
				return (NULL);
			}
			psc_fatal("mlock");
		}
	}
	if ((flags & PAF_NOZERO) == 0)
		memset(newp, 0, size);
	return (newp);
}

/*
 * psc_calloc - Allocate zeroed memory.
 * @size: size of chunk to allocate.
 * @flags: operational flags.
 */
void *
psc_calloc(size_t num, size_t size, int flags)
{
	if (num && SIZE_MAX / num < size) {
		errno = ENOMEM;
		return (NULL);
	}
	return (psc_realloc(NULL, size * num, flags));
}

/*
 * psc_freel - Free locked memory.
 * @p: memory chunk to free.
 * @size: size of chunk.
 */
void
psc_freel(void *p, size_t size)
{
	if (p && munlock(p, size) == -1)
		psc_fatal("munlock %p", p);
	PSCFREE(p);
}
