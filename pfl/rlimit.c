/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/types.h>
#include <sys/resource.h>

#include <inttypes.h>

#include "pfl/lock.h"
#include "pfl/log.h"

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
