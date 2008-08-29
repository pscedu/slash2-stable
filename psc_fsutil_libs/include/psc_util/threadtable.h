/* $Id$ */

#include <pthread.h>

#include "psc_ds/hash.h"

struct psc_thread;

#define psc_threadtbl_init(sz)		init_hash_table(&thrHtable, sz, "threads")

#define psc_threadtbl_get()		_psc_threadtbl_get(pthread_self(), 0)
#define psc_threadtbl_getid(thrid)	_psc_threadtbl_get(thrid, 0)
#define psc_threadtbl_get_canfail()	_psc_threadtbl_get(pthread_self(), 1)

void    prthrname(pthread_t);
void    psc_threadtbl_put(struct psc_thread *);
struct psc_thread *
	_psc_threadtbl_get(pthread_t, int);

extern struct hash_table thrHtable;
