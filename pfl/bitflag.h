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

#ifndef _PFL_BITSTR_H_
#define _PFL_BITSTR_H_

#include <stdint.h>

#include "pfl/cdefs.h"
#include "pfl/lock.h"
#include "pfl/log.h"

#define PFL_BITSTR_SETCHK_STRICT	(1 << 0)
#define PFL_BITSTR_SETCHK_ABORT		(1 << 1)

/**
 * pfl_bitstr_setchk - check and/or set bits in an integer.
 * @f: flags variable to perform operations on.
 * @lck: optional spinlock.
 * @checkon: values to ensure are enabled.
 * @checkoff: values to ensure are disabled.
 * @turnon: values to enable.
 * @turnoff: values to disable.
 * @flags: settings which dictate operation of this routine.
 * Notes: returns -1 on failure, 0 on success.
 */
static __inline int
pfl_bitstr_setchk(int *f, psc_spinlock_t *lck, int checkon,
    int checkoff, int turnon, int turnoff, int flags)
{
	int strict, locked;

	locked = 0; /* gcc */
	strict = ATTR_ISSET(flags, PFL_BITSTR_SETCHK_STRICT);

	if (lck)
		locked = reqlock(lck);

	/* check on bits */
	if (checkon &&
	    (!ATTR_HASANY(*f, checkon) ||
	     (strict && !ATTR_HASALL(*f, checkon))))
		goto error;

	/* check off bits */
	if (checkoff &&
	    (ATTR_HASALL(*f, checkoff) ||
	     (strict && ATTR_HASANY(*f, checkoff))))
		goto error;

	/* strict setting mandates turn bits be in negated state */
	if (strict &&
	    ((turnon && ATTR_HASANY(*f, turnon)) ||
	     (turnoff && ATTR_HASANY(~(*f), turnoff))))
		goto error;

	/* set on bits */
	if (turnon)
		*f |= turnon;

	/* unset off bits */
	if (turnoff)
		*f &= ~turnoff;

	if (lck)
		ureqlock(lck, locked);
	return (1);
 error:
	if (lck)
		ureqlock(lck, locked);
	psc_assert((flags & PFL_BITSTR_SETCHK_ABORT) == 0);
	return (0);
}

/**
 * pfl_bitstr_nset - count number of bits set in a bitstr.
 * @val: value to inspect.
 * @len: length (# of bytes) of region.
 */
static __inline int
pfl_bitstr_nset(const void *val, int len)
{
	const unsigned char *p;
	int n, c = 0;

	for (p = val; len > 0; p++, len--)
		for (n = 0; n < NBBY; n++)
			if (*p & (1 << n))
				c++;
	return (c);
}

/**
 * pfl_bitstr_copy - Copy bits from one place to another.
 * @dst: place to copy to.
 * @doff: offset into destination bitstring where to begin copying bits to.
 * @src: place to copy from.
 * @soff: offset into source bitstring where to begin copying bits from.
 * @nbits: length of copy.
 *
 * Note: Only overlapped cases where src+soff > dst+doff are implemented.
 *
 *	      |=========== memory ===========|
 * SUPPORTED:
 *	src:         |-----------------------|
 *	dst:  |-----------------------|
 *
 * NOT SUPPORTED:
 *	src:  |-----------------------|
 *	dst:         |-----------------------|
 *
 * XXX 32-bit architectures should use 32-bit values instead of 64-bit
 * since the wordsize will be faster.
 * 
 * Called by fnstat_prdat().
 */
static __inline void
pfl_bitstr_copy(void *dst, int doff, const void *src, int soff, int nbits)
{
	const unsigned char *in8;
	unsigned char *out8, b;
	const uint64_t *in64;
	uint64_t *out64;

	psc_assert(doff >= 0 && soff >= 0);
//	if (dst + doff  > src + soff &&
//	    dst + doff < src + soff + nbits)
//		psc_fatalx("overlap case not implemented");

	in64 = (const uint64_t *)src + soff / sizeof(*in64) / NBBY;
	out64 = (uint64_t *)dst + doff / sizeof(*out64) / NBBY;
	soff %= sizeof(*in64) * NBBY;
	doff %= sizeof(*out64) * NBBY;

	for (; nbits >= NBBY * (int)sizeof(*out64);
	    in64++, out64++, nbits -= NBBY * (int)sizeof(*out64)) {
		/* copy bits in the same byte position */
		*out64 = (*out64 & (UINT64_MAX >> ((sizeof(*out64) * NBBY) - doff))) |
		    ((*in64 >> soff) << doff);

		/* copy bits from next src position to prev dst pos */
		if (soff > doff)
			*out64 |= in64[1] << (NBBY *
			    (int)sizeof(*in64) - soff + doff);

		/* copy bits from prev src position to next dst pos */
		else if (soff < doff)
			out64[1] = (out64[1] & (UINT64_MAX << doff)) |
			    (*in64 >> (NBBY * (int)sizeof(*in64) - doff - soff));
	}

	in8 = (const unsigned char *)in64 + soff / NBBY;
	out8 = (unsigned char *)out64 + doff / NBBY;
	soff %= NBBY;
	doff %= NBBY;

	for (; nbits >= NBBY; in8++, out8++, nbits -= NBBY) {
		*out8 = (*out8 & (0xff >> (NBBY - doff))) |
		    ((*in8 >> soff) << doff);

		if (soff > doff)
			*out8 |= in8[1] << (NBBY - soff + doff);
		else if (soff < doff)
			out8[1] = (out8[1] & (0xff << doff)) |
			    (*in8 >> (NBBY - doff - soff));
	}
	if (nbits) {
		b = ((*in8 >> soff) & ~(0xff << nbits)) << doff;
		b |= *out8 & (0xff >> (NBBY - doff));
		if (nbits < NBBY - doff)
			b |= *out8 & ~(0xff >> (NBBY - nbits));
		*out8 = b;
		nbits -= NBBY - doff;
	}
	if (nbits > 0) {
		if (soff > doff)
			*out8 |= (in8[1] & ~(0xff << nbits)) <<
			    (NBBY - soff + doff);
		else if (soff < doff)
			out8[1] = (out8[1] & (0xff << (nbits))) |
			    ((*in8 >> (NBBY - doff - soff)) & ~(0xff << nbits));
	}
}

#endif /* _PFL_BITSTR_H_ */
