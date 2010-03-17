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

/*
 * Implementation of posix_memalign(3).
 *
 * XXX: need to track the original pointer and pass that as
 * the value to free(3).
 */

#include <sys/param.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "psc_util/alloc.h"
#include "psc_util/bitflag.h"
#include "psc_util/log.h"

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

psc_fatalx("broken");
	if (psc_countbits(alignment) != 1)
		psc_fatalx("%zu: bad alignment size, must be power of two (%x bits)",
		    size, psc_countbits(alignment));

	size += alignment;
	startp = malloc(size);
	if (startp == NULL)
		return (errno);
	/* XXX track original pointer value somewhere. */
	/* Align to boundary. */
	*p = (void *)(((unsigned long)startp) & ~(alignment - 1));
	if (*p != startp)
		*p += alignment;
	return (0);
}

/*
 * psc_freen - Free aligned memory.
 * @p: memory chunk to free.
 */
__weak void
psc_freen(void *p)
{
	/* XXX recover original pointer value. */
	PSCFREE(p);
}
