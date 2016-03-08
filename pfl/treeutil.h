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

#ifndef _PFL_TREEUTIL_H_
#define _PFL_TREEUTIL_H_

#include <stddef.h>

#define PSC_SPLAY_XREMOVE(name, head, elm)				\
	do {								\
		if (SPLAY_REMOVE(name, (head), (elm)) != (elm))		\
			psc_fatalx("SPLAY_REMOVE: item not found");	\
	} while (0)

#define PSC_SPLAY_XINSERT(name, head, elm)				\
	do {								\
		if (SPLAY_INSERT(name, (head), (elm)))			\
			psc_fatalx("SPLAY_INSERT: item with key "	\
			    "already exists");				\
	} while (0)

#define PSC_SPLAY_ENTRY_INIT(name, elm)					\
	do {								\
		*SPLAY_GETLEFT(name, elm) = NULL;			\
		*SPLAY_GETRIGHT(name, elm) = NULL;			\
	} while (0)

#define PSC_RB_XREMOVE(name, head, elm)					\
	do {								\
		if (RB_FIND(name, (head), (elm)) == NULL)		\
			psc_fatalx("RB_REMOVE: item not found");	\
		RB_REMOVE(name, (head), (elm));				\
	} while (0)

#define PSC_RB_XINSERT(name, head, elm)					\
	do {								\
		if (RB_INSERT(name, (head), (elm)))			\
			psc_fatalx("RB_INSERT: item with key "		\
			    "already exists");				\
	} while (0)

#define PSC_RB_ENTRY_INIT(name, elm)					\
	do {								\
		*RB_GETLEFT(name, elm) = NULL;				\
		*RB_GETRIGHT(name, elm) = NULL;				\
	} while (0)

#endif /* _PFL_TREEUTIL_H_ */
