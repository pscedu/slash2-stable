/* $Id$ */

#ifndef __PFL_METER_H__
#define __PFL_METER_H__

#include <sys/types.h>

#include <stdarg.h>

#include "psc_ds/list.h"
#include "psc_util/lock.h"

#define PSC_METER_NAME_MAX 30

struct psc_meter {
	struct psclist_head	pm_lentry;
	char			pm_name[PSC_METER_NAME_MAX];
	size_t			pm_cur;
	size_t			pm_max;
};

extern struct psclist_head	pscMetersList;
extern psc_spinlock_t		pscMetersLock;

void psc_meter_init(struct psc_meter *, size_t, const char *, ...);
void psc_meter_free(struct psc_meter *);

#endif /* __PFL_METER_H__ */
