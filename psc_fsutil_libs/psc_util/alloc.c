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

#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/cdefs.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

long pscPageSize;

/*
 * _psc_pool_reapsome - Provide an overrideable reclaimer for when
 *	pools are not in use.
 */
__weak void
_psc_pool_reapsome(__unusedx size_t size)
{
}

/*
 * psc_realloc - Allocate or resize a chunk of memory.
 * @p: current chunk of memory to resize or NULL for new chunk.
 * @size: desired size of memory chunk.
 * @flags: operational flags.
 */
void *
_psc_realloc(void *p, size_t size, int flags)
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
		if (newp == NULL && size == 0) {
			psc_assert(p);
			newp = malloc(0);
			psc_assert(newp);
			return (newp);
		}
	}
	if (newp == NULL) {
		/*
		 * We didn't get our memory.  Try reaping some pools
		 * if enabled and retry, otherwise, handle failure.
		 */
		if ((flags & PAF_NOREAP) == 0) {
			_psc_pool_reapsome(size);
			flags |= PAF_NOREAP;
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
	/* XXX provide a way to zero out new regions of realloc'd mem */
	if (p == NULL && (flags & PAF_NOZERO) == 0)
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

/*
 * psc_freen - Free aligned memory.
 * @p: memory chunk to free.
 */
__weak void
psc_freen(void *p)
{
	PSCFREE(p);
}

/*
 * psc_freenl - Free locked aligned memory.
 * @p: memory chunk to free.
 * @size: size of chunk.
 */
__weak void
psc_freenl(void *p, size_t size)
{
	if (p && munlock(p, size) == -1)
		psc_fatal("munlock %p", p);
	PSCFREE(p);
}
