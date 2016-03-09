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

/*
 * Doubly linked list implementation.
 *
 * A "list head" here is a handle for the linked list.  For example:
 *
 *	struct psclist_head mylisthd;
 *
 * Custom structures may be added to this list via containing a
 * psc_listentry structure in their makeup:
 *
 *	struct mystruct {
 *		int			myvalue;
 *		struct psc_listentry	lentry;
 *	};
 *
 * And an example on how to add a mystruct item instance to our list:
 *
 *	struct mystruct a;
 *
 *	INIT_LISTENTRY(&a->lentry);
 *	psclist_add(&a->lentry, &mylisthd);
 *
 * Now, one may iterate through mylisthd and come across the `a' element
 * that was added.
 *
 * This distinction between list entry and head structures reduces some
 * flexbility with the API (since you must have a list head) at the
 * benefit of reducing confusion and avoiding abuse.
 *
 * Under PFL_DEBUG mode, the list head here becomes the plh_owner of the
 * item when it is added.  This provision helps one debugging which list
 * an item was already on when trying to add the item to a different
 * list.
 */

#ifndef _PFL_LIST_H_
#define _PFL_LIST_H_

#include <stdint.h>
#include <stdio.h>

#include "pfl/pfl.h"
#include "pfl/log.h"

/* note: this structure is used for both a head and an entry */
struct psclist_head {
	struct psclist_head		*plh_next;
	struct psclist_head		*plh_prev;
#if PFL_DEBUG
	uint64_t			 plh_magic;
	struct psclist_head		*plh_owner;	/* psclist_head this item is on */
#endif
};

#define psc_listentry psclist_head

#define PLENT_MAGIC			UINT64_C(0x1234123412341234)

#if PFL_DEBUG
#  define PSCLIST_HEAD_INIT(name)	{ &(name), &(name), PLENT_MAGIC, &(name) }
#  define PSC_LISTENTRY_INIT		{ NULL, NULL, PLENT_MAGIC, NULL }
#else
#  define PSCLIST_HEAD_INIT(name)	{ &(name), &(name) }
#  define PSC_LISTENTRY_INIT		{ NULL, NULL }
#endif

#define PSCLIST_HEAD(name)							\
	struct psclist_head name = PSCLIST_HEAD_INIT(name)

#define psc_listhd_first(hd)		(hd)->plh_next
#define psc_listhd_last(hd)		(hd)->plh_prev

/**
 * psc_lentry_next - Access the entry following the specified entry.
 * @e: entry
 */
#define psc_lentry_next(e)		(e)->plh_next

/**
 * psc_lentry_prev - Access the entry before the specified entry.
 * @e: entry
 */
#define psc_lentry_prev(e)		(e)->plh_prev

#if PFL_DEBUG
#  define INIT_LISTHEAD(hd)							\
	do {									\
		psc_listhd_first(hd) = (hd);					\
		psc_listhd_last(hd) = (hd);					\
		(hd)->plh_owner = (hd);						\
		(hd)->plh_magic = PLENT_MAGIC;					\
	} while (0)

#  define INIT_LISTENTRY(e)							\
	do {									\
		psc_lentry_prev(e) = NULL;					\
		psc_lentry_next(e) = NULL;					\
		(e)->plh_owner = NULL;						\
		(e)->plh_magic = PLENT_MAGIC;					\
	} while (0)
#else
#  define INIT_LISTHEAD(hd)							\
	do {									\
		psc_listhd_first(hd) = (hd);					\
		psc_listhd_last(hd) = (hd);					\
	} while (0)

#  define INIT_LISTENTRY(e)							\
	do {									\
		psc_lentry_prev(e) = NULL;					\
		psc_lentry_next(e) = NULL;					\
	} while (0)
#endif

#define INIT_PSCLIST_HEAD(hd)	INIT_LISTHEAD(hd)
#define INIT_PSC_LISTENTRY(e)	INIT_LISTENTRY(e)

static __inline void
_psclist_add(struct psc_listentry *e, struct psc_listentry *prev,
    struct psc_listentry *next)
{
#if PFL_DEBUG
	psc_assert(e->plh_owner == NULL);
	psc_assert(prev->plh_owner && next->plh_owner);
	psc_assert(prev->plh_owner == next->plh_owner);

	psc_assert(e->plh_magic == PLENT_MAGIC);
	psc_assert(prev->plh_magic == PLENT_MAGIC);
	psc_assert(next->plh_magic == PLENT_MAGIC);

	psc_assert(psc_lentry_prev(e) == NULL && psc_lentry_next(e) == NULL);
	psc_assert(psc_lentry_prev(prev) && psc_lentry_next(prev));
	psc_assert(psc_lentry_prev(next) && psc_lentry_next(next));

