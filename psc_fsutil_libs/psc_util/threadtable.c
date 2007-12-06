/* $Id: threadtable.c 1907 2007-10-18 18:15:57Z yanovich $ */

/*
 * This file could be replaced by thread-local storage at some point.
 */

#include <pthread.h>
#include <stdio.h>

#include "psc_types.h"
#include "psc_ds/hash.h"
#include "psc_util/alloc.h"
#include "psc_util/assert.h"
#include "psc_util/threadtable.h"
#include "psc_util/thread.h"

#define PSC_THREAD_HTABLE_SZ 53

struct hash_table thrHtable;

/**
 * psc_threadtbl_put - adds a thread to the thread hash table.
 * @thr: pointer to psc_thread
 */
void
psc_threadtbl_put(struct psc_thread *thr)
{
	struct hash_entry *e;

	e = PSCALLOC(sizeof(*e));
	psc_assert(thr);

	init_hash_entry(e, &thr->pscthr_hashid, thr);
	add_hash_entry(&thrHtable, e);
}

/**
 * _psc_threadtbl_get - lookup thread data structure.
 * @canfail: whether the lookup can fail and return NULL.
 */
struct psc_thread *
_psc_threadtbl_get(pthread_t thrid, int canfail)
{
	struct hash_entry *e;

	if (canfail && thrHtable.htable_buckets == NULL)
		return (NULL);

	e = get_hash_entry(&thrHtable, thrid, NULL);

	if (!canfail)
		psc_assert(e != NULL);
	return (e ? e->private : NULL);
}

/**
 * psc_threadtbl_init - intialize thread hash table.
 */
void
psc_threadtbl_init(void)
{
	init_hash_table(&thrHtable, PSC_THREAD_HTABLE_SZ, "threads");
}

void
prthrname(pthread_t thrid)
{
	struct psc_thread *thr;

	thr = psc_threadtbl_getid(thrid);
	printf("%s\n", thr->pscthr_name);
}
