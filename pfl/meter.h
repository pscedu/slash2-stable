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

/*
 * This interface provides a basic rudiment for progress tracking for
 * reporting purposes.
 */

#ifndef _PFL_METER_H_
#define _PFL_METER_H_

#include <sys/types.h>

#include <stdarg.h>

#include "pfl/list.h"
#include "pfl/lockedlist.h"

#define PSC_METER_NAME_MAX	32

struct pfl_meter {
	struct psclist_head	 pm_lentry;
	char			 pm_name[PSC_METER_NAME_MAX];
	uint64_t		 pm_cur;
	uint64_t		 pm_max;
	uint64_t		*pm_maxp;
};

#define pfl_meter_free(pm)	pll_remove(&pfl_meters, (m))

void pfl_meter_init(struct pfl_meter *, uint64_t, const char *, ...);
void pfl_meter_destroy(struct pfl_meter *);

extern struct psc_lockedlist	 pfl_meters;

#endif /* _PFL_METER_H_ */
