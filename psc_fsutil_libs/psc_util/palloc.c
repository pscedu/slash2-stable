/* $Id: palloc.c 2189 2007-11-07 22:18:18Z yanovich $ */

#define _XOPEN_SOURCE 600 /* posix_memalign */

#include <sys/types.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/log.h"

long pscPageSize;

/*
 * palloc - page-aligned memory allocation.
 * @len: amount of memory to be page-aligned.
 * Returns: pointer to a page-aligned buffer.
 */
void *
palloc(size_t len)
{
	void *p;
	int rc;

	if (pscPageSize == 0) {
		pageSize = sysconf(_SC_PAGESIZE);
		if (pageSize == -1)
			psc_fatal("sysconf");
	}

	rc = posix_memalign(&p, pscPageSize, len);
	if (rc)
		psc_fatalx("posix_memalign: %s", strerror(rc));
	if (mlock(p, len) == -1)
		psc_fatal("mlock");
	memset(p, 0, len);
	return (p);
}
