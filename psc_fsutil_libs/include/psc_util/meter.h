/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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
