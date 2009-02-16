/* $Id$ */

#include "psc_ds/listcache.h"
#include "psc_ds/lockedlist.h"

struct psc_lockedlist pscListCaches =
    PLL_INITIALIZER(&pscListCaches, struct psc_listcache, lc_index_lentry);
