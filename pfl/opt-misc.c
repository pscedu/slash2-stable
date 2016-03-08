/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2015, Pittsburgh Supercomputing Center
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
 * Compiler optimized miscellaneous routines.
 * These routines are compiled with optimization even in debug builds
 * as the code is considered to be robust...
 */

#include <sys/types.h>

#include <string.h>

#include "pfl/alloc.h"

void *
pfl_memdup(const void *p, size_t len)
{
	void *d;

	d = PSCALLOC(len);
	memcpy(d, p, len);
	return (d);
}

int
pfl_memchk(const void *buf, int val, size_t len)
{
	const char *p, *ep = PSC_AGP(buf, len);
	const uint64_t *ip;
	uint64_t ival;

	ival = ((uint64_t)val << 32) | val;
	for (ip = buf; (char *)(ip + 1) < ep; ip++)
		if (*ip != ival)
			return (0);
	for (p = (void *)ip; p < ep; p++)
		if (*p != val)
			return (0);
	return (1);
}

/*
 * "This well-known hash function was used in P.J. Weinberger's
 * C compiler (cf. Compilers: Principles, Techniques, and Tools,
 * by Aho, Sethi & Ullman, Addison-Wesley, 1988, p. 436)."
 */
uint64_t
_psc_str_hashify(const void *s, size_t len)
{
	const unsigned char *p;
	uint64_t h = 0, g;

	if (s == NULL)
		return (-1);
	for (p = s; len && *p != '\0'; p++, len--) {
		h = (h << 4) + *p;
		g = h & UINT64_C(0xf000000000000000);
		if (g) {
			h ^= g >> 56;
			h ^= g;
		}
	}
	return (h);
}
