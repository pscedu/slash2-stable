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

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

#ifdef PFL_DEBUG
#  define GUARD_AFTER (PFL_DEBUG > 1)
#endif

int			psc_pagesize;

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
	int guard_after = GUARD_AFTER;
	struct psc_memalloc *pma;
	size_t specsize, oldlen;

	if (flags & PAF_PAGEALIGN)
		guard_after = 0;

	if ((flags & PAF_NOGUARD) == 0) {
		if (p) {
			pma = psc_hashtbl_searchdel(&psc_memallocs,
			    NULL, &p);
			psc_assert(pma);
			p = pma->pma_allocbase;
			oldlen = pma->pma_userlen;

			/*
			 * Remove protection restrictions since the
			 * region may move, although maybe we should
			 * keep it around in case someone tries to
			 * access the old pointer...
			 *
			 * XXX if CANFAIL occurs, restore protections.
			 */
			if (mprotect(pma->pma_allocbase, psc_pagesize +
			    PSC_ALIGN(pma->pma_userlen, psc_pagesize),
			    PROT_READ | PROT_WRITE) == -1)
				psc_fatal("mprotect");
		}

		specsize = size;

		if (size == 0)
			size = 1;

		size = psc_pagesize + PSC_ALIGN(size, psc_pagesize);

		flags |= PAF_PAGEALIGN;
	}
#endif

 retry:
	if (flags & PAF_PAGEALIGN) {
#ifndef PFL_DEBUG
		if (p)
			errx(1, "realloc of page-aligned is not implemented");
#endif
		/*
		 * XXX can posix_memalign(sz=0) return NULL like
		 * realloc(0, 0) can?
		 */
		rc = posix_memalign(&newp, psc_pagesize, size);
		if (rc) {
			errno = rc;
			newp = NULL;
#ifdef PFL_DEBUG
		} else if (p) {
			memcpy(newp, p, pma->pma_userlen);
			free(p);
#endif
		}
	} else {
		if (p && size == 0) {
			/*
			 * realloc(3) to zero on glibc returns NULL
			 * unconditionally, so specifically catch this
			 * and do a manual free and zero-length
			 * malloc(3).
			 */
			free(p);
			p = NULL;
			newp = malloc(0);
			psc_assert(newp);
		} else
			newp = realloc(p, size);
	}
	if (newp == NULL) {
		/*
		 * We didn't get our memory.  Try reaping some pools
		 * if enabled and retry; otherwise, handle failure.
		 */
		if ((flags & PAF_NOREAP) == 0) {
			_psc_pool_reapsome(size);
			flags |= PAF_NOREAP;
			goto retry;
		}
		if (flags & PAF_CANFAIL) {
			psc_error("malloc/realloc");
			return (NULL);
		}
		err(1, "malloc/realloc");
	}

#ifdef PFL_DEBUG
	if ((flags & PAF_NOGUARD) == 0) {
		int rem = psc_pagesize - specsize % psc_pagesize;

		if (p == NULL) {
			/*
			 * XXX consider using guard region for this to
			 * save on two malloc overhead.
			 */
			pma = calloc(1, sizeof(*pma));
			psc_assert(pma);
			psc_hashent_init(&psc_memallocs, pma);
		}

		pma->pma_userlen = specsize;
		pma->pma_allocbase = newp;

		if (guard_after) {
			pma->pma_userbase = (char *)newp + rem;
			pma->pma_guardbase = (char *)newp;

			if (mprotect(newp, PSC_ALIGN(specsize, psc_pagesize),
			    PROT_READ | PROT_WRITE) == -1)
				err(1, "mprotect");
			if (mprotect((char *)newp + size - psc_pagesize,
			    psc_pagesize, PROT_NONE) == -1)
				err(1, "mprotect");
		} else {
			pma->pma_userbase = (char *)newp + psc_pagesize;
			pma->pma_guardbase = (char *)pma->pma_userbase +
			    specsize;

			if (mprotect(newp + psc_pagesize,
			    PSC_ALIGN(specsize, psc_pagesize),
			    PROT_READ | PROT_WRITE) == -1)
				err(1, "mprotect");
			if (mprotect(newp, psc_pagesize, PROT_NONE) == -1)
				err(1, "mprotect");
		}
		if (rem != psc_pagesize)
			memset(pma->pma_guardbase, PFL_MEMGUARD_MAGIC, rem);

		psc_hashtbl_add_item(&psc_memallocs, pma);
		/* XXX mprotect PROT_NONE the pma itself */

		if ((flags & PAF_NOLOG) == 0)
			psclog_debug("alloc [guard] %p len %zd : "
			    "user %p len %zd : guard %p len %d\n",
			    pma->pma_allocbase, size,
			    pma->pma_userbase, pma->pma_userlen,
			    pma->pma_guardbase, rem);

		newp = pma->pma_userbase;
		size = specsize;

		/* Zero out any newly realloc()'d region */
		if (p && size > oldlen && (flags & PAF_NOZERO) == 0)
			memset((char *)newp + oldlen, 0, size - oldlen);
	}
#endif

	if (flags & PAF_LOCK) {
		/*
		 * Disallow realloc(p, sz, PAF_LOCK) because we can't
		 * munlock the old region as we don't have the size.
		 */
		if (p)
			psc_fatalx("unable to lock realloc'd mem");
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
_psc_free(void *p)
{
#ifdef PFL_DEBUG
	struct psc_memalloc *pma;
	size_t len;

	pma = psc_hashtbl_searchdel(&psc_memallocs, NULL, &p);
	psc_assert(pma);

	if (pma->pma_userlen % psc_pagesize)
		psc_assert(pfl_memchk(pma->pma_guardbase, PFL_MEMGUARD_MAGIC,
		    psc_pagesize - pma->pma_userlen % psc_pagesize));

	len = pma->pma_userlen;
	if (len == 0)
		len = 1;

	/* disable access to region */
	if (mprotect(pma->pma_allocbase, psc_pagesize +
	    PSC_ALIGN(len, psc_pagesize), PROT_READ | PROT_WRITE) == -1)
		psc_fatal("mprotect");

	p = pma->pma_allocbase;
	free(pma);
#endif
	free(p);
}

void
_psc_munlock(void *p, size_t len)
{
	if (munlock(p, len) == -1)
		psc_fatal("munlock %p", p);
}

void
psc_memallocs_init(void)
{
#ifdef PFL_DEBUG
	psc_hashtbl_init(&psc_memallocs, PHTF_NOMEMGUARD | PHTF_NOLOG,
	    struct psc_memalloc, pma_userbase, pma_hentry, 2047, NULL,
	    "memallocs");
#endif
}
