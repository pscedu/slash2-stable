/* $Id$ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
