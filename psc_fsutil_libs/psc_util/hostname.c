/* $Id$ */

/*
 * This is single-threaded, overridden by psc_util/thread.c
 * for multi-threaded environments.
 */

#include <sys/param.h>

#include <unistd.h>

#include "pfl/cdefs.h"
#include "psc_util/log.h"

char psc_hostname[HOST_NAME_MAX];

__weak const char *
psc_get_hostname(void)
{
	if (psc_hostname[0] == '\0')
		if (gethostname(psc_hostname, sizeof(psc_hostname)) == -1)
			psc_fatal("gethostname");
	return (psc_hostname);
}
