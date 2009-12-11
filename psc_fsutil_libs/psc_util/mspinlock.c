/* $Id$ */

#include <pthread.h>
#include <stdint.h>

#include "pfl/cdefs.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/lock.h"
#include "psc_util/mspinlock.h"

struct psc_vbitmap	*_psc_mspin_unthridmap;
psc_spinlock_t		 _psc_mspin_unthridmap_lock = LOCK_INITIALIZER;
pthread_key_t		 _psc_mspin_thrkey;
