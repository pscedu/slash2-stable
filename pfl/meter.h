/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
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

#include "pfl/list.h"
#include "pfl/lockedlist.h"

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
void psc_meter_destroy(struct psc_meter *);

extern struct psc_lockedlist	 psc_meters;

#endif /* _PFL_METER_H_ */
