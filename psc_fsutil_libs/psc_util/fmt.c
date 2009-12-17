/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <stdio.h>

#include "psc_util/fmt.h"

void
psc_fmt_human(char buf[PSCFMT_HUMAN_BUFSIZ], double num)
{
	int mag;

	/*
	 * 1234567
	 * 1000.3K
	 */
	for (mag = 0; num >= 1024.0; mag++)
		num /= 1024.0;
	if (mag > 6)
		snprintf(buf, PSCFMT_HUMAN_BUFSIZ, "%.1e", num);
	else
		snprintf(buf, PSCFMT_HUMAN_BUFSIZ, "%6.1f%c", num, "BKMGTPE"[mag]);
}

void
psc_fmt_ratio(char buf[PSCFMT_RATIO_BUFSIZ], int n, int d)
{
	if (n == d)
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "100%%");
	else if (n == 0)
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "0%%");
	else if (d)
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "%5.2f%%",
		    n * 100.0 / d);
	else
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "<und>");
}
