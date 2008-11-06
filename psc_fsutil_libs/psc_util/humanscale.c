/* $Id$ */

#include <stdio.h>

#include "psc_util/humanscale.h"

void
psc_humanscale(char buf[PSC_CTL_HUMANBUF_SZ], double num)
{
	int mag;

	/*
	 * 1234567
	 * 1000.3K
	 */
	for (mag = 0; num > 1024.0; mag++)
		num /= 1024.0;
	if (mag > 6)
		snprintf(buf, sizeof(buf), "%.1e", num);
	else
		snprintf(buf, sizeof(buf), "%6.1f%c", num, "BKMGTPE"[mag]);
}
