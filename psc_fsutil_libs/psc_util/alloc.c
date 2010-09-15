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
#  include <fcntl.h>
#  include <unistd.h>

#  define GUARD_AFTER (PFL_DEBUG > 1)
#endif

struct psc_memalloc_key {
	void *p;
#ifndef __LP64__
	/*
	 * The hashtbl API requires keys to be 64-bit, so keep 4 zero
	 * bytes on 32-bit architectures.
	 */
	long l;
#endif
};

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

#ifdef PFL_DEBUG
struct psc_memalloc *
_psc_getpma(void *p)
{
	struct psc_memalloc_key key;
	struct psc_memalloc *pma;

	memset(&key, 0, sizeof(key));
	key.p = p;
	pma = psc_hashtbl_searchdel(&psc_memallocs, NULL, &key);
	psc_assert(pma);
	return (pma);
}

void
_psc_checkpma(struct psc_memalloc *pma)
{
	if (pma->pma_userlen % psc_pagesize)
		psc_assert(pfl_memchk(pma->pma_guardbase, PFL_MEMGUARD_MAGIC,
		    psc_pagesize - pma->pma_userlen % psc_pagesize));

	/*
	 * XXX disable access to region; add a mode where we leak on
	 * purpose with PROT_NONE to prevent use-after-free.
	 */
	if (mprotect(pma->pma_allocbase, psc_pagesize +
	    PSC_ALIGN(pma->pma_userlen, psc_pagesize),
	    PROT_READ | PROT_WRITE) == -1)
		psc_fatal("mprotect");
}
#endif

/**
 * psc_realloc - Allocate or resize a chunk of memory.
 * @oldp: current chunk of memory to resize or NULL for new chunk.
 * @size: desired size of memory chunk.
 * @flags: operational flags.
 */
void *
_psc_realloc(void *oldp, size_t size, int flags)
{
	int rc, save_errno;
	void *newp;

#ifdef PFL_DEBUG
	int guard_after = GUARD_AFTER;
	struct psc_memalloc *pma;
	size_t specsize, oldlen;

	/*
	 * Since PAGEALIGN'd allocations must end on a page-boundary,
	 * there is no guarentee they can begin on one, so we must force
	 * "guard-before" mode.
	 */
//	if (flags & PAF_PAGEALIGN | PAF_LOCK)
	if (flags & PAF_PAGEALIGN)
		guard_after = 0;

	if (oldp && (flags & PAF_NOGUARD) == 0) {
		if (flags & PAF_PAGEALIGN)
			errx(1, "realloc of page-aligned is not implemented");

		pma = _psc_getpma(oldp);

		/* XXX if CANFAIL occurs, restore protections. */
		_psc_checkpma(pma);

		oldp = pma->pma_allocbase;
		oldlen = pma->pma_userlen;
	}
#endif

	/*
	 * Disallow realloc(p, sz, PAF_LOCK) because we can't munlock
	 * the old region as we don't have the size.
	 */
	if ((flags & PAF_LOCK) && oldp)
		psc_fatalx("unable to lock realloc'd mem");

	/*
	 * If the new allocation size is 0, treat it like a free and
	 * return NULL.  The caller should be able to do anything he
	 * wants with NULL in place of a zero-length pointer as this
	 * behavior differs on systems.
	 */
	if (size == 0) {
		if (oldp) {
#ifdef PFL_DEBUG
			free(pma);
#endif
			free(oldp);
		}
		return (NULL);
	}

#ifdef PFL_DEBUG
	if ((flags & PAF_NOGUARD) == 0) {
		specsize = size;
		size = psc_pagesize + PSC_ALIGN(size, psc_pagesize);
		flags |= PAF_PAGEALIGN;

		if (oldp && size == psc_pagesize +
		    PSC_ALIGN(pma->pma_userlen, psc_pagesize)) {
			/* alloc rounds to identical size */
			newp = oldp;
			goto movepage;
		}
	}
#endif

 retry:
	if (flags & PAF_PAGEALIGN) {
#ifndef PFL_DEBUG
		if (oldp)
			errx(1, "realloc of page-aligned is not implemented");
#endif
		rc = posix_memalign(&newp, psc_pagesize, size);
		if (rc) {
			errno = rc;
			newp = NULL;
#ifdef PFL_DEBUG
		} else if (oldp) {
 movepage:
			/*
			 * Since realloc() does not work with
			 * posix_memalign() in that a page-aligned base
			 * is guarenteed to be returned, fabricate a
			 * realloc, solely for the PFL memory guard, by
			 * issuing a new page-aligned alloc, copying,
			 * and freeing the old chunk.
			 */
			if (guard_after)
				memmove((char *)newp + (psc_pagesize -
				    specsize % psc_pagesize) % psc_pagesize,
				    pma->pma_userbase, pma->pma_userlen);
			else
				memmove((char *)newp +
				    (pma->pma_userbase - pma->pma_allocbase),
				    pma->pma_userbase, pma->pma_userlen);
			if (size != psc_pagesize +
			    PSC_ALIGN(pma->pma_userlen, psc_pagesize)) {
				free(pma->pma_allocbase);
				goto setupguard;
			}
#endif
		}
	} else
		newp = realloc(oldp, size);
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
		int rem;

		if (oldp == NULL) {
			/*
			 * XXX consider using guard region for this to
			 * save on two malloc overhead.
			 */
			pma = calloc(1, sizeof(*pma));
			psc_assert(pma);
			psc_hashent_init(&psc_memallocs, pma);
		}
 setupguard:
		rem = (psc_pagesize - specsize % psc_pagesize) % psc_pagesize;

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
		if (oldp && size > oldlen && (flags & PAF_NOZERO) == 0)
			memset((char *)newp + oldlen, 0, size - oldlen);
	}
