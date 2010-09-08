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
	int guard_after = GUARD_AFTER;
	struct psc_memalloc *pma;
	size_t specsize;

	if (flags & PAF_PAGEALIGN)
		guard_after = 0;

	if ((flags & PAF_NOGUARD) == 0) {
		if (p) {
			pma = psc_hashtbl_searchdel(&psc_memallocs,
			    NULL, &p);
			psc_assert(pma);
			p = pma->pma_allocbase;

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
	if ((flags & PAF_PAGEALIGN) && p == NULL) {
		/*
		 * XXX can posix_memalign(sz=0) return NULL like
		 * realloc(0, 0) can?
		 */
		rc = posix_memalign(&newp, psc_pagesize, size);
		if (rc) {
			errno = rc;
			newp = NULL;
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
		 * if enabled and retry, otherwise, handle failure.
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
		/* Zero out any newly realloc()'d region */
		if (p && specsize > pma->pma_userlen &&
		    (flags & PAF_NOZERO) == 0)
			memset((char *)newp + pma->pma_userlen, 0,
			    specsize - pma->pma_userlen);

		if (p == NULL) {
			/*
			 * XXX consider using guard region for this to
			 * save on malloc overhead.
			 */
			pma = calloc(1, sizeof(*pma));
			psc_assert(pma);
			psc_hashent_init(&psc_memallocs, pma);
		}

		pma->pma_userlen = specsize;
		pma->pma_allocbase = newp;

		if (guard_after) {
			pma->pma_userbase = (char *)newp + specsize;
			pma->pma_guardbase = (char *)newp;

			if (mprotect(newp, PSC_ALIGN(specsize,
			    psc_pagesize), PROT_READ | PROT_WRITE))
				err(1, "mprotect");
			if (mprotect((char *)newp +
			    PSC_ALIGN(specsize, psc_pagesize),
			    psc_pagesize, PROT_NONE))
				err(1, "mprotect");
		} else {
			pma->pma_userbase = (char *)newp + psc_pagesize;
			pma->pma_guardbase = (char *)pma->pma_userbase +
			    specsize;

			if (mprotect(pma->pma_userbase,
			    PSC_ALIGN(specsize, psc_pagesize),
			    PROT_READ | PROT_WRITE))
				err(1, "mprotect");
			if (mprotect(newp, psc_pagesize, PROT_NONE))
				err(1, "mprotect");
		}
		memset(pma->pma_guardbase, PFL_MEMGUARD_MAGIC,
		    psc_pagesize - pma->pma_userlen % psc_pagesize);

		psc_hashtbl_add_item(&psc_memallocs, pma);
		/* XXX mprotect PROT_NONE the pma itself */

		if ((flags & PAF_NOLOG) == 0)
			psclog_debug("alloc [guard] %p len %zd : "
			    "user %p len %zd : guard %p len %zd\n",
			    pma->pma_allocbase, size,
			    pma->pma_userbase, pma->pma_userlen,
			    pma->pma_guardbase, psc_pagesize -
			    pma->pma_userlen % psc_pagesize);

		newp = pma->pma_userbase;
		size = specsize;
	}
#endif

	if (flags & PAF_LOCK) {
		/* Disallow realloc(p, sz, PAF_LOCK) for now. */
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

	psc_assert(pfl_memchk(pma->pma_guardbase, PFL_MEMGUARD_MAGIC,
	    psc_pagesize - pma->pma_userlen % psc_pagesize));

	len = pma->pma_userlen;
	if (len == 0)
		len = 1;

	/* disable access to region */
	if (mprotect(pma->pma_allocbase, psc_pagesize +
	    PSC_ALIGN(len, psc_pagesize), PROT_NONE) == -1)
		psc_fatal("mprotect");

	p = pma->pma_allocbase;
	free(pma);
#endif
	free(p);
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
