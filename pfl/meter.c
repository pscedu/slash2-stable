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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/list.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/meter.h"

struct psc_lockedlist	psc_meters =
    PLL_INIT(&psc_meters, struct psc_meter, pm_lentry);

void
psc_meter_init(struct psc_meter *pm, uint64_t max, const char *fmt, ...)
{
	va_list ap;
	int rc;

	memset(pm, 0, sizeof(*pm));
	INIT_PSC_LISTENTRY(&pm->pm_lentry);
	pm->pm_max = max;
	pm->pm_maxp = &pm->pm_max;

	va_start(ap, fmt);
	rc = vsnprintf(pm->pm_name, sizeof(pm->pm_name), fmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf");

	pll_addtail(&psc_meters, pm);
}
