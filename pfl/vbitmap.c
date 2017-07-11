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
 * Variable-sized bitmaps.  Internally, bitmaps are arrays
 * of chars that are realloc(3)'d to different lengths.
 *
 * This API is not thread-safe!
 */

#include <sys/param.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pfl/alloc.h"
#include "pfl/bitflag.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"
#include "pfl/vbitmap.h"

#define VB_CLEAR_UNALLOC(vb)						\
	do {								\
		if ((vb)->vb_lastsize && (vb)->vb_lastsize != NBBY)	\
			*(vb)->vb_end &= ~(0xff << (vb)->vb_lastsize);	\
	} while (0)

/*
 * Create a new variable-sized bitmap.
 * @nelems: number of entries in bitmap.
 * @flags: operational flags.
 */
struct psc_vbitmap *
psc_vbitmap_newf(size_t nelems, int flags)
{
	struct psc_vbitmap *vb;
	size_t bytes;

	vb = PSCALLOC(sizeof(*vb));
	vb->vb_flags = flags;

	bytes = howmany(nelems, NBBY);
	/*
 	 * Note that if bytes is zero, vb_start can still return
 	 * a non-NULL value, which should be used to call free().
 	 */
	vb->vb_start = PSCALLOC(bytes);
	vb->vb_pos = vb->vb_start;
	if (bytes)
		vb->vb_end = vb->vb_start + bytes - 1;
	else
		vb->vb_end = vb->vb_start;
	vb->vb_lastsize = nelems % NBBY;
	if (nelems && vb->vb_lastsize == 0)
		vb->vb_lastsize = NBBY;
	return (vb);
}

void
_psc_vbitmap_free(struct psc_vbitmap *vb)
{
	if ((vb->vb_flags & PVBF_EXTALLOC) == 0)
		PSCFREE(vb->vb_start);
	vb->vb_start = NULL;
	vb->vb_end = NULL;
	vb->vb_pos = NULL;
	if ((vb->vb_flags & PVBF_STATIC) == 0)
		PSCFREE(vb);
}

/*
 * Set the value of a bit in a vbitmap.
 * @vb: variable bitmap.
 * @pos: position to set.
 * @set: value bit should take on.
 * Returns the previous value of the bit in the specified position.
 */
int
psc_vbitmap_setval(struct psc_vbitmap *vb, size_t pos, int set)
{
	size_t shft, bytes;
	int oldval;

	psc_assert(pos < psc_vbitmap_getsize(vb));

	bytes = pos / NBBY;
	shft = pos % NBBY;
	oldval = (vb->vb_start[bytes] >> shft) & 1;
	if (set)
		vb->vb_start[bytes] |= (1 << shft);
	else
		vb->vb_start[bytes] &= ~(1 << shft);
	return (oldval);
}

/*
 * Set a range bits of a vbitmap.
 * @vb: variable bitmap.
 * @pos: starting position to set.
 * @size: length of region (# of bits) to set.
 * @val: true or false on whether to set or unset the values.
 */
int
psc_vbitmap_setval_range(struct psc_vbitmap *vb,
    size_t pos, size_t size, int val)
{
	size_t shft, bytes, len;
	unsigned char *p;

	if (pos + size > psc_vbitmap_getsize(vb))
		return (EINVAL);

	bytes = pos / NBBY;
	shft = pos % NBBY;

	p = &vb->vb_start[bytes];

	/* change bits in first byte */
	if (shft) {
		len = MIN(size, NBBY - shft);
		if (val)
			*p++ |= ~(0xff << len) << shft;
		else
			*p++ &= (0xff << (len + shft)) |
			    (0xff >> (NBBY - shft));
		size -= len;
	}

	/* change whole bytes */
	if (val)
		for (; size >= 8; p++, size -= 8)
			*p = 0xff;
	else
		for (; size >= 8; p++, size -= 8)
			*p = 0;

	/* change bits in last byte */
	if (size) {
		if (val)
			*p |= 0xff >> (NBBY - size);
		else
			*p &= 0xff >> (NBBY - size);
	}
	return (0);
}

/*
 * Get bit value for an element of a vbitmap.
 * @vb: variable bitmap.
 * @pos: element # to get.
 */
int
psc_vbitmap_get(const struct psc_vbitmap *vb, size_t pos)
{
	size_t shft, bytes;

	psc_assert(pos < psc_vbitmap_getsize(vb));

	bytes = pos / NBBY;
	shft = pos % NBBY;
	return ((vb->vb_start[bytes] & (1 << shft)) >> shft);
}

