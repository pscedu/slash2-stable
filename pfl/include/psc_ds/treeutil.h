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

#ifndef _PFL_TREEUTIL_H_
#define _PFL_TREEUTIL_H_

#include <stddef.h>

#define PSC_SPLAY_XREMOVE(name, head, elm)				\
	do {								\
		if (name ## _SPLAY_REMOVE((head), (elm)) != (elm))	\
			psc_fatalx("SPLAY_REMOVE: item not found");	\
	} while (0)

#define PSC_SPLAY_XINSERT(name, head, elm)				\
	do {								\
		if (name ## _SPLAY_INSERT((head), (elm)))		\
			psc_fatalx("SPLAY_INSERT: item with key "	\
			    "already exists");				\
	} while (0)

#define PSC_SPLAY_ENTRY_INIT(name, elm)					\
	do {								\
		*SPLAY_GETLEFT(name, elm) = NULL;			\
		*SPLAY_GETRIGHT(name, elm) = NULL;			\
	} while (0)

#define PSC_SPLAY_ENTRY_DISJOINT(name, elm)				\
	_PSC_SPLAY_ENTRY_DISJOINT(*SPLAY_GETLEFT(name, elm),		\
	    *SPLAY_GETRIGHT(name, elm))

static __inline int
_PSC_SPLAY_ENTRY_DISJOINT(const void *left, const void *right)
{
	return (left == NULL && right == NULL);
}

#endif /* _PFL_TREEUTIL_H_ */
