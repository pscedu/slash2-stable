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
	return (PCPP_STR(psc_hostname));
}
