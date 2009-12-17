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
#include <string.h>

#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/meter.h"

struct psclist_head	pscMetersList = PSCLIST_HEAD_INIT(pscMetersList);
psc_spinlock_t		pscMetersLock = LOCK_INITIALIZER;

void
psc_meter_init(struct psc_meter *pm, size_t max, const char *fmt, ...)
{
	va_list ap;
	int rc;

	memset(pm, 0, sizeof(*pm));
	pm->pm_max = max;

	va_start(ap, fmt);
	rc = vsnprintf(pm->pm_name, sizeof(pm->pm_name), fmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf");

	spinlock(&pscMetersLock);
	psclist_xadd(&pm->pm_lentry, &pscMetersList);
	freelock(&pscMetersLock);
}

void
psc_meter_free(struct psc_meter *pm)
{
	spinlock(&pscMetersLock);
	psclist_del(&pm->pm_lentry);
	freelock(&pscMetersLock);
}
