/* $Id$ */

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "psc_util/log.h"
#include "psc_util/pthrutil.h"

void
psc_pthread_mutex_init(pthread_mutex_t *mut)
{
	pthread_mutexattr_t attr;
	int rc;

	rc = pthread_mutexattr_init(&attr);
	if (rc)
		psc_fatalx("pthread_mutexattr_init: %s", strerror(rc));
	rc = pthread_mutexattr_settype(&attr,
	    PTHREAD_MUTEX_ERRORCHECK_NP);
	if (rc)
		psc_fatalx("pthread_mutexattr_settype: %s",
		    strerror(rc));
	rc = pthread_mutex_init(mut, &attr);
	if (rc)
		psc_fatalx("pthread_mutex_init: %s", strerror(rc));
}

void
psc_pthread_mutex_lock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_lock(mut);
	if (rc)
		psc_fatalx("pthread_mutex_lock: %s", strerror(rc));
}

void
psc_pthread_mutex_unlock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_unlock(mut);
	if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
}

int
psc_pthread_mutex_reqlock(pthread_mutex_t *mut)
{
	int rc;

	rc = pthread_mutex_lock(mut);
	if (rc == EDEADLK)
		rc = 1;
	else if (rc)
		psc_fatalx("pthread_mutex_unlock: %s", strerror(rc));
	return (rc);
}

void
psc_pthread_mutex_ureqlock(pthread_mutex_t *mut, int waslocked)
{
	if (!waslocked)
		psc_pthread_mutex_unlock(mut);
}
