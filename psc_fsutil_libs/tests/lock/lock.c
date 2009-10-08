/* $Id$ */

#define TEST_LOCK_INCLUDE	"psc_util/lock.h"
#define TEST_LOCK_TYPE		psc_spinlock_t
#define TEST_LOCK_INITIALIZER	LOCK_INITIALIZER
#define TEST_LOCK_ACQUIRE(lk)	spinlock(lk)
#define TEST_LOCK_RELEASE(lk)	freelock(lk)

#include "lock_template.c"
