/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <pthread.h>
#include <string.h>

#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/memnode.h"

__static psc_spinlock_t		psc_memnodes_lock = SPINLOCK_INIT;
__static struct psc_dynarray	psc_memnodes = DYNARRAY_INIT;
__static pthread_key_t		psc_memnodes_key;

void
psc_memnode_init(void)
{
	int rc;

	rc = pthread_key_create(&psc_memnodes_key, NULL);
	if (rc)
		psc_fatalx("pthread_key_create: %s", strerror(rc));
}

struct psc_memnode *
psc_memnode_get(void)
{
	struct psc_memnode *pmn, **pmnv;
	int memnid;
	int rc;

	pmn = pthread_getspecific(psc_memnodes_key);
	if (pmn)
		return (pmn);

	memnid = psc_memnode_getid();
	spinlock(&psc_memnodes_lock);
	if (psc_dynarray_ensurelen(&psc_memnodes, memnid + 1) == -1)
		psc_fatalx("ensurelen");
	pmnv = psc_dynarray_get(&psc_memnodes);
	pmn = pmnv[memnid];
	if (pmn == NULL) {
		pmn = pmnv[memnid] = psc_alloc(sizeof(*pmn),
		    PAF_NOLOG);
		INIT_SPINLOCK(&pmn->pmn_lock);
		psc_dynarray_init(&pmn->pmn_keys);
	}
	freelock(&psc_memnodes_lock);
	rc = pthread_setspecific(psc_memnodes_key, pmn);
	if (rc)
		psc_fatalx("pthread_setspecific: %s", strerror(rc));
	return (pmn);
}

void *
psc_memnode_getkey(struct psc_memnode *pmn, int key)
{
	int locked;
	void *val;

	val = NULL;
	locked = reqlock(&pmn->pmn_lock);
	if (psc_dynarray_len(&pmn->pmn_keys) > key)
		val = psc_dynarray_getpos(&pmn->pmn_keys, key);
	ureqlock(&pmn->pmn_lock, locked);
	return (val);
}

void
psc_memnode_setkey(struct psc_memnode *pmn, int pos, void *val)
{
	int locked;
	void **v;

	locked = reqlock(&pmn->pmn_lock);
	if (psc_dynarray_ensurelen(&pmn->pmn_keys, pos + 1) == -1)
		psc_fatalx("ensurelen");
	v = psc_dynarray_get(&pmn->pmn_keys);
	v[pos] = val;
	ureqlock(&pmn->pmn_lock, locked);
}

void *
psc_memnode_getobj(int pos, void *(*initf)(void *), void *arg)
{
	struct psc_memnode *pmn;
	void *p;

	pmn = psc_memnode_get();
	p = psc_memnode_getkey(pmn, pos);
	if (p)
		return (p);
	spinlock(&pmn->pmn_lock);
	p = psc_memnode_getkey(pmn, pos);
	if (p) {
		freelock(&pmn->pmn_lock);
		return (p);
	}
	p = initf(arg);
	psc_memnode_setkey(pmn, pos, p);
	freelock(&pmn->pmn_lock);
	return (p);
}
