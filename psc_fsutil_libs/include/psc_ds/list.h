/* $Id$ */

/* XXX add GPL copyright */

#ifndef _PFL_LIST_H_
#define _PFL_LIST_H_

#ifndef HAVE_PSC_LIST_CORE
#define HAVE_PSC_LIST_CORE

#include <stdio.h>

#include "psc_util/assert.h"

/* -*- Mode: C; tab-width: 8 -*- */

/* I stole this out of the kernel :P -pauln */

/*
 * Simple doubly linked psclist implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole psclists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct psclist_head {
	struct psclist_head *znext, *zprev;
};

#define PSCLIST_INIT_CHECK(l) (((l)->zprev == NULL) && ((l)->znext == NULL))

#define PSCLIST_HEAD_INIT(name)	{ &(name), &(name) }
#define PSCLIST_ENTRY_INIT(name)	{ NULL, NULL }

#define PSCLIST_HEAD(name) \
	struct psclist_head name = PSCLIST_HEAD_INIT(name)

#define INIT_PSCLIST_HEAD(ptr)		\
	do {				\
		(ptr)->znext = (ptr);	\
		(ptr)->zprev = (ptr);	\
	} while (0)

#define INIT_PSCLIST_ENTRY(ptr)		\
	do {				\
		(ptr)->znext = NULL;	\
		(ptr)->zprev = NULL;	\
	} while (0)

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal psclist manipulation where we know
 * the prev/next entries already!
 */
static __inline__ void
__psclist_add(struct psclist_head *new, struct psclist_head *prev,
	struct psclist_head *next)
{
#if 0
	psc_assert(new->zprev == NULL && new->znext == NULL);
#endif
	next->zprev = new;
	new->znext = next;
	new->zprev = prev;
	prev->znext = new;
}

/**
 * psclist_add - add a new entry
 * @new: new entry to be added
 * @head: psclist head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static __inline__ void
psclist_add(struct psclist_head *new, struct psclist_head *head)
{
	__psclist_add(new, head, head->znext);
}

/**
 * psclist_xadd - add an element to a list and check for exclusive membership.
 * @new: entry to be added
 * @head: psclist_head to add it after
 *
 * Insert a new entry after the specified head.
 * Ensure entry doesn't already belong to another list.
 * This is good for implementing stacks.
 */
static __inline__ void
psclist_xadd(struct psclist_head *new, struct psclist_head *head)
{
        psc_assert(new->zprev == NULL && new->znext == NULL);
        __psclist_add(new, head, head->znext);
}

/**
 * psclist_add_tail - add a new entry
 * @new: new entry to be added
 * @head: psclist head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static __inline__ void
psclist_add_tail(struct psclist_head *new, struct psclist_head *head)
{
	__psclist_add(new, head->zprev, head);
}

/**
 * psclist_xadd_tail - add a new entry and check for exclusive membership.
 * @new: new entry to be added
 * @head: psclist head to add it before
 *
 * Insert a new entry before the specified head.
 * Ensure entry doesn't already belong to another list.
 * This is useful for implementing queues.
 */
static __inline__ void
psclist_xadd_tail(struct psclist_head *new, struct psclist_head *head)
{
        psc_assert(new->zprev == NULL && new->znext == NULL);
	__psclist_add(new, head->zprev, head);
}

/*
 * Delete a psclist entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal psclist manipulation where we know
 * the prev/next entries already!
 */
static __inline__ void
__psclist_del(struct psclist_head *prev, struct psclist_head *next)
{
	next->zprev = prev;
	prev->znext = next;
}

/**
 * psclist_del - deletes entry from psclist.
 * @entry: the element to delete from the psclist.
 * Note: psclist_empty on entry does not return true after this, the entry is in an undefined state.
 */
static __inline__ void
psclist_del(struct psclist_head *entry)
{
	__psclist_del(entry->zprev, entry->znext);
	entry->znext = entry->zprev = NULL;
}

/**
 * psclist_del_init - deletes entry from psclist and reinitialize it.
 * @entry: the element to delete from the psclist.
 */
static __inline__ void
psclist_del_init(struct psclist_head *entry)
{
	__psclist_del(entry->zprev, entry->znext);
	INIT_PSCLIST_HEAD(entry);
}

/**
 * psclist_empty - tests whether a psclist is empty
 * @head: the psclist to test.
 */
static __inline__ int
psclist_empty(const struct psclist_head *head)
{
#if 0
	if (head->znext == head) {
		if (head->zprev != head)
			abort();
		return (1);
	} else
		return (0);
#endif
	return head->znext == head;
}

/**
 * psclist_disjoint - tests whether a psclist entry is not a member of a list.
 * @entry: the psclist entry to test.
 */
#define psclist_disjoint(ent)	((ent)->znext == NULL && (ent)->zprev == NULL)

/**
 * psclist_splice - join two psclists
 * @psclist: the new psclist to add.
 * @head: the place to add it in the first psclist.
 */
