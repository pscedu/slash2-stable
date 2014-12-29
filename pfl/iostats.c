/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/lockedlist.h"
#include "pfl/time.h"
#include "pfl/iostats.h"
#include "pfl/log.h"

struct psc_lockedlist	psc_iostats =
    PLL_INIT(&psc_iostats, struct psc_iostats, ist_lentry);

/*
 * I/O statistic interval lengths:
 *	(o) values are in seconds,
 *	(o) must be even divisors and multipliers of each other, and
 *	(o) must be sorted by increasing lengths (1,2,3) not (3,2,1).
 */
int psc_iostat_intvs[] = {
	1,
	10
};

void
psc_iostats_initf(struct psc_iostats *ist, int flags, const char *fmt,
    ...)
{
	struct timeval tv;
	va_list ap;
	int i, rc;

	memset(ist, 0, sizeof(*ist));
	INIT_PSC_LISTENTRY(&ist->ist_lentry);
	ist->ist_flags = flags;

	va_start(ap, fmt);
	rc = vsnprintf(ist->ist_name, sizeof(ist->ist_name), fmt, ap);
	va_end(ap);
	if (rc == -1 || rc >= (int)sizeof(ist->ist_name))
		psc_fatal("vsnprintf");

	PFL_GETTIMEVAL(&tv);

	for (i = 0; i < IST_NINTV; i++) {
		ist->ist_intv[i].istv_lastv = tv;
		ist->ist_intv[i].istv_intv_dur.tv_sec = 1;
		psc_atomic64_init(&ist->ist_intv[i].istv_cur_len);
	}

	pll_add(&psc_iostats, ist);
}

void
psc_iostats_rename(struct psc_iostats *ist, const char *fmt, ...)
{
	va_list ap;
	int rc;

	PLL_LOCK(&psc_iostats);

	va_start(ap, fmt);
	rc = vsnprintf(ist->ist_name, sizeof(ist->ist_name), fmt, ap);
	va_end(ap);

	PLL_ULOCK(&psc_iostats);

	if (rc == -1 || rc >= (int)sizeof(ist->ist_name))
		psc_fatal("vsnprintf");
}

void
psc_iostats_destroy(struct psc_iostats *ist)
{
	pll_remove(&psc_iostats, ist);
}

void
pfl_iostats_grad_init(struct pfl_iostats_grad *ist0, int flags,
    const char *prefix)
{
	const char *suf, *nsuf, *mode = "rd";
	struct pfl_iostats_grad *ist;
	uint64_t sz, nsz;
	int sub, i;

	for (i = 0; i < 2; i++) {
		sz = 0;
		suf = "";
		nsuf = "K";
		for (ist = ist0; ist->size; ist++, sz = nsz) {
			nsz = ist->size / 1024;

			if (nsz == 1024) {
				nsuf = "M";
				nsz = 1;
			}
			sub = nsz == 1 || nsz == 1024 ? 0 : 1;
			psc_iostats_initf(i ? &ist->rw.wr : &ist->rw.rd,
			    flags, "%s-%s:%d%s-%d%s", prefix, mode, sz,
			    suf, nsz - sub, nsuf);

			suf = "K";
		}

		psc_iostats_initf(i ? &ist->rw.wr : &ist->rw.rd, flags,
		    "%s-%s:>=%d%s", prefix, mode, sz, nsuf);

		mode = "wr";
	}
}
