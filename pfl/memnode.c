/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <pthread.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/dynarray.h"
#include "pfl/lock.h"
#include "pfl/memnode.h"

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
	int memnid, rc;

	pmn = pthread_getspecific(psc_memnodes_key);
	if (pmn)
		return (pmn);

	memnid = psc_memnode_getid();
	spinlock(&psc_memnodes_lock);
	if (psc_dynarray_ensurelen(&psc_memnodes, memnid + 1) == -1)
		psc_fatalx("ensurelen");
	pmnv = psc_dynarray_get_mutable(&psc_memnodes);
	pmn = pmnv[memnid];
	if (pmn == NULL) {
		pmn = psc_alloc(sizeof(*pmn), PAF_NOLOG);
		INIT_SPINLOCK(&pmn->pmn_lock);
		psc_dynarray_init(&pmn->pmn_keys);
		rc = pthread_setspecific(psc_memnodes_key, pmn);
		if (rc)
			psc_fatalx("pthread_setspecific: %s",
			    strerror(rc));
		psc_dynarray_setpos(&psc_memnodes, memnid, pmn);
	}
	freelock(&psc_memnodes_lock);
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

	locked = reqlock(&pmn->pmn_lock);
	if (psc_dynarray_ensurelen(&pmn->pmn_keys, pos + 1) == -1)
		psc_fatalx("ensurelen");
	psc_dynarray_setpos(&pmn->pmn_keys, pos, val);
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