/*
 * Get the number of free (i.e. unset) bits in a variable bitmap.
 * @vb: variable bitmap.
 * Returns: number of free bits.
 */
int
psc_vbitmap_nfree(const struct psc_vbitmap *vb)
{
	unsigned char *p;
	int n, j;

	for (n = 0, p = vb->vb_start; p <= vb->vb_end; p++)
		for (j = 0; j < (p == vb->vb_end ?
		    vb->vb_lastsize : NBBY); j++)
			if ((*p & (1 << j)) == 0)
				n++;
	return (n);
}

/*
 * Invert the state of all bits in a vbitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_invert(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p < vb->vb_end ||
	    (p == vb->vb_end && vb->vb_lastsize); p++)
		*p = ~(*p);
}

/*
 * Toggle on all bits in a vbitmap.
 * XXX this should just be a call to setval_range()
 * @vb: variable bitmap.
 */
void
psc_vbitmap_setall(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p < vb->vb_end ||
	    (p == vb->vb_end && vb->vb_lastsize); p++)
		*p = 0xff;
}

/*
 * Toggle off all bits in a vbitmap.
 * XXX this should just be a call to setval_range()
 * @vb: variable bitmap.
 */
void
psc_vbitmap_clearall(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p < vb->vb_end ||
	    (p == vb->vb_end && vb->vb_lastsize); p++)
		*p = 0;
}

/*
 * Determine if there are any empty slots in a range.
 * @vb: variable bitmap.
 */
int
pfl_vbitmap_israngeset(struct psc_vbitmap *vb, int val,
    size_t start, size_t len)
{
	unsigned char *p, fv;
	size_t adj;

	if (len == 0)
		return (1);

	fv = val ? 0xff : 0;
	psc_assert(start < psc_vbitmap_getsize(vb));
	psc_assert(start + len > start);
	psc_assert(start + len <= psc_vbitmap_getsize(vb));
	p = vb->vb_start;
	p += start / NBBY;
	adj = start & 7;
	if (adj) {
		int mask;

		mask = ((1 << MIN(len, NBBY)) - 1) << adj;
		if ((*p & mask) != (fv & mask))
			return (0);
		if (len < NBBY)
			return (1);

		if (adj > len)
			len = 0;
		else
			len -= NBBY - adj;
		p++;
	}
	// XXX use native word size for speed
	for (; p < vb->vb_end && len >= NBBY; p++, len -= 8)
		if (*p != fv)
			return (0);
	if (len == 0)
		return (1);

	psc_assert(len <= 8);

	/* Check last byte. */
	if (fv) {
		if ((0xff & (*p | (0xff << len))) != 0xff)
			return (0);
	} else {
		if (*p & ~(0xff << len))
			return (0);
	}
	return (1);
}

/*
 * Report the largest contiguous region in the bitmap.
 * @vb: variable bitmap.
 * Returns: size of the region.
 */
int
psc_vbitmap_lcr(struct psc_vbitmap *vb)
{
	unsigned char *p;
	int i, n = 0, r = 0;

	VB_CLEAR_UNALLOC(vb);
	for (p = vb->vb_start; p < vb->vb_end ||
	    (p == vb->vb_end && vb->vb_lastsize); p++) {
		if (*p == 0)
			n += NBBY;
		else if (*p == 0xff) {
			if (n > r)
				r = n;
			n = 0;
		} else {
			for (i = 0; i < NBBY; i++) {
				if ((*p & (1 << i)) == 0)
					n++;
				else {
					if (n > r)
						r = n;
					n = 0;
				}
			}
		}
	}
	if (n > r)
		r = n;
	return (r);
}

/*
 * Try to get 'N' contiguous slots (or bits)
 * @vb: variable bitmap.
 * @nslots:  as an input parameter, requests 'N' number of slots.
 *	On output, informs the caller of the starting slot.
 * Returns: number of slots assigned, 0 for none.
 *
 * XXX: adjust this to take into account vb_lastsize.
 */