static __inline__ void
psclist_splice(struct psclist_head *list, struct psclist_head *head)
{
	struct psclist_head *first = list->znext;

	if (first != list) {
		struct psclist_head *last = list->zprev;
		struct psclist_head *at = head->znext;

		first->zprev = head;
		head->znext = first;

		last->znext = at;
		at->zprev = last;
	}
}

/**
 * psclist_entry - get the struct for this entry
 * @ptr:	the &struct psclist_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the struct psclist_head within the struct.
 */
#define psclist_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * psclist_next_entry - get the element following the specified element.
 * @e: an element with a list member.
 * @member: the name of the struct psclist_head within the struct.
 */
#define psclist_next_entry(e, memb) \
	psclist_entry(psclist_next(&e->memb), typeof(*e), memb)

/**
 * psclist_for_each	-	iterate over a psclist
 * @pos:	the &struct psclist_head to use as a loop counter.
 * @head:	the head for your psclist.
 */
#define psclist_for_each(pos, head) \
	for ((pos) = (head)->znext; (pos) != (head); \
		(pos) = (pos)->znext)

/**
 * psclist_for_each_safe	-	iterate over a psclist safe against removal of psclist entry
 * @pos:	the &struct psclist_head to use as a loop counter.
 * @n:		another &struct psclist_head to use as temporary storage
 * @head:	the head for your psclist.
 */
#define psclist_for_each_safe(pos, n, head) \
	for (pos = (head)->znext, n = pos->znext; pos != (head); \
		pos = n, n = pos->znext)

#endif /* HAVE_PSC_LIST_CORE */

/**
 * psclist_first -	grab first entry from a psclist
 * @head:	the head for your psclist.
 */
#define psclist_first(head) (head)->znext

/**
 * psclist_first_entry - grab first item from a psclist
 * @head: list head.
 */
#define psclist_first_entry(hd, type, memb) \
	psclist_entry((hd)->znext, type, memb)

/**
 * psclist_last - grab last list entry.
 * @hd: list head.
 */
#define psclist_last(hd) (hd)->zprev

/**
 * psclist_last_entry - grab last list item.
 * @hd: list head.
 */
#define psclist_last_entry(hd, type, memb) \
	psclist_entry((hd)->zprev, type, memb)

/**
 * psclist_next -	grab the entry following the specified entry.
 * @e:	entry
 */
#define psclist_next(e) (e)->znext

/**
 * psclist_prev -	grab the entry before the specified entry.
 * @e:	entry
 */
#define psclist_prev(e) (e)->zprev

/**
 * psclist_prev_entry - grab item before specified entry.
 * @e: entry
 */
#define psclist_prev_entry(e, type, memb) \
	psclist_entry((e)->zprev, type, memb)

/**
 * psclist_for_each_entry_safe - iterate over list of given type safe
 *	against removal of list entry
 * @pos:        the type * to use as a loop counter.
 * @n:          another type * to use as temporary storage
 * @head:       the head for your list.
 * @member:     the name of the list_struct within the struct.
 */
#define psclist_for_each_entry_safe(pos, n, head, member)		     \
	for ((pos) = psclist_entry((head)->znext, typeof(*(pos)), member),   \
	    (n) = psclist_entry((pos)->member.znext, typeof(*(pos)), member);\
	    &(pos)->member != (head);					     \
	    (pos) = (n), (n) = psclist_entry((n)->member.znext, typeof(*(n)), member))

/**
 * psclist_for_each_entry - iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @hd:		the head for your list.
 * @member:	list entry member of structure.
 */
#define psclist_for_each_entry(pos, hd, member)				    \
	for ((pos) = psclist_entry((hd)->znext, typeof(*(pos)), member);    \
	    &(pos)->member != (hd);					    \
	    (pos) = psclist_entry((pos)->member.znext, typeof(*(pos)), member))

/**
 * psclist_for_each_entry2 - iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @offset:	offset into type * of list entry.
 */
#define psclist_for_each_entry2(pos, head, offset)		            \
	for ((pos) = (void *)(((char *)(head)->znext) - (offset));          \
	    ((char *)pos) + (offset) != (void *)(head);			    \
	    (pos) = (void *)(((char *)(((struct psclist_head *)(((char *)pos) + (offset)))->znext)) - (offset)))

#undef list_head
#undef LIST_HEAD_INIT
#undef LIST_ENTRY_INIT
#undef LIST_HEAD
#undef INIT_LIST_HEAD
#undef INIT_LIST_ENTRY

#undef list_add
#undef list_add_tail
#undef list_del
#undef list_del_init
#undef list_empty
#undef list_splice
#undef list_entry
#undef list_for_each
#undef list_for_each_safe

#define list_head		ERROR
#define LIST_HEAD_INIT		ERROR
#define LIST_ENTRY_INIT		ERROR
#define LIST_HEAD		ERROR
#define INIT_LIST_HEAD		ERROR
#define INIT_LIST_ENTRY		ERROR

#define list_add		ERROR
#define list_add_tail		ERROR
#define list_del		ERROR
#define list_del_init		ERROR
#define list_empty		ERROR
#define list_splice		ERROR
#define list_entry		ERROR
#define list_for_each		ERROR
#define list_for_each_safe	ERROR

#endif /* _PFL_LIST_H_ */
