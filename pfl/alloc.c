/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/types.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/hashtbl.h"
#include "pfl/log.h"
#include "pfl/str.h"

#if PFL_DEBUG > 1

struct psc_memalloc {
	void			*pma_allocbase;		/* guarded alloc region */
	void			*pma_userbase;		/* user alloc region */
#ifndef __LP64__
	long			_pma_pad;
#endif
	void			*pma_guardbase;		/* user alloc region */
	size_t			 pma_userlen;
	union {
		struct pfl_hashentry	 pmau_hentry;
		struct psc_listentry	 pmau_lru_lentry;
	} pma_u;
#define pma_hentry	pma_u.pmau_hentry
#define pma_lru_lentry	pma_u.pmau_lru_lentry
};

#define PMA_LEN(pma)		(psc_pagesize +				\
				 PSC_ALIGN((pma)->pma_userlen, psc_pagesize))

/*
 * Memory guard diagram legend:
 *	X - no access (PROT_NONE)
 *	U - user region
 *	G - inner guard byte, checked in free() and realloc()
 *	< - left fill
 *	> - right fill
 *
 * If PFL_DEBUG > 2, the layout looks like this ("guard after"):
 *
 *	 <----- PAGE -----> <----- PAGE ----->
 *	+-----------+------+------------------+
 *	| GGGGGGGG> | UUUU | XXXXXXXXXXXXXXXX |
 *	+-----------+------+------------------+
 *
 * Otherwise, the layout looks like this ("guard before"):
 *
 *	 <----- PAGE -----> <----- PAGE ----->
 *	+------------------+------+-----------+
 *	| XXXXXXXXXXXXXXXX | UUUU | <GGGGGGGG |
 *	+------------------+------+-----------+
 *
 * If PAF_ALIGN was specified, case #2 must always be used.  The guard
 * checks should hopefully be enough to catch out-of-range memory
 * access.
 *
 * When a region of memory is freed, the page(s) constituting the area
 * are mapped PROT_NONE.
 */

#  define PFL_MEMGUARD_MAGIC	0x7a	/* guard byte val written within page */
#endif

#define GUARD_AFTER	(PFL_DEBUG > 2)

#if PFL_DEBUG > 1
#  include "pfl/list.h"
#  include "pfl/mem.h"
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

#if PFL_DEBUG > 1
struct psc_hashtbl	psc_memallocs;
struct psc_lockedlist	psc_memallocs_lru =
    PLL_INIT_NOLOG(&psc_memallocs_lru, struct psc_memalloc, pma_lru_lentry);
#endif

#if PFL_DEBUG > 1
struct psc_memalloc *
_psc_getpma(void *p)
{
	struct psc_memalloc_key key;
	struct psc_memalloc *pma;

	memset(&key, 0, sizeof(key));
	key.p = p;
	pma = psc_hashtbl_searchdel(&psc_memallocs, &key);
	psc_assert(pma);
	return (pma);
}

void
pscmem_checkinnerguard(struct psc_memalloc *pma)
{
	size_t len;

	if (pma->pma_userlen % psc_pagesize) {
		len = psc_pagesize - pma->pma_userlen % psc_pagesize;
		psc_mprotect(MIN(pma->pma_guardbase, pma->pma_userbase),
		    len, PROT_READ);
		psc_assert(pfl_memchk(pma->pma_guardbase,
		    PFL_MEMGUARD_MAGIC, len));
	}
}

void _psc_lru_sysfree(struct psc_memalloc *);
#endif

/*
 * Allocate or resize a chunk of memory.
 * @oldp: current chunk of memory to resize or NULL for new chunk.
 * @size: desired size of memory chunk.
 * @flags: operational flags.
 */
