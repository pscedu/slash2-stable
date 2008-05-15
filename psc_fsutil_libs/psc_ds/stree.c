/* $Id$ */

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
	INIT_PSCLIST_ENTRY(&ptn->ptn_sibling);
}

struct psc_streenode *
psc_stree_addchild(struct psc_streenode *ptn, void *data)
{
	struct psc_streenode *child;

	child = PSCALLOC(sizeof(*child));
	INIT_PSCLIST_HEAD(&child->ptn_children);
	child->ptn_data = data;
	psclist_xadd(&child->ptn_sibling, &ptn->ptn_children);
	return (child);

}
