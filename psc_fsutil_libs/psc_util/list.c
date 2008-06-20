/* $Id$ */

#include "psc_util/list.h"
#include "psc_util/listcache.h"
#include "psc_util/lock.h"

struct psclist_head pscListCaches = PSCLIST_HEAD_INIT(pscListCaches);
psc_spinlock_t pscListCachesLock = LOCK_INITIALIZER;