	e->plh_owner = prev->plh_owner;
#endif

	psc_lentry_prev(next) = e;
	psc_lentry_next(e) = next;
	psc_lentry_prev(e) = prev;
	psc_lentry_next(prev) = e;
}

#define psclist_add_head(e, hd)		_psclist_add((e), (hd), psc_listhd_first(hd))
#define psclist_add_tail(e, hd)		_psclist_add((e), psc_listhd_last(hd), (hd))
#define psclist_add_after(e, before)	_psclist_add((e), (before), psc_lentry_next(before))
#define psclist_add_before(e, after)	_psclist_add((e), psc_lentry_prev(after), (after))

#define psclist_add(e, hd)		psclist_add_tail((e), (hd))

/**
 * psclist_del - Delete an entry from the list it is contained within.
 * @e: the entry to remove from a list.
 * @hd: containing list head.
 */
static __inline void
psclist_del(struct psclist_head *e, __unusedx const void *hd)
{
	struct psc_listentry *prev, *next;

#if PFL_DEBUG
	psc_assert(e->plh_owner == hd);
	psc_assert(e->plh_magic == PLENT_MAGIC);
	psc_assert(psc_lentry_prev(e) && psc_lentry_next(e));
	if (psc_lentry_prev(e) == psc_lentry_next(e))
		psc_assert(psc_lentry_prev(e) == hd);
#else
	(void)hd;
#endif

	prev = psc_lentry_prev(e);
	next = psc_lentry_next(e);

	psc_lentry_next(prev) = next;
	psc_lentry_prev(next) = prev;

	psc_lentry_next(e) = psc_lentry_prev(e) = NULL;

#if PFL_DEBUG
	e->plh_owner = NULL;
#endif
}

/**
 * psc_listhd_empty - Tests whether a list has no items.
 * @hd: the list head to test.
 */
static __inline int
psc_listhd_empty(const struct psclist_head *hd)
{
#if PFL_DEBUG
	psc_assert(hd->plh_magic == PLENT_MAGIC);
	psc_assert(hd->plh_owner == hd);
	psc_assert(psc_listhd_first(hd) && psc_listhd_last(hd));
	if (psc_listhd_first(hd) == hd)
		psc_assert(psc_listhd_last(hd) == hd);
	if (psc_listhd_last(hd) == hd)
		psc_assert(psc_listhd_first(hd) == hd);
#endif
	return (psc_listhd_first(hd) == hd);
}

/**
 * psclist_disjoint - Test whether a psc_listentry is not on a list.
 * @entry: the psc_listentry to test.
 */
static __inline int
psclist_disjoint(const struct psc_listentry *e)
{
#if PFL_DEBUG
	psc_assert(e->plh_magic == PLENT_MAGIC);
	if (psc_lentry_prev(e))
		psc_assert(psc_lentry_next(e) && e->plh_owner);
	if (psc_lentry_next(e))
		psc_assert(psc_lentry_prev(e) && e->plh_owner);
	if (e->plh_owner)
		psc_assert(psc_lentry_prev(e) && psc_lentry_next(e));
#endif
	return (psc_lentry_prev(e) == NULL);
}

#define psclist_conjoint(e, hd)	_psclist_conjoint(PFL_CALLERINFO(), (e), (hd))

/**
 * psclist_conjoint - Test whether a psc_listentry is on a list.
 * @e: the psc_listentry to test.
 * @hd: the psclist_head the entry must be on.
 */
#define _pfl_callerinfo pci
static __inline int
_psclist_conjoint(const struct pfl_callerinfo *pci,
    const struct psc_listentry *e, const struct psclist_head *hd)
{
#if PFL_DEBUG
	psc_assert(e->plh_magic == PLENT_MAGIC);
	if (hd == NULL) {
		psclog_warnx("conjoint passed NULL");
		hd = e->plh_owner;
	}
	if (psc_lentry_prev(e))
		psc_assert(psc_lentry_next(e) && hd == e->plh_owner);
	if (psc_lentry_next(e))
		psc_assert(psc_lentry_prev(e) && hd == e->plh_owner);
	if (psc_lentry_prev(e) == psc_lentry_next(e) && psc_lentry_prev(e))
		psc_assert(psc_lentry_prev(e) == (hd));
	if (e->plh_owner)
		psc_assert(psc_lentry_prev(e) && psc_lentry_next(e));
#else
	(void)hd;
	(void)pci;
#endif
	return (psc_lentry_prev(e) != NULL);
}
#undef _pfl_callerinfo

#define psclist_entry2(p, offset)						\
	((struct psc_listentry *)((char *)(p) + (offset)))