int
psc_vbitmap_getncontig(struct psc_vbitmap *vb, int *nslots)
{
	unsigned char *p;
	int i=0, sbit=0, ebit=0, t1=0, t2=0;

#define TEST_AND_SET					\
	do {						\
		if ((t2 - t1) > (ebit - sbit)) {	\
			sbit = t1;			\
			ebit = t2;			\
		}					\
		t1 = t2 + 1;				\
	} while (0)

	for (p = vb->vb_start; p <= vb->vb_end; p++)
		for (i = 0; i < (p == vb->vb_end ?
		    vb->vb_lastsize : NBBY); i++, t2++) {
			if (*p & (1 << i))
				TEST_AND_SET;
			else if (t2 - t1 == *nslots)
				goto mark_bits;
		}
 mark_bits:
	TEST_AND_SET;

	if (ebit - sbit) {
		for (i = sbit; i < ebit; i++)
			vb->vb_start[i / NBBY] |= 1 << (i % NBBY);
		/* Inform the caller of the start bit */
		*nslots = sbit;
	}
	return (ebit - sbit);
}

/*
 * Return next unused slot (a bit with value of 0) from a vbitmap.
 * @vb: variable bitmap.
 * @elem: pointer to element#.
 * Returns: Boolean true on success, false on failure.
 */
int
psc_vbitmap_next(struct psc_vbitmap *vb, size_t *elem)
{
	unsigned char *start, *pos;
	int bytepos;

 retry:
	pos = start = vb->vb_pos;
	do {
		/* Check if byte is completely full. */
		if (pos == vb->vb_end) {
			if (vb->vb_lastsize == NBBY) {
				if (*pos != 0xff)
					goto found;
			} else if (pos && *pos != ~(char)(0x100 -
			    (1 << vb->vb_lastsize)))
				goto found;
			pos = vb->vb_start;	/* last byte is full, wrap around */
		} else {
			if (*pos != 0xff)
				goto found;
			pos++;			/* current byte is full, advance */
		}
	} while (pos != start);

	if ((vb->vb_flags & (PVBF_AUTO | PVBF_EXTALLOC)) == PVBF_AUTO) {
		size_t newsiz;

		if (psc_vbitmap_getsize(vb))
			newsiz = 2 * psc_vbitmap_getsize(vb);
		else
			newsiz = 128;
		if (psc_vbitmap_resize(vb, newsiz) == -1)
			return (-1);
		goto retry;
	}
	return (0);

 found:
	/* We now have a byte from the bitmap that has a zero. */
	vb->vb_pos = pos;
	bytepos = ffs(~*pos) - 1;
	*pos |= 1 << bytepos;
	*elem = NBBY * (pos - vb->vb_start) + bytepos;
	return (1);
}

/*
 * Set position where psc_vbitmap_next() looks for next unset bit.
 * @vb: variable bitmap.
 * @pos: bit position where searching will continue from.
 */
void
psc_vbitmap_setnextpos(struct psc_vbitmap *vb, size_t pos)
{
	psc_assert(pos < psc_vbitmap_getsize(vb));
	vb->vb_pos = vb->vb_start + pos / NBBY;
}

/*
 * Resize a bitmap.
 * @vb: variable bitmap.
 * @newsize: new size the bitmap should take on.
 */
int
psc_vbitmap_resize(struct psc_vbitmap *vb, size_t newsize)
{
	size_t nsiz, osiz;
	ptrdiff_t pos;

	pos = vb->vb_pos - vb->vb_start;
	osiz = howmany(psc_vbitmap_getsize(vb), NBBY);
	nsiz = howmany(newsize, NBBY);
	if (vb->vb_start && nsiz == osiz)
		/*
		 * Resizing inside a byte; no mem alloc changes
		 * necessary.
		 */
		VB_CLEAR_UNALLOC(vb);
	else {
		/* XXX check return code ? */
		vb->vb_start = psc_realloc(vb->vb_start, nsiz, 0);
		if (nsiz)
			vb->vb_end = vb->vb_start + nsiz - 1;
		else
			vb->vb_end = vb->vb_start;

		/* Initialize new sections of the bitmap to zero. */
		if (nsiz > (size_t)osiz)
			memset(vb->vb_start + osiz, 0, nsiz - osiz);
	}
	vb->vb_lastsize = newsize % NBBY;
	vb->vb_pos = vb->vb_start + pos;
	if (vb->vb_pos > vb->vb_end)
		vb->vb_pos = vb->vb_start;

	if (newsize && vb->vb_lastsize == 0)
		vb->vb_lastsize = NBBY;
	return (0);
}

