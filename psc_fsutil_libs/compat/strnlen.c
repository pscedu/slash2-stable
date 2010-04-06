/* $Id$ */

#include "psc_util/str.h"

size_t
strnlen(const char *s, size_t max)
{
	size_t j;

	for (j = 0; j < max && s[j] != '\0'; j++)
		;
	return (j);
}
