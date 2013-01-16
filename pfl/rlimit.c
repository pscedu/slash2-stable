/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/resource.h>

#include <inttypes.h>

#include "psc_util/lock.h"
#include "psc_util/log.h"

psc_spinlock_t psc_rlimit_lock = SPINLOCK_INIT;

int
psc_setrlimit(int field, rlim_t soft, rlim_t hard)
{
	struct rlimit rlim;
	int locked, rc;

	rlim.rlim_cur = soft;
	rlim.rlim_max = hard;
	locked = reqlock(&psc_rlimit_lock);
	rc = setrlimit(field, &rlim);
	ureqlock(&psc_rlimit_lock, locked);
	return (rc);
}

int
psc_getrlimit(int field, rlim_t *soft, rlim_t *hard)
{
	struct rlimit rlim;
	int locked, rc;

	locked = reqlock(&psc_rlimit_lock);
	rc = getrlimit(field, &rlim);
	ureqlock(&psc_rlimit_lock, locked);
	if (rc == 0) {
		if (soft)
			*soft = rlim.rlim_cur;
		if (hard)
			*hard = rlim.rlim_max;
	}
	return (rc);
}

int
psc_rlim_adj(int field, int adjv)
{
	rlim_t v;
	int rc;

	spinlock(&psc_rlimit_lock);
	rc = psc_getrlimit(field, NULL, &v);
	if (rc == -1)
		psclog_warn("getrlimit %d", field);
	else {
		v += adjv;
		rc = psc_setrlimit(field, v, v);
		if (rc == -1)
			psclog_warn("setrlimit %d %"PRId64, field, v);
	}
	freelock(&psc_rlimit_lock);
	return (rc == 0);
}