/*
 * Get a string of the vbitmap in binary encoding.
 */
char *
pfl_vbitmap_getbinstring(const struct psc_vbitmap *vb)
{
	unsigned char *p;
	char *str, *t;
	ptrdiff_t len;
	int i;

	len = psc_vbitmap_getsize(vb) + 1;
	str = PSCALLOC(len);
	for (p = vb->vb_start, t = str; p < vb->vb_end; p++)
		for (i = 0; i < NBBY; i++) {
			*t++ = ((*p >> i) & 1) ? '1' : '0';
			if (t - str >= len)
				goto done;
		}
	for (i = 0; i < vb->vb_lastsize && t - str < len; i++)
		*t++ = ((*p >> i) & 1) ? '1' : '0';

 done:
	*t = '\0';
	return (str);
}

/*
 * Get an abbreviated binary representation of a vbitmap.
 */
char *
pfl_vbitmap_getabbrbinstring(const struct psc_vbitmap *vb)
{
	int i, runlen = 0, n, thisval, lastval = *vb->vb_start & 1;
	unsigned char *p;
	char *str, *t;
	ptrdiff_t len;

	len = psc_vbitmap_getsize(vb) + 1;
	str = PSCALLOC(len);
	for (p = vb->vb_start, t = str; p < vb->vb_end; p++) {
		if ((*p == 0 || *p == 0xff) &&
		    (*p & lastval) == lastval) {
			runlen += NBBY;
			continue;
		}
		for (i = 0; i < NBBY; i++) {
			thisval = (*p >> i) & 1;
			if (thisval != lastval) {
				*t++ = lastval ? '1' : '0';
				lastval = thisval;
				if (runlen > 3) {
					n = snprintf(t, len - (t - str),
					    ":%d,", runlen);
					psc_assert(n != -1);
					t += n;
				}
				runlen = 0;
				if (t - str >= len)
					goto done;
			}
			runlen++;
		}
	}
	if (runlen) {
		*t++ = lastval ? '1' : '0';
		n = snprintf(t, len - (t - str), ":%d,", runlen);
		psc_assert(n != -1);
		t += n;
	}
	for (i = 0; i < vb->vb_lastsize && t - str < len; i++)
		*t++ = ((*p >> i) & 1) ? '1' : '0';

 done:
	*t = '\0';
	return (str);
}

/*
 * Print the contents of a bitmap in binary.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_printbin(const struct psc_vbitmap *vb)
{
	unsigned char *p;
	int j;

	for (p = vb->vb_start; p < vb->vb_end; p++) {
		printf("%d%d%d%d%d%d%d%d ",
		    (*p >> 0) & 1, (*p >> 1) & 1,
		    (*p >> 2) & 1, (*p >> 3) & 1,
		    (*p >> 4) & 1, (*p >> 5) & 1,
		    (*p >> 6) & 1, (*p >> 7) & 1);
		if (((p + 1 - vb->vb_start) % 8) == 0 ||
		    p == vb->vb_end)
			printf("\n");
	}
	for (j = 0; j < vb->vb_lastsize; j++)
		printf("%d", (*p >> j) & 1);
	if (vb->vb_lastsize)
		printf("\n");
}

/*
 * Print the contents of a bitmap in hexadecimal.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_printhex(const struct psc_vbitmap *vb)
{
	const unsigned char *p;

	for (p = vb->vb_start; p <= vb->vb_end; p++) {
		printf("%02x", *p);
		if (((p + 1 - vb->vb_start) % 32) == 0 ||
		    p == vb->vb_end)
			printf("\n");
		else if (((p + 1 - vb->vb_start) % 4) == 0 ||
		    p == vb->vb_end)
			printf(" ");
	}
}

/*
 * Gather the statistics of a bitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_getstats(struct psc_vbitmap *vb, int *used, int *total)
{
	VB_CLEAR_UNALLOC(vb);
	*used = pfl_bitstr_nset(vb->vb_start,
	    (vb->vb_lastsize ? 1 : 0) +
	    vb->vb_end - vb->vb_start);
	*total = psc_vbitmap_getsize(vb);
}

/*
 * Print the statistics of a bitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_printstats(struct psc_vbitmap *vb)
{
	int used, total;

	psc_vbitmap_getstats(vb, &used, &total);
	printf("vbitmap statistics: %d/%d (%.4f%%) in use\n", used,
	    total, 100.0 * used / total);
}
