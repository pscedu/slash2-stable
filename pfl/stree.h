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

#ifndef _PFL_VTREE_H_
#define _PFL_VTREE_H_

struct psc_streenode {
	struct psc_listentry	 ptn_sibling;	/* link in parent's list */
	struct psclist_head	 ptn_children;	/* branch */
	void			*ptn_data;
};

#define PSC_STREE_INIT(t)						\
	{ PSC_LISTENTRY_INIT, PSCLIST_HEAD_INIT((t).ptn_children), NULL }

#define PSC_STREE_FOREACH_CHILD(child, ptn)				\
	psclist_for_each_entry((child), &(ptn)->ptn_children, ptn_sibling)

struct psc_streenode *
     psc_stree_addchild(struct psc_streenode *, void *);
struct psc_streenode *
     psc_stree_addchild_sorted(struct psc_streenode *, void *,
	 int (*)(const void *, const void *), off_t);
void psc_stree_init(struct psc_streenode *);

#endif /* _PFL_VTREE_H_ */
