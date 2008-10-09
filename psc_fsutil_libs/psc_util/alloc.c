/* $Id$ */

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

__weak void
_psc_pool_reap(void)
{
}

void *
psc_alloc(size_t size, int flags)
{
	int rc, save_errno;
	void *p;

 retry:
	if (flags & PAF_PAGEALIGN) {
		rc = posix_memalign(&p, pscPageSize, size);
		if (rc) {
			errno = rc;
			p = NULL;
		}
	} else
		p = malloc(size);
	if (p == NULL) {
		if ((flags & PAF_NOREAP) == 0) {
			_psc_pool_reap();
			flags &= ~PAF_NOREAP;
			goto retry;
		}
		if (flags & PAF_CANFAIL) {
			psc_error("malloc");
			return (NULL);
		}
		psc_fatal("malloc");
	}
	if ((flags & PAF_LOCK) && mlock(p, size) == -1) {
		if (flags & PAF_CANFAIL) {
			save_errno = errno;
			PSCFREE(p);
			psc_error("mlock");
			errno = save_errno;
			return (NULL);
		}
		psc_fatal("mlock");
	}
	memset(p, 0, size);
	return (p);
}

void *
psc_calloc(size_t num, size_t size)
{
	if (num && SIZE_MAX / num < size) {
		errno = ENOMEM;
		return (NULL);
	}
	return (psc_alloc(size * num, PAF_CANFAIL));
}

void
psc_freel(void *p, size_t size)
{
	if (p && munlock(p, size) == -1)
		psc_fatal("munlock %p", p);
	PSCFREE(p);
}
