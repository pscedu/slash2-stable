/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <stdio.h>
#include <string.h>

#include "psc_ds/list.h"
#include "psc_ds/stree.h"
#include "psc_util/alloc.h"

void
psc_stree_init(struct psc_streenode *ptn)
{
	memset(ptn, 0, sizeof(*ptn));
	INIT_PSCLIST_HEAD(&ptn->ptn_children);
	INIT_PSC_LISTENTRY(&ptn->ptn_sibling);
}

struct psc_streenode *
psc_stree_addchild(struct psc_streenode *ptn, void *data)
{
	struct psc_streenode *child;

	child = PSCALLOC(sizeof(*child));
	INIT_PSCLIST_HEAD(&child->ptn_children);
	INIT_PSC_LISTENTRY(&child->ptn_sibling);
	child->ptn_data = data;
	psclist_add(&child->ptn_sibling, &ptn->ptn_children);
	return (child);
}
