/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Lists which interface with multiwaits.
 *
 * mlists are like psclist_caches but work with multiwaits so threads
 * can pull a single item off any of a number of lists whenever an
 * item becomes available on any of them.
 *
 * A single psc_mlist variable represent one list, so to properly
 * poll a set, there is a bit of custom setup required by adding each
 * mlist's mwcond to a multiwait and then waiting on it.
 */

#ifndef _PFL_MLIST_H_
#define _PFL_MLIST_H_

#include <stddef.h>

#include "pfl/explist.h"
#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/lock.h"
#include "psc_util/multiwait.h"

struct psc_mlist {
	struct psc_explist		pml_explist;
//	struct psc_multiwaitcond	pml_mwcond_want;	/* when someone wants an obj */
	struct psc_multiwaitcond	pml_mwcond_empty;	/* when we're empty */
#define pml_lentry	pml_explist.pexl_lentry
#define pml_lock	pml_explist.pexl_lock
#define pml_name	pml_explist.pexl_name
#define pml_nitems	pml_explist.pexl_nitems
#define pml_nseen	pml_explist.pexl_nseen
#define pml_offset	pml_explist.pexl_offset
#define pml_pll		pml_explist.pexl_pll
};

#define MLIST_LOCK(pml)			PLL_LOCK(&(pml)->pml_pll)
#define MLIST_ULOCK(pml)		PLL_ULOCK(&(pml)->pml_pll)
#define MLIST_RLOCK(pml)		PLL_RLOCK(&(pml)->pml_pll)
#define MLIST_URLOCK(pml, lk)		PLL_URLOCK(&(pml)->pml_pll, (lk))
#define MLIST_HASLOCK(pml)		PLL_HASLOCK(&(pml)->pml_pll)

#define psc_mlist_addtail(pml, p)	_psc_mlist_add(PFL_CALLERINFO(), (pml), (p), 1)
#define psc_mlist_addhead(pml, p)	_psc_mlist_add(PFL_CALLERINFO(), (pml), (p), 0)

#define psc_mlist_add(pml, p)		 psc_mlist_addtail((pml), (p))

/**
 * psc_mlist_empty - Check if an mlist is empty.
 * @pml: the mlist to check.
 */
#define psc_mlist_empty(pml)		pll_empty(&(pml)->pml_pll)

#define psc_mlist_size(pml)		pll_nitems(&(pml)->pml_pll)

/**
 * psc_mlist_tryget - Get an item from an mlist if one is available.
 * @pml: the mlist to access.
 */
#define psc_mlist_tryget(pml)		pll_get(&(pml)->pml_pll)

/**
 * psc_mlist_remove - Remove an item's membership from a mlist.
 * @pml: the mlist to access.
 * @p: item to unlink.
 */
#define psc_mlist_remove(pml, p)	pll_remove(&(pml)->pml_pll, (p))

#define psc_mlist_conjoint(pml, p)	pll_conjoint(&(pml)->pml_pll, (p))

/**
 * psc_mlist_reginit - Initialize and register an mlist.
 * @pml: mlist to initialize.
 * @mwcarg: multiwaitcond to use for availability notification.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @namefmt: printf(3) format for mlist name.
 */
#define psc_mlist_reginit(pml, mwcarg, type, member, namefmt, ...)	\
	_psc_mlist_reginit((pml), 0, (mwcarg), offsetof(type, member),	\
	    (namefmt), ## __VA_ARGS__)

void	_psc_mlist_add(const struct pfl_callerinfo *, struct psc_mlist *, void *, int);
void	_psc_mlist_init(struct psc_mlist *, int, void *, ptrdiff_t,
	    const char *, ...);
void	_psc_mlist_reginit(struct psc_mlist *, int, void *, ptrdiff_t,
	    const char *, ...);

extern struct psc_lockedlist psc_mlists;

#endif /* _PFL_MLIST_H_ */
