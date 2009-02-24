/* $Id$ */

/*
 * Multilockable lists.
 *
 * mlists are like psclist_caches but work with multilocks, so threads
 * can pull a single item off any of a number of lists whenever an
 * item becomes available on any of them.
 *
 * A single psc_mlist variable represent one list, so to properly
 * poll a set, there is a bit of custom setup required by adding each
 * mlist's mlockcond to a multilock and then waiting on it.
 */

#ifndef _PFL_MLIST_H_
#define _PFL_MLIST_H_

#include <stdarg.h>
#include <string.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/assert.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/multilock.h"

#define MLIST_LOCK(pml)  spinlock(&(pml)->pml_lock)
#define MLIST_ULOCK(pml) freelock(&(pml)->pml_lock)

#define PML_NAME_MAX 32

struct psc_mlist {
	struct psclist_head	pml_index_lentry;	/* link adjoining mlists */
	psc_spinlock_t		pml_lock;

	int			pml_size;
	struct psclist_head	pml_listhd;
	struct multilock_cond	pml_mlcond_empty;	/* wait here while list is empty */
	char			pml_name[PML_NAME_MAX];	/* for ml mgmt */
	size_t			pml_nseen;		/* total #items placed on us */
};

/**
 * psc_mlist_empty - check if an mlist is empty.
 * @pml: the multilockable list to check.
 */
#define psc_mlist_empty(pml)	(psc_mlist_size(pml) == 0)

struct psclist_head *
	psc_mlist_tryget(struct psc_mlist *);
void	psc_mlist_put(struct psc_mlist *, struct psclist_head *);
void	psc_mlist_del(struct psc_mlist *, struct psclist_head *);
void	psc_mlist_init(struct psc_mlist *, void *arg, const char *, ...);
int	psc_mlist_size(struct psc_mlist *);

extern struct psc_lockedlist psc_mlists;

#endif /* _PFL_MLIST_H_ */
