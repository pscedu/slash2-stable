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

#include <stdio.h>
#include <string.h>

#include "pfl/list.h"
#include "pfl/stree.h"
#include "pfl/alloc.h"

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

struct psc_streenode *
psc_stree_addchild_sorted(struct psc_streenode *ptn, void *data,
    int (*cmpf)(const void *, const void *), off_t offset)
{
	struct psc_streenode *child;

	child = PSCALLOC(sizeof(*child));
	INIT_PSCLIST_HEAD(&child->ptn_children);
	INIT_PSC_LISTENTRY(&child->ptn_sibling);
	child->ptn_data = data;
	psclist_add_sorted(&ptn->ptn_children, &child->ptn_sibling,
	    cmpf, offset);
	return (child);
}
