/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

void
psc_dumpstack(__unusedx int sig)
{
	static psc_spinlock_t lock = LOCK_INITIALIZER;
	char buf[BUFSIZ];

	spinlock(&lock);
	snprintf(buf, sizeof(buf), "pstack %d || gstack %d",
	    getpid(), getpid());
	system(buf);
	_exit(1);
}