#define psclist_entry(p, type, memb)						\
	psclist_entry2((p), offsetof(type, memb))

/**
 * psc_lentry_obj2 - Get the struct for a list entry given offset.
 * @e: the psc_listentry.
 * @type: the type of the struct this entry is embedded in.
 * @memb: the structure member name of the psc_listentry.
 */
#define psc_lentry_obj2(e, type, offset)					\
	((type *)((char *)(e) - (offset)))

/**
 * psc_lentry_obj - Get the struct for a list entry.
 * @e: the psc_listentry.
 * @type: the type of the struct this entry is embedded in.
 * @memb: the structure member name of the psc_listentry.
 */
#define psc_lentry_obj(e, type, memb)						\
	psc_lentry_obj2((e), type, offsetof(type, memb))

#if PFL_DEBUG
#  define psc_lentry_hd(e)		(e)->plh_owner
#else
#  define psc_lentry_hd(e)		NULL
#endif

/**
 * psc_listhd_first_obj - Grab first item from a list head or NULL if empty.
 * @hd: list head.
 * @type: structure type containing a psc_listentry.
 * @memb: structure member name of psc_listentry.
 */
#define psc_listhd_first_obj(hd, type, memb)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj(psc_listhd_first(hd), type, memb))

#define psc_listhd_first_obj2(hd, type, offset)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj2(psc_listhd_first(hd), type, offset))

/**
 * psc_listhd_last_obj - Grab last item from a list head or NULL if empty.
 * @hd: list head.
 * @type: structure type containing a psc_listentry.
 * @memb: structure member name of psc_listentry.
 */
#define psc_listhd_last_obj(hd, type, memb)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj(psc_listhd_last(hd), type, memb))

#define psc_listhd_last_obj2(hd, type, offset)					\
	(psc_listhd_empty(hd) ? NULL :						\
	 psc_lentry_obj2(psc_listhd_last(hd), type, offset))

static __inline void *
_psclist_next_obj(struct psclist_head *hd, void *p,
    unsigned long offset, int wantprev)
{
	struct psc_listentry *e, *n;

	psc_assert(p);
	e = (void *)((char *)p + offset);

	/*
	 * Ensure integrity of entry: must be contained in
	 * a list and must not inadvertenly be a head!
	 */
	psc_assert(psc_lentry_next(e) && psc_lentry_prev(e));
	n = wantprev ? psc_lentry_prev(e) : psc_lentry_next(e);
	psc_assert(n != e);
	if (n == hd)
		return (NULL);
	return ((char *)n - offset);
}

/**
 * psclist_prev_obj - Grab the item before the specified item on a list.
 * @hd: head of the list.
 * @p: item on list.
 * @memb: psc_listentry member name in structure.
 */
#define psclist_prev_obj(hd, p, memb)						\
	_psclist_next_obj((hd), (p), offsetof(typeof(*(p)), memb), 1)

#define psclist_prev_obj2(hd, p, offset)					\
	_psclist_next_obj((hd), (p), (offset), 1)

/**
 * psclist_next_obj - Grab the item following the specified item on a list.
 * @hd: head of the list.
 * @p: item on list.
 * @memb: psc_listentry member name in structure.
 */
#define psclist_next_obj(hd, p, memb)						\
	_psclist_next_obj((hd), (p), offsetof(typeof(*(p)), memb), 0)

#define psclist_next_obj2(hd, p, offset)					\
	_psclist_next_obj((hd), (p), (offset), 0)

/**
 * psclist_for_each - Iterate over a psclist.
 * @e: the &struct psclist_head to use as a loop counter.
 * @head: the head for your psclist.
 */
#define psclist_for_each(e, hd)							\
	for ((e) = psc_listhd_first(hd);					\
	     (e) != (hd) || ((e) = NULL);					\
	     (e) = psc_lentry_next(e))

/**
 * psclist_for_each_safe - Iterate over a list safe against removal
 *	of the iterating entry.
 * @e: the entry to use as a loop counter.
 * @n: another entry to use as temporary storage.
 * @hd: the head of the list.
 */
#define psclist_for_each_safe(e, n, hd)						\
	for ((e) = psc_listhd_first(hd), (n) = psc_lentry_next(e);		\
	    (e) != (hd) || ((e) = (n) = NULL);					\
	    (e) = (n), (n) = psc_lentry_next(e))

/**
 * psclist_for_each_entry_safe - Iterate over list of given type safe
 *	against removal of list entry
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry_safe(p, n, hd, memb)				\
	for ((p) = psc_listhd_first_obj((hd), typeof(*(p)), memb),		\
	     (n) = (p) ? psclist_next_obj((hd), (p), memb) : NULL;		\
	     (p); (p) = (n), (n) = (n) ? psclist_next_obj((hd), (n), memb) : NULL)

/**
 * psclist_for_each_backwards - Iterate over a psclist.
 * @e: the &struct psclist_head to use as a loop counter.
 * @head: the head for your psclist.
 */
