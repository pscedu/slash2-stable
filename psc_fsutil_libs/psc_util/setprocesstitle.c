/* $Id$ */

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "psc_util/setprocesstitle.h"
#include "psc_util/strlcpy.h"

int
setprocesstitle(char **av, const char *fmt, ...)
{
	char buf[2048];
	size_t len, newlen;
	va_list ap;
	int j;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	newlen = strlen(buf);

	if (av[1])
		len = av[1] - av[0];
	else
		len = strlen(av[0]);
	strlcpy(av[0], buf, len + 1);
	if (newlen < len)
		memset(av[0] + newlen, '\0', len - newlen);
	for (j = 1; av[j]; j++)
		memset(av[j], '\0', strlen(av[j]));
	return (0);
}
