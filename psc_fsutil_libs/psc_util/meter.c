/* $Id$ */

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
