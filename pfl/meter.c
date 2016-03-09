/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/meter.h"

struct psc_lockedlist	pfl_meters =
    PLL_INIT(&pfl_meters, struct pfl_meter, pm_lentry);

void
pfl_meter_init(struct pfl_meter *pm, uint64_t max, const char *fmt, ...)
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

	pll_addtail(&pfl_meters, pm);
}

void
pfl_meter_destroy(struct pfl_meter *pm)
{
	pll_remove(&pfl_meters, pm);
}
