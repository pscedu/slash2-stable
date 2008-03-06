/* $Id$ */

#include <sys/param.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "psc_ds/vbitmap.h"
#include "psc_util/cdefs.h"

/**
 * vbitmap_new - create a new variable-sized bitmap.
 * @nelems: number of entries in bitmap.
 */
struct vbitmap *
vbitmap_new(size_t nelems)
{
	struct vbitmap *vb;
	size_t bytes;

	if ((vb = malloc(sizeof(*vb))) == NULL)
		return (NULL);
	memset(vb, 0, sizeof(*vb));

	bytes = howmany(nelems, NBBY);
	vb->vb_start = calloc(bytes, 1);
	if (vb->vb_start == NULL && bytes) {
		free(vb);
		return (NULL);
	}
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
 * vbitmap_free - reclaim memory from a variable-sized bitmap.
 * @vb: pointer to bitmap.
 */
void
vbitmap_free(struct vbitmap *vb)
{
	free(vb->vb_start);
	vb->vb_start = NULL;
	vb->vb_end = NULL;
	vb->vb_pos = NULL;
}

/**
 * vbitmap_unset - unset bit for an element of a variable-sized bitmap.
 * @vb: pointer to bitmap.
 * @elem: element# to unset.
 */
void
vbitmap_unset(struct vbitmap *vb, size_t elem)
{
	size_t pos, bytes;

	bytes = elem / NBBY;
	pos = elem % NBBY;
	vb->vb_start[bytes] &= ~(1 << pos);
}

/**
 * vbitmap_set - set bit for an element of a variable-sized bitmap.
 * @vb: pointer to bitmap.
 * @elem: element# to set.
 */
void
vbitmap_set(struct vbitmap *vb, size_t elem)
{
	size_t pos, bytes;

	bytes = elem / NBBY;
	pos = elem % NBBY;
	vb->vb_start[bytes] |= (1 << pos);
}

/**
 * vbitmap_get - get bit for an element of a variable-sized bitmap.
 * @vb: pointer to bitmap.
 * @elem: element # to get.
 */
int
vbitmap_get(const struct vbitmap *vb, size_t elem)
{
	size_t pos, bytes;

	bytes = elem / NBBY;
	pos = elem % NBBY;
	return (vb->vb_start[bytes] & (1 << pos));
}

__static __inline int
bs_nfree(int b, int m)
{
	int i, n;

	if (b == 0)
		return (NBBY);
	if (b == 0xff)
		return (0);
	for (i = n = 0; i < m; i++)
		if ((b & (1 << i)) == 0)
			n++;
	return (n);
}

/**
 * vbitmap_nfree - report the number of free bits in the bitmap.
 * @vb: pointer to bitmap.
 * Returns: number of free bits.
 */
int
vbitmap_nfree(const struct vbitmap *vb)
{
	unsigned char *p;
	int n;

	for (n = 0, p = vb->vb_start; p < vb->vb_end; p++)
		n += bs_nfree(*p, NBBY);
	n += bs_nfree(*p, vb->vb_lastsize);
	return (n);
}

/**
 * vbitmap_lcr - report the largest contiguous region in the bitmap.
 * @vb: pointer to bitmap.
 * Returns: size of the region.
 */
int
vbitmap_lcr(const struct vbitmap *vb)
{
	unsigned char *p;
	int i, n=0, r=0;

	for (p = vb->vb_start; p <= vb->vb_end; p++)
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
	if (n > r)
		r = n;
	return (r);
}

/**
 * vbitmap_getncontig - try to get 'N' contiguous slots (or bits)
 * @vb: pointer to bitmap.
 * @nslots:  as an input parameter, requests 'N' number of slots.
 *	On output, informs the caller of the starting slot.
 * Returns: number of slots assigned, 0 for none.
 */
int
vbitmap_getncontig(struct vbitmap *vb, int *nslots)
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
		for (i = 0; i < NBBY; i++, t2++) {
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
 * vbitmap_next - return next unused slot from a variable-sized bitmap.
 * @vb: pointer to bitmap.
 * @elem: pointer to element#.
 * Returns: true on success.
 */
int
vbitmap_next(struct vbitmap *vb, size_t *elem)
{
	unsigned char *start, *pos;
	int bytepos;

	pos = start = vb->vb_pos;
	do {
		/* Check if byte is completely full. */
		if (*pos != 0xff)
			goto found;

		/* Byte is full, advance. */
		if (pos == vb->vb_end)
			pos = vb->vb_start;
		else
			pos++;
	} while (pos != start);
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
 * vbitmap_resize - resize a bitmap.
 * @vb: pointer to bitmap.
 * @newsize: new size the bitmap should take on.
 */
int
vbitmap_resize(struct vbitmap *vb, size_t newsize)
{
	unsigned char *start;
	ptrdiff_t pos, end;
	size_t siz;

	pos = vb->vb_pos - vb->vb_start;
	end = vb->vb_end - vb->vb_start;

	siz = howmany(newsize, NBBY);
	start = realloc(vb->vb_start, siz);
	if (start == NULL && siz)
		return (-1);
	vb->vb_start = start;
	vb->vb_pos = start + pos;
	if (siz)
		vb->vb_end = start + siz - 1;
	else
		vb->vb_end = start + siz;
	vb->vb_lastsize = newsize % NBBY;
	/* Initialize new sections of the bitmap to zero. */
	if (siz > (size_t)end)
		memset(start + end + 1, 0, siz - end - 1);
	if (newsize && vb->vb_lastsize == 0)
		vb->vb_lastsize = NBBY;
	return (0);
}

/**
 * vbitmap_getsize - get the number of elements a bitmap represents.
 * @vb: pointer to bitmap.
 */
size_t
vbitmap_getsize(const struct vbitmap *vb)
{
	ptrdiff_t diff;

	diff = vb->vb_end - vb->vb_start;
	if (diff < NBBY)
		return (vb->vb_lastsize);
	return ((diff - 1) * NBBY + vb->vb_lastsize);
}

/**
 * vbitmap_printbin - print the contents of a bitmap in binary.
 * @vb: pointer to bitmap.
 */
void
vbitmap_printbin(const struct vbitmap *vb)
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
 * vbitmap_printhex - print the contents of a bitmap in hexadecimal.
 * @vb: pointer to bitmap.
 */
void
vbitmap_printhex(const struct vbitmap *vb)
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

__inline int
countbits(int a)
{
	int c;

	for (c = 0; a; a >>= 1)
		if (a & 1)
			c++;
	return (c);
}

/**
 * vbitmap_printstats - print the statistics of a bitmap.
 * @vb: pointer to bitmap.
 */
void
vbitmap_printstats(const struct vbitmap *vb)
{
	const unsigned char *p;
	int nset;

	nset = 0;
	for (p = vb->vb_start; p <= vb->vb_end; p++)
		nset += countbits(*p);

	printf("statistics: %d/%d (%.4f%%) in use\n", nset,
	       (int)((vb->vb_end - vb->vb_start) * NBBY + vb->vb_lastsize),
		100.0 * nset / NBBY / (vb->vb_end - vb->vb_start + 1));
}

/**
 * vbitmap_getstats - gather the statistics of a bitmap.
 * @vb: pointer to bitmap.
 */
void
vbitmap_getstats(const struct vbitmap *vb, int *used, int *total)
{
	const unsigned char *p;

	*used = 0;
	for (p = vb->vb_start; p <= vb->vb_end; p++)
		*used += countbits(*p);
	*total = vb->vb_lastsize + NBBY * (vb->vb_end - vb->vb_start);
}
