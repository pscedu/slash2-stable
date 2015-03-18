/* $Id$ */
/* %PSC_COPYRIGHT% */

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
