/* $Id$ */
/* %ISC_LICENSE% */

#include <stdio.h>
#include <stdlib.h>

int
bsearch_floor(const void *key, const void *base, size_t nel,
    size_t width, int (*cmpf)(const void *, const void *))
{
	int rc, min, max, mid;
	const void *p;

	min = mid = 0;
	max = nel - 1;
	while (min <= max) {
		mid = (max + min) / 2;
		p = (char *)base + mid * width;
		rc = cmpf(key, p);
		if (rc < 0) {
			max = mid - 1;
			mid--;
			/*
			 * If the item doesn't exist, inform caller that
			 * the position the item should take on is before
			 * this mid index.
			 */
		} else if (rc > 0)
			min = mid + 1;
		else
			break;
	}
	return (mid);
}

int
bsearch_ceil(const void *key, const void *base, size_t nel,
    size_t width, int (*cmpf)(const void *, const void *))
{
	int rc, min, max, mid;
	const void *p;

	min = mid = 0;
	max = nel - 1;
	while (min <= max) {
		mid = (max + min) / 2;
		p = (char *)base + mid * width;
		rc = cmpf(key, p);
		if (rc < 0)
			max = mid - 1;
		else if (rc > 0) {
			min = mid + 1;

			/*
			 * If the item doesn't exist, inform caller that
			 * the position the item should take on is after
			 * this mid index.
			 */
			mid++;
		} else
			break;
	}
	return (mid);
}
