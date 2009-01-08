/* $Id$ */

#ifndef _PFL_MEMNODE_H_
#define _PFL_MEMNODE_H_

#ifdef HAVE_CPUSET
#include <numa.h>
#endif

static __inline int
psc_memnode_getid(void)
{
#ifdef HAVE_CPUSET
	return (numa_preferred());
#else
	return (0);
#endif
}

struct psc_memnode {
	struct dynarray	pmn_keys;
};

struct psc_hashtbl	psc_memnodes;
pthread_key_t		psc_memnodes_key;

static __inline struct psc_memnode *
psc_memnode_get(void)
{
	struct psc_memnode *pmn;
	int memnid;

	pmn = pthread_getspecific(psc_memnodes_key);
	if (pmn)
		return (pmn);

	memnid = psc_memnode_getid();
	b = psc_hashbkt_get(&psc_memnodes, );
	psc_hashbkt_lock(b);
	pmn = psc_hashbkt_search();
	if (pmn == NULL) {
		pmn = PSCALLOC(sizeof(*pmn));
		psc_hashbkt_add();
	}
	psc_hashbkt_unlock(b);
	pthread_setspecific(psc_memnodes_key, pmn);
	return (pmn);
}

static __inline void *
psc_memnode_getkey(struct psc_memnode *pmn, int key)
{
	void *val;

	val = NULL;
	spinlock(&pmn->pmn_lock);
	if (dynarray_len(&pmn->pmn_keys) > key)
		val = dynarray_getpos(&pmn->pmn_keys, key);
	freelock(&pmn->pmn_lock);
	return (val);
}

static __inline void
psc_memnode_setkey(struct psc_memnode *pmn, int pos, void *val)
{
	spinlock(&pmn->pmn_lock);
	dynarray_hintlen(&pmn->pmn_keys, pos + 1);
	dynarray_setpos(&pmn->pmn_keys, pos + 1, val);
	freelock(&pmn->pmn_lock);
}

#endif /* _PFL_MEMNODE_H_ */