void *
_psc_realloc(void *oldp, size_t size, int flags)
{
	int rc, save_errno;
	void *newp;

#if PFL_DEBUG > 1
	int guard_after = GUARD_AFTER;
	struct psc_memalloc *pma;
	size_t specsize, oldlen;

#  if PFL_MEMGUARD_LINGERSZ
	if (oldp == NULL) {
		PLL_LOCK(&psc_memallocs_lru);
		if (pll_nitems(&psc_memallocs_lru) >= PFL_MEMGUARD_LINGERSZ) {
			pma = pll_get(&psc_memallocs_lru);
			_psc_lru_sysfree(pma);
		}
		PLL_ULOCK(&psc_memallocs_lru);
	}
#  endif

	/*
	 * Since PAGEALIGN'd allocations must end on a page-boundary,
	 * there is no guarentee they can begin on one, so we must force
	 * "guard-before" mode.
	 */
//	if (flags & PAF_PAGEALIGN | PAF_LOCK)
	if (flags & PAF_PAGEALIGN)
		guard_after = 0;

	if (oldp && (flags & PAF_NOGUARD) == 0) {
		pma = _psc_getpma(oldp);

		/* XXX if CANFAIL occurs, restore protections. */
		pscmem_checkinnerguard(pma);
		psc_mprotect(pma->pma_allocbase, PMA_LEN(pma),
		    PROT_READ | PROT_WRITE);
		oldp = pma->pma_allocbase;
		oldlen = pma->pma_userlen;
	}
#endif

	if (oldp && (flags & PAF_PAGEALIGN))
		errx(1, "realloc of page-aligned is not implemented");

	/*
	 * Disallow realloc(p, sz, PAF_LOCK) because we can't munlock
	 * the old region as we don't have the size.
	 */
	if ((flags & PAF_LOCK) && oldp)
		// errx()
		psc_fatalx("unable to realloc mlocked mem");

	/*
	 * If the new allocation size is 0, treat it like a free and
	 * return NULL.  The caller should be able to do anything he
	 * wants with NULL in place of a zero-length pointer as this
	 * behavior differs on systems.
	 */
	if (oldp)
		psc_assert(size);

#if PFL_DEBUG > 1
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
		rc = posix_memalign(&newp, psc_pagesize, size);
		if (rc) {
			errno = rc;
			newp = NULL;
#if PFL_DEBUG > 1
		} else if (oldp) {
 movepage:
			/*
			 * To support protected PAGEALIGN'd allocations,
			 * since realloc() does not work with
			 * posix_memalign() such that a page-aligned
			 * base is guarenteed to be returned, fabricate
			 * a realloc by issuing a new page-aligned
			 * alloc, copying, and freeing the old chunk.
			 */
			if (guard_after)
				memmove((char *)newp + (psc_pagesize -
				    specsize % psc_pagesize) % psc_pagesize,
				    pma->pma_userbase, pma->pma_userlen);
			else
				memmove((char *)newp +
				    (pma->pma_userbase - pma->pma_allocbase),
				    pma->pma_userbase, pma->pma_userlen);
			if (size != PMA_LEN(pma)) {
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
			void psc_pool_reapmem(size_t);

			psc_pool_reapmem(size);
			flags |= PAF_NOREAP;
			goto retry;
		}
		if (flags & PAF_CANFAIL) {
			psclog_error("malloc/realloc");
			return (NULL);
		}
		psc_fatal("Allocate failed, size = %zd", size);
	}

#if PFL_DEBUG > 1
	if ((flags & PAF_NOGUARD) == 0) {
		int rem;

		if (oldp == NULL) {
			/*
			 * XXX consider using guard region for this to
			 * save on two malloc overhead or pools.
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
			psc_mprotect((char *)newp + size - psc_pagesize,
			    psc_pagesize, PROT_NONE);
		} else {
			pma->pma_userbase = (char *)newp + psc_pagesize;
			pma->pma_guardbase = (char *)pma->pma_userbase +
			    specsize;
			psc_mprotect(newp, psc_pagesize, PROT_NONE);
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
			psclog_error("mlock");
			errno = save_errno;
			return (NULL);
		}
		psc_fatal("mlock");
	}
	if (oldp == NULL && (flags & PAF_NOZERO) == 0)
		memset(newp, 0, size);
	return (newp);
}

/*
 * Allocate zeroed memory for an array.
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

char *
pfl_strndup(const char *str, size_t len)
{
	char *p;

	p = PSCALLOC(len);
	strlcpy(p, str, len);
	return (p);
}

/*
 * Duplicate a string, allocating memory as necessary for duplicate.
 * @str: original string to duplicate.
 *
 * Advantages of using this over strdup(2) are ties into reaping pool
 * hogs and logging.
 */
char *
pfl_strdup(const char *str)
{
	return (pfl_strndup(str, strlen(str) + 1));
}

#if PFL_DEBUG > 1 && PFL_MEMGUARD_LINGERSZ
/*
 * The user wants to release a chunk of memory.  Instead of releasing it
 * back into the system, set PROT_NONE on it and keep it around for
 * awhile.
 *
 * @p: memory base.
 * @flags: flags the region was allocated with.
 */
void
_psc_lru_userfree(void *p, int flags, ...)
{
	struct psc_memalloc *pma;

	if (flags & PAF_LOCK) {
		size_t len;
		va_list ap;

		va_start(ap, flags);
		len = va_arg(ap, size_t);
		va_end(ap);

		if (munlock(p, len) == -1)
			psc_fatal("munlock %p", p);
	}

	pma = _psc_getpma(p);
	pscmem_checkinnerguard(pma);
	psc_mprotect(pma->pma_allocbase, PMA_LEN(pma), PROT_NONE);
	INIT_PSC_LISTENTRY(&pma->pma_lru_lentry);
	pll_add(&psc_memallocs_lru, pma);
}

/*
 * OK, no more games, the system needs the memory back so release it for
 * real.
 *
 * @p: memory base.
 * @flags: flags the region was allocated with.
 */
void
_psc_lru_sysfree(struct psc_memalloc *pma)
{
	void *p;

	p = pma->pma_allocbase;
	psc_mprotect(pma->pma_allocbase, PMA_LEN(pma),
	    PROT_READ | PROT_WRITE);
	free(pma);
	free(p);
}
#else
/*
 * Counterpart to psc_alloc (unless LINGER is enabled): release memory.
 * @p: memory base.
 * @flags: flags the region was allocated with.
 */
void
_psc_free(void *p, int flags, ...)
{
	va_list ap;

	if (flags & PAF_LOCK) {
		size_t len;

		va_start(ap, flags);
		len = va_arg(ap, size_t);
		va_end(ap);

		if (munlock(p, len) == -1)
			psc_fatal("munlock %p", p);
	}

#if PFL_DEBUG > 1
	if ((flags & PAF_NOGUARD) == 0) {
		struct psc_memalloc *pma;

		pma = _psc_getpma(p);
		p = pma->pma_allocbase;
		pscmem_checkinnerguard(pma);
		psc_mprotect(pma->pma_allocbase, PMA_LEN(pma),
		    PROT_READ | PROT_WRITE);
		free(pma);
	}
#  endif

	free(p);
}
#endif

#define PFL_DEF_NMEMMAPS	(1024 * 1024)
#define _PATH_MAX_MEMMAPS	"/proc/sys/vm/max_map_count"

void
psc_memallocs_init(void)
{
#if PFL_DEBUG > 1
	char *p, *endp, *fn = _PATH_MAX_MEMMAPS;
	long val, nmaps;
	FILE *fp;

	psc_hashtbl_init(&psc_memallocs, PHTF_NOMEMGUARD | PHTF_NOLOG,
	    struct psc_memalloc, pma_userbase, pma_hentry, 3067, NULL,
	    "memallocs");

	fp = fopen(fn, "rw");
	if (fp) {
		nmaps = PFL_DEF_NMEMMAPS;
		p = getenv("PSC_MAX_NMEMMAPS");
		if (p) {
			val = strtol(p, &endp, 10);
			if (val < 0 || val > INT_MAX ||
			    endp == p || *endp != '\0')
				warnx("invalid env PSC_MAX_NMEMMAPS: %s", p);
			else
				nmaps = val;
		}

		if (fscanf(fp, "%ld", &val) != 1) {
			warnx("read %s", fn);
			goto bail;
		}

		errno = 0;
		rewind(fp);
		if (errno) {
			warnx("rewind %s", fn);
			goto bail;
		}

		if (val < nmaps) {
			if (fprintf(fp, "%ld", nmaps) != 1)
				warn("error writing %s", fn);
		}
 bail:
		fclose(fp);
	}
#endif
}