#define psclist_for_each_backwards(e, hd)					\
	for ((e) = psc_listhd_last(hd);						\
	     (e) != (hd) || ((e) = NULL);					\
	     (e) = psc_lentry_prev(e))

/**
 * psclist_for_each_entry_safe_backwards - Iterate backwards over a list safe
 *	against removal of entries.
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry_safe_backwards(p, n, hd, memb)			\
	for ((p) = psc_listhd_first_obj((hd), typeof(*(p)), memb),		\
	     (n) = (p) ? psclist_prev_obj((hd), (p), memb) : NULL;		\
	     (p); (p) = (n), (n) = (n) ? psclist_prev_obj((hd), (n), memb) : NULL)

/**
 * psclist_for_each_entry - Iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry(p, hd, memb)					\
	for ((p) = psc_listhd_first_obj((hd), typeof(*(p)), memb);		\
	     (p); (p) = psclist_next_obj((hd), (p), memb))

/**
 * psclist_for_each_entry_backwards - Iterate backwards over a list.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry_backwards(p, hd, memb)				\
	for ((p) = psc_listhd_last_obj((hd), typeof(*(p)), memb);		\
	     (p); (p) = psclist_prev_obj((hd), (p), memb))

/**
 * psclist_for_each_entry2 - Iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2(p, hd, offset)					\
	for ((p) = psc_listhd_first_obj2((hd), typeof(*(p)), (offset));		\
	     (p); (p) = psclist_next_obj2((hd), (p), (offset)))

/**
 * psclist_for_each_entry2_backwards - Iterate backwards over a list.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_backwards(p, hd, offset)			\
	for ((p) = psc_listhd_last_obj2((hd), typeof(*(p)), (offset));		\
	     (p); (p) = psclist_prev_obj2((hd), (p), (offset)))

/**
 * psclist_for_each_entry2_safe - iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_safe(p, n, hd, offset)				\
	for ((p) = psc_listhd_first_obj2((hd), typeof(*(p)), (offset)),		\
	     (n) = (p) ? psclist_next_obj2((hd), (p), (offset)) : NULL;		\
	     (p); (p) = (n), (n) = (n) ? psclist_next_obj2((hd), (n), (offset)) : NULL)

/**
 * psclist_for_each_entry2_backwards_safe - iterate over list of given type.
 * @p: the type * to use as a loop counter.
 * @n: another type * to use as temporary storage
 * @hd: the head for your list.
 * @offset: offset into type * of list entry.
 */
#define psclist_for_each_entry2_backwards_safe(p, n, hd, offset)				\
	for ((p) = psc_listhd_last_obj2((hd), typeof(*(p)), (offset)),		\
	     (n) = (p) ? psclist_prev_obj2((hd), (p), (offset)) : NULL;		\
	     (p); (p) = (n), (n) = (n) ? psclist_prev_obj2((hd), (n), (offset)) : NULL)

/**
 * psclist_for_each_entry2_cont - Continue iterating over list of given type.
 * @p: the type * to use as a loop counter.
 * @hd: the head for your list.
 * @memb: list entry member of structure.
 */
#define psclist_for_each_entry2_cont(p, hd, offset)				\
	for (; (p); (p) = psclist_next_obj2((hd), (p), (offset)))

void psclist_sort(void **, struct psclist_head *, int, ptrdiff_t,
	void (*)(void *, size_t, size_t, int (*)(const void *, const void *)),
	int (*)(const void *, const void *));

static __inline void
psclist_add_sorted(struct psclist_head *hd, struct psc_listentry *e,
    int (*cmpf)(const void *, const void *), ptrdiff_t offset)
{
	struct psclist_head *t;

	psc_assert(e);
	psclist_for_each(t, hd)
		if (cmpf((char *)e - offset, (char *)t - offset) < 0) {
			psclist_add_before(e, t);
			return;
		}
	psclist_add(e, hd);
}

static __inline void
psclist_add_sorted_backwards(struct psclist_head *hd, struct psc_listentry *e,
    int (*cmpf)(const void *, const void *), ptrdiff_t offset)
{
	struct psclist_head *t;

	psc_assert(e);
	psclist_for_each_backwards(t, hd)
		if (cmpf((char *)e - offset, (char *)t - offset) > 0) {
			psclist_add_after(e, t);
			return;
		}
	psclist_add_head(e, hd);
}

#endif /* _PFL_LIST_H_ */
