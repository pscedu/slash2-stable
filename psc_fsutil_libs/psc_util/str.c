/* $Id$ */
/* %PSC_COPYRIGHT% */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/str.h"
#include "psc_util/alloc.h" 

int
pfl_asprintf(char **p, const char *fmt, ...)
{
	va_list ap, apd;
	int sz;

	va_start(ap, fmt);
	va_copy(apd, ap);
	sz = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	*p = PSCALLOC(sz);

	vsnprintf(*p, sz, fmt, apd);
	va_end(apd);

	return (sz);
}
