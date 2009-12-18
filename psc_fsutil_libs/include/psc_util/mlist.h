/* $Id$ */

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

#include "psc_ds/list.h"
#include "psc_ds/listguts.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/lock.h"
#include "psc_util/multiwait.h"

struct psc_mlist {
	struct psc_listguts		pml_guts;

//	struct psc_multiwaitcond	pml_mwcond_want;	/* when someone wants an obj */
	struct psc_multiwaitcond	pml_mwcond_empty;	/* when we're empty */
#define pml_index_lentry		pml_guts.plg_index_lentry
#define pml_lock			pml_guts.plg_lock
#define pml_name			pml_guts.plg_name
#define pml_listhd			pml_guts.plg_listhd
#define pml_size			pml_guts.plg_size
#define pml_nseen			pml_guts.plg_nseen
#define pml_entsize			pml_guts.plg_entsize
#define pml_offset			pml_guts.plg_offset
};

#define MLIST_LOCK(pml)		spinlock(&(pml)->pml_lock)
#define MLIST_ULOCK(pml)	freelock(&(pml)->pml_lock)

/**
 * psc_mlist_empty - check if an mlist is empty.
 * @pml: the mlist to check.
 */
#define psc_mlist_empty(pml)	(psc_mlist_size(pml) == 0)

#define psc_mlist_reginit(pml, mwcarg, type, member, namefmt, ...)	\
	_psc_mlist_reginit((pml), 0, (mwcarg), sizeof(type),		\
	    offsetof(type, member), (namefmt), ## __VA_ARGS__)

void	*psc_mlist_tryget(struct psc_mlist *);
void	 psc_mlist_add(struct psc_mlist *, void *);
void	 psc_mlist_remove(struct psc_mlist *, void  *);
void	_psc_mlist_init(struct psc_mlist *, int, void *, size_t,
		ptrdiff_t, const char *, ...);
void	_psc_mlist_reginit(struct psc_mlist *, int, void *, size_t,
		ptrdiff_t, const char *, ...);
int	 psc_mlist_size(struct psc_mlist *);

extern struct psc_lockedlist psc_mlists;

#endif /* _PFL_MLIST_H_ */
