/* $Id$ */

#include <stdarg.h>
#include <stdio.h>

#include "psc_ds/lockedlist.h"
#include "psc_util/iostats.h"
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
	if (rc == -1 || rc >= (int)sizeof(ist->ist_name))
		psc_fatal("vsnprintf");

	pll_add(&psc_iostats, ist);
}

void
iostats_rename(struct iostats *ist, const char *fmt, ...)
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
