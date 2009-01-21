/* $Id$ */

#include <stdarg.h>
#include <stdio.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/iostats.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

struct psc_lockedlist psc_iostats =
    PLL_INITIALIZER(&psc_iostats, struct iostats, ist_lentry);

void
iostats_init(struct iostats *ist, const char *fmt, ...)
{
	va_list ap;
	int rc;

	memset(ist, 0, sizeof(*ist));

	va_start(ap, fmt);
	rc = vsnprintf(ist->ist_name, sizeof(ist->ist_name), fmt, ap);
	va_end(ap);
	if (rc == -1)
		psc_fatal("vsnprintf");

	pll_add(&psc_iostats, ist);
}
