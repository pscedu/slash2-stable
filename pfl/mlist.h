/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
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
#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "pfl/lock.h"
#include "pfl/multiwait.h"

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
void	 pfl_mlist_destroy(struct psc_mlist *);
void	_psc_mlist_init(struct psc_mlist *, int, void *, ptrdiff_t,
	    const char *, ...);
void	_psc_mlist_reginit(struct psc_mlist *, int, void *, ptrdiff_t,
	    const char *, ...);

extern struct psc_lockedlist psc_mlists;

#endif /* _PFL_MLIST_H_ */