#endif

	if ((flags & PAF_LOCK) && mlock(newp, size) == -1) {
		if (flags & PAF_CANFAIL) {
psc_fatalx("not ready");
			save_errno = errno;
			PSCFREE(newp);
			psc_error("mlock");
			errno = save_errno;
			return (NULL);
		}
		psc_fatal("mlock");
	}
	if (oldp == NULL && (flags & PAF_NOZERO) == 0)
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
_psc_free(void *p, int flags, ...)
{
	va_list ap;

	if (p == NULL)
		return;

	if (flags & PAF_LOCK) {
		size_t len;

		va_start(ap, flags);
		len = va_arg(ap, size_t);
		va_end(ap);

		if (munlock(p, len) == -1)
			psc_fatal("munlock %p", p);
	}

#ifdef PFL_DEBUG
	if ((flags & PAF_NOGUARD) == 0) {
		struct psc_memalloc *pma;

		pma = _psc_getpma(p);
		p = pma->pma_allocbase;
		_psc_checkpma(pma);
		free(pma);
	}
#endif

	free(p);
}

#define PFL_DEF_NMEMMAPS	(1024 * 1024)
#define _PATH_MAX_MEMMAPS	"/proc/sys/vm/max_map_count"

void
psc_memallocs_init(void)
{
#ifdef PFL_DEBUG
	char *p, *endp, *fn = _PATH_MAX_MEMMAPS;
	long val, nmaps;
	char buf[128];
	ssize_t rc;
	int fd;

	fd = open(fn, O_RDWR);
	if (fd != -1) {
		nmaps = PFL_DEF_NMEMMAPS;
		p = getenv("PSC_MAX_NMEMMAPS");
		if (p) {
			nmaps = strtol(p, &endp, 10);
			if (nmaps < 0 || nmaps > INT_MAX ||
			    endp == p || *endp != '\0')
				psc_fatalx("invalid env PSC_MAX_NMEMMAPS: %s", p);
		}

		rc = pread(fd, buf, sizeof(buf), 0);
		if (rc == -1)
			psc_fatal("read %s", fn);

		val = strtol(buf, &endp, 10);
		if (val < 0 || val > INT_MAX ||
		    endp == p || *endp != '\0')
			psc_fatalx("error reading: %s", fn);

		if (val < nmaps) {
			snprintf(buf, sizeof(buf), "%ld", nmaps);
			rc = pwrite(fd, buf, sizeof(buf), 0);
			if (rc != sizeof(buf))
			    psc_fatalx("write %s", fn);
		}

		close(fd);
	}

	psc_hashtbl_init(&psc_memallocs, PHTF_NOMEMGUARD | PHTF_NOLOG,
	    struct psc_memalloc, pma_userbase, pma_hentry, 2047, NULL,
	    "memallocs");
#endif
}
