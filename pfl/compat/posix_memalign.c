/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/alloc.h"
#include "pfl/bitflag.h"

/*
 * An overrideable aligned memory allocator for systems which do not
 * support posix_memalign(3).
 *
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
	*p = (void *)(((uintptr_t)startp) & ~(alignment - 1));
	if (*p != startp)
		*p += alignment;
	return (0);
}

/*
 * Free aligned memory.
 * @p: memory chunk to free.
 */
void
psc_freen(void *p)
{
	/* XXX recover original pointer value. */
	PSCFREE(p);
}
