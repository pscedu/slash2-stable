/* $Id: lock2.c 8259 2009-10-08 03:15:33Z yanovich $ */

#define TEST_LOCK_INCLUDE	"psc_util/spinlock.h"
#define TEST_LOCK_TYPE		struct psc_spinlock
#define TEST_LOCK_INITIALIZER	PSC_SPINLOCK_INIT
#define TEST_LOCK_ACQUIRE(lk)	psc_spin_lock(lk)
#define TEST_LOCK_RELEASE(lk)	psc_spin_unlock(lk)

#include "../lock/lock_template.c"
