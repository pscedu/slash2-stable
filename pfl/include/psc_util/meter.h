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
 * This interface provides a basic rudiment for progress tracking for
 * reporting purposes.
 */

#ifndef _PFL_METER_H_
#define _PFL_METER_H_

#include <sys/types.h>

#include <stdarg.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"

#define PSC_METER_NAME_MAX	32

struct psc_meter {
	struct psclist_head	 pm_lentry;
	char			 pm_name[PSC_METER_NAME_MAX];
	uint64_t		 pm_cur;
	uint64_t		 pm_max;
	uint64_t		*pm_maxp;
};

#define psc_meter_free(pm)	pll_remove(&psc_meters, (m))

void psc_meter_init(struct psc_meter *, uint64_t, const char *, ...);

extern struct psc_lockedlist	 psc_meters;

#endif /* _PFL_METER_H_ */
