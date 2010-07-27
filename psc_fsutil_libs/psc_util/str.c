/* $Id$ */

#include <string.h>

#include "pfl/str.h"

int
pfl_strncmp2(const char *a, size_t alen, const char *b, size_t blen)
{
	size_t j;

	if (alen == blen)
		return (strncmp(a, b, alen));

	for (j = 0; ; j++) {
		if (j > alen)
			return (-1);
		if (j > blen)
			return (1);

		if (a[j] < b[j])
			return (-1);
		if (a[j] > b[j])
			return (1);
	}
	return (0);
}
