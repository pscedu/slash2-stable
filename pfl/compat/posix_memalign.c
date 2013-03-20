/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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
 * the value to psc_free_aligned().
 */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "pfl/compat/posix_memalign.h"
#include "psc_util/alloc.h"
#include "psc_util/bitflag.h"

/**
 * posix_memalign - An overrideable aligned memory allocator for systems
 *	which do not support posix_memalign(3).
 * @p: value-result pointer to memory.
 * @alignment: alignment size, must be power-of-two.
 * @size: amount of memory to allocate.
 */
int
posix_memalign(void **p, size_t alignment, size_t size)
{
	void *startp;

errno = ENOTSUP;
err(1, "posix_memalign");

	if (pfl_bitstr_nset(&alignment, sizeof(alignment)) != 1)
		errx(1, "posix_memalign: %zu: bad alignment size, "
		    "must be power of two (%x bits)",
		    size, pfl_bitstr_nset(&alignment, sizeof(alignment)));

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
 * psc_free_aligned - Free aligned memory.
 * @p: memory chunk to free.
 */
void
psc_freen(void *p)
{
	/* XXX recover original pointer value. */
	PSCFREE(p);
}
