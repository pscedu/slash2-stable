/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <stdarg.h>
#include <stdio.h>

#include "psc_ds/lockedlist.h"
#include "psc_util/iostats.h"
#include "psc_util/log.h"

struct psc_lockedlist psc_iostats =
    PLL_INITIALIZER(&psc_iostats, struct iostats, ist_lentry);

void
iostats_init(struct iostats *ist, const char *fmt, ...)
{
	va_list ap;
	int rc;

	memset(ist, 0, sizeof(*ist));
	if (gettimeofday(&ist->ist_lasttv, NULL) == -1)
		psc_fatal("gettimeofday");

	va_start(ap, fmt);
	rc = vsnprintf(ist->ist_name, sizeof(ist->ist_name), fmt, ap);
	va_end(ap);
	if (rc == -1 || rc >= (int)sizeof(ist->ist_name))
		psc_fatal("vsnprintf");

	pll_add(&psc_iostats, ist);
}

void
iostats_rename(struct iostats *ist, const char *fmt, ...)
{
	va_list ap;
	int rc;

	PLL_LOCK(&psc_iostats);

	va_start(ap, fmt);
	rc = vsnprintf(ist->ist_name, sizeof(ist->ist_name), fmt, ap);
	va_end(ap);

	PLL_ULOCK(&psc_iostats);

	if (rc == -1 || rc >= (int)sizeof(ist->ist_name))
		psc_fatal("vsnprintf");
}
