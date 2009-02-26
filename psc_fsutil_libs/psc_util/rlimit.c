/* $Id$ */

#include <sys/types.h>
#include <sys/resource.h>

#include "psc_util/lock.h"

psc_spinlock_t psc_rlimit_lock = LOCK_INITIALIZER;

int
psc_setrlimit(int field, rlim_t soft, rlim_t hard)
{
	struct rlimit rlim;
	int locked, rc;

	rlim.rlim_cur = soft;
	rlim.rlim_max = hard;
	locked = reqlock(&psc_rlimit_lock);
	rc = setrlimit(field, &rlim);
	ureqlock(&psc_rlimit_lock, locked);
	return (rc);
}

int
psc_getrlimit(int field, rlim_t *soft, rlim_t *hard)
{
	struct rlimit rlim;
	int locked, rc;

	locked = reqlock(&psc_rlimit_lock);
	rc = getrlimit(field, &rlim);
	ureqlock(&psc_rlimit_lock, locked);
	if (rc == 0) {
		if (soft)
			*soft = rlim.rlim_cur;
		if (hard)
			*hard = rlim.rlim_max;
	}
	return (rc);
}
