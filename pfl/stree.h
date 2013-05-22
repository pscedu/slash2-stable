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

#ifndef _PFL_VTREE_H_
#define _PFL_VTREE_H_

struct psc_streenode {
	struct psclist_head	 ptn_sibling;
	struct psclist_head	 ptn_children;	/* branch */
	void			*ptn_data;
};

#define PSC_STREE_INIT(t)						\
	{ PSC_LISTENTRY_INIT, PSCLIST_HEAD_INIT((t).ptn_children), NULL }

#define PSC_STREE_FOREACH_CHILD(child, ptn)				\
	psclist_for_each_entry((child), &(ptn)->ptn_children, ptn_sibling)

struct psc_streenode *
     psc_stree_addchild(struct psc_streenode *, void *);
void psc_stree_init(struct psc_streenode *);

#endif /* _PFL_VTREE_H_ */
