/* $Id$ */

#include <stdio.h>

#include <stdarg.h>

#include "zestList.h"
#include "zestLock.h"
#include "iostats.h"

struct psclist_head	iostatsList = PSCLIST_HEAD_INIT(iostatsList);
psc_spinlock_t		iostatsListLock = LOCK_INITIALIZER;

void
iostats_init(struct iostats *ist, const char *fmt, ...)
{
	va_list ap;
	int rc;

	INIT_PSCLIST_ENTRY(&ist->ist_lentry);

	va_start(ap, fmt);
	rc = vsnprintf(ist->ist_name, sizeof(ist->ist_name), fmt, ap);
	va_end(ap);
	if (rc == -1)
		psc_fatal("vsnprintf");

	spinlock(&iostatsListLock);
	psclist_add(&ist->ist_lentry, &iostatsList);
	freelock(&iostatsListLock);
}
