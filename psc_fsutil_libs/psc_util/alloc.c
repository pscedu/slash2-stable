/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

long			psc_pagesize;

#ifdef PFL_DEBUG
struct psc_hashtbl	psc_memallocs;
#endif

/**
 * _psc_pool_reapsome - Provide an overrideable reclaimer for when pools
 *	are not in use.
 */
__weak void
_psc_pool_reapsome(__unusedx size_t size)
{
}

/**
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

#ifdef PFL_DEBUG
	struct psc_memalloc *pma;
	size_t specsize;

	if ((flags & PAF_NOGUARD) == 0) {
		specsize = size;

		if (size == 0)
			size = 1;

		size = psc_pagesize + PSC_ALIGN(size,
		    psc_pagesize);

		flags |= PAF_PAGEALIGN;
	}
#endif

 retry:
	if ((flags & PAF_PAGEALIGN) && p == NULL) {
		/* XXX can posix_memalign(sz=0) return NULL like realloc(0, 0) can? */
		rc = posix_memalign(&newp, psc_pagesize, size);
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
			goto out;
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
	if (p == NULL && (flags & PAF_NOZERO) == 0)
		memset(newp, 0, size);
 out:
#ifdef PFL_DEBUG
	if ((flags & PAF_NOGUARD) == 0) {
		if (p == NULL)

		pma = malloc(sizeof(*pma));
		psc_assert(pma);
		psc_hashent_init(&psc_memallocs, pma);

		pma->pma_start_base = newp;
		pma->pma_end_base = newp + size - psc_pagesize;
		pma->pma_total_size = size;
		pma->pma_offset = size -
		    psc_pagesize - specsize;
		newp = pma->pma_base = newp + pma->pma_offset;

		if (mmap(pma->pma_start_base, pma->pma_offset,
		    PROT_NONE, MAP_FIXED, -1, 0))
			psc_fatal("mmap");
		if (mmap(pma->pma_end_base, psc_pagesize,
		    PROT_NONE, MAP_FIXED, -1, 0))
			psc_fatal("mmap");

		psc_hashtbl_add_item(&psc_memallocs, pma);
	}
#endif
	return (newp);
}

/**
 * psc_calloc - Allocate zeroed memory for an array.
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
	return (psc_alloc(size * num, flags));
}

/**
 * psc_strdup - Duplicate a string, allocating memory as necessary for
 *	duplicate.
 * @str: original string to duplicate.
 *
 * Advantages of using this over strdup(2) are ties into reaping pool
 * hogs and logging.
 */
char *
psc_strdup(const char *str)
{
	size_t len;
	char *p;

	len = strlen(str) + 1;
	p = PSCALLOC(len);
	strlcpy(p, str, len);
	return (p);
}

void
_psc_free_guards(void *p)
{
#ifdef PFL_DEBUG
	struct psc_memalloc *pma, q;

	q.pma_base = p;

	pma = psc_hashtbl_searchdel(&psc_memallocs, NULL, &q);
	psc_assert(pma);

	if (munmap(pma->pma_start_base, pma->pma_offset) == -1)
		psc_fatal("munmap");
	if (munmap(pma->pma_end_base, psc_pagesize) == -1)
		psc_fatal("munmap");

	p = pma->pma_start_base;
	free(pma);
#endif
	free(p);
}

void
psc_memallocs_init(void)
{
#ifdef PFL_DEBUG
	psc_hashtbl_init(&psc_memallocs, 0, struct psc_memalloc,
	    pma_base, pma_hentry, 2048 - 1, NULL, "memallocs");
#endif
}
