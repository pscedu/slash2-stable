/* $Id$ */

#include <stdio.h>

#include <stdarg.h>

#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/iostats.h"

struct psclist_head	pscIostatsList = PSCLIST_HEAD_INIT(pscIostatsList);
psc_spinlock_t		pscIostatsListLock = LOCK_INITIALIZER;

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

	spinlock(&pscIostatsListLock);
	psclist_add(&ist->ist_lentry, &pscIostatsList);
	freelock(&pscIostatsListLock);
}
