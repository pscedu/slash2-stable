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

#include "pfl/cdefs.h"
#include "pfl/vbitmap.h"
#include "psc_util/alloc.h"
#include "psc_util/bitflag.h"
#include "psc_util/log.h"

#define VB_CLEAR_UNALLOC(vb)						\
	do {								\
		if ((vb)->vb_lastsize && (vb)->vb_lastsize != NBBY)	\
			*(vb)->vb_end &= ~(0xff << (vb)->vb_lastsize);	\
	} while (0)

/**
 * psc_vbitmap_newf - Create a new variable-sized bitmap.
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

/**
 * psc_vbitmap_attach - Initialize a variable bitmap from a chunk of memory.
 * @buf: memory where to read bitmap from.
 * @size: length of memory buffer.
 */
struct psc_vbitmap *
psc_vbitmap_attach(unsigned char *buf, size_t size)
{
	struct psc_vbitmap *vb;

	vb = PSCALLOC(sizeof(*vb));
	vb->vb_flags |= PVBF_EXTALLOC;
	vb->vb_pos = vb->vb_start = buf;
	vb->vb_end = buf + (size - 1); /* XXX does not handle 0 */
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

/**
 * psc_vbitmap_setval - Set the value of a bit in a vbitmap.
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

/**
 * psc_vbitmap_setval_range - Set a range bits of a vbitmap.
 * @vb: variable bitmap.
 * @pos: starting position to set.
 * @size: length of region (# of bits) to set.
 * @set: true or false on whether to set or unset the values.
 */
int
psc_vbitmap_setval_range(struct psc_vbitmap *vb,
    size_t pos, size_t size, int set)
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
		if (set)
			*p++ |= ~(0xff << len) << shft;
		else
			*p++ &= (0xff << (len + shft)) |
			    (0xff >> (NBBY - shft));
		size -= len;
	}

	/* change whole bytes */
	if (set)
		for (; size >= 8; p++, size -= 8)
			*p = 0xff;
	else
		for (; size >= 8; p++, size -= 8)
			*p = 0;

	/* change bits in last byte */
	if (size) {
		if (set)
			*p |= 0xff >> (NBBY - size);
		else
			*p &= 0xff >> (NBBY - size);
	}
	return (0);
}

/**
 * psc_vbitmap_get - Get bit value for an element of a vbitmap.
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

/**
 * psc_vbitmap_nfree - Get the number of free (i.e. unset) bits
 *	in a variable bitmap.
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

/**
 * psc_vbitmap_invert - Invert the state of all bits in a vbitmap.
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

/**
 * psc_vbitmap_setall - Toggle on all bits in a vbitmap.
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

/**
 * psc_vbitmap_clearall - Toggle off all bits in a vbitmap.
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

/**
 * psc_vbitmap_isfull - Determine if there are any
 *	empty slots in a vbitmap.
 * @vb: variable bitmap.
 */
int
psc_vbitmap_isfull(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p < vb->vb_end; p++)
		if (*p != 0xff)
			return (0);
	if (vb->vb_lastsize == 0)
		return (1);
	return (ffs(~*p) > vb->vb_lastsize);
}

/**
 * psc_vbitmap_lcr - Report the largest contiguous region in the bitmap.
 * @vb: variable bitmap.
 * Returns: size of the region.
 */
int
psc_vbitmap_lcr(struct psc_vbitmap *vb)
{
	unsigned char *p;
	int i, n=0, r=0;

	VB_CLEAR_UNALLOC(vb);
	for (p = vb->vb_start; p < vb->vb_end ||
	    (p == vb->vb_end && vb->vb_lastsize); p++) {
		if (*p == 0x00)
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

/**
 * psc_vbitmap_getncontig - Try to get 'N' contiguous slots (or bits)
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

/**
 * psc_vbitmap_next - Return next unused slot (a bit with value of 0) from a vbitmap.
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
			pos = vb->vb_start;	/* byte is full, advance */
		} else {
			if (*pos != 0xff)
				goto found;
			pos++;			/* byte is full, advance */
		}
	} while (pos != start);

	if ((vb->vb_flags & (PVBF_AUTO | PVBF_EXTALLOC)) == PVBF_AUTO) {
		int newsiz;

		newsiz = psc_vbitmap_getsize(vb) + 1;
		/* XXX allocate some extra slack here too? */
		if (psc_vbitmap_resize(vb, newsiz) == -1)
			return (-1);
		psc_vbitmap_setnextpos(vb, newsiz - 1);
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

/**
 * psc_vbitmap_setnextpos - Set position where psc_vbitmap_next() looks
 *	for next unset bit.
 * @vb: variable bitmap.
 * @pos: bit position where searching will continue from.
 */
void
psc_vbitmap_setnextpos(struct psc_vbitmap *vb, size_t pos)
{
	psc_assert(pos < psc_vbitmap_getsize(vb));
	vb->vb_pos = vb->vb_start + pos / NBBY;
}

/**
 * psc_vbitmap_resize - Resize a bitmap.
 * @vb: variable bitmap.
 * @newsize: new size the bitmap should take on.
 */
int
psc_vbitmap_resize(struct psc_vbitmap *vb, size_t newsize)
{
	ptrdiff_t pos, osiz;
	size_t nsiz;

	pos = vb->vb_pos - vb->vb_start;
	osiz = vb->vb_end - vb->vb_start;

	nsiz = howmany(newsize, NBBY);
	if (vb->vb_start && nsiz == (size_t)osiz + howmany(vb->vb_lastsize, NBBY))
		/* resizing inside a byte; no mem alloc changes necessary */
		VB_CLEAR_UNALLOC(vb);
	else {
		void *start;

		start = psc_realloc(vb->vb_start, nsiz, 0);

		/* special case for resizing NULL vbitmaps */
		if (vb->vb_start == NULL)
			memset(start, 0, nsiz);
		vb->vb_start = start;
		if (nsiz)
			vb->vb_end = vb->vb_start + nsiz - 1;
		else
			vb->vb_end = vb->vb_start;
	}
	vb->vb_lastsize = newsize % NBBY;
	vb->vb_pos = vb->vb_start + pos;
	if (vb->vb_pos > vb->vb_end)
		vb->vb_pos = vb->vb_start;

	/* Initialize new sections of the bitmap to zero. */
	if (nsiz > (size_t)osiz)
		memset(vb->vb_start + osiz + 1, 0, nsiz - osiz - 1);
	if (newsize && vb->vb_lastsize == 0)
		vb->vb_lastsize = NBBY;
	return (0);
}

/**
 * psc_vbitmap_getsize - Get the number of elements a bitmap represents.
 * @vb: variable bitmap.
 */
size_t
psc_vbitmap_getsize(const struct psc_vbitmap *vb)
{
	return ((vb->vb_end - vb->vb_start) * NBBY + vb->vb_lastsize);
}

/**
 * psc_vbitmap_printbin - Print the contents of a bitmap in binary.
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

/**
 * psc_vbitmap_printhex - Print the contents of a bitmap in hexadecimal.
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

/**
 * psc_vbitmap_getstats - Gather the statistics of a bitmap.
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

/**
 * psc_vbitmap_printstats - Print the statistics of a bitmap.
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
