/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2008-2015, Pittsburgh Supercomputing Center (PSC).
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
 * Exportable lists are thread-safe doubly linked list structures that
 * are exported via a daemon control interface.
 */

#ifndef _PFL_EXPLIST_H_
#define _PFL_EXPLIST_H_

#include "pfl/list.h"
#include "pfl/lockedlist.h"

struct pfl_opstat;

#define PEXL_NAME_MAX		32

struct psc_explist {
	struct psc_lockedlist	 pexl_pll;
	struct psc_listentry	 pexl_lentry;			/* link adjoining lists */
	char			 pexl_name[PEXL_NAME_MAX];	/* for list mgt */
	struct pfl_opstat	*pexl_nseen;			/* stat: total #times add() */
	struct pfl_opstat	*pexl_st_removes;		/* stat: total #times remove() */
#define pexl_flags	pexl_pll.pll_flags
#define pexl_listhd	pexl_pll.pll_listhd
#define pexl_lock	pexl_pll.pll_lock
#define pexl_lockp	pexl_pll.pll_lockp
#define pexl_nitems	pexl_pll.pll_nitems
#define pexl_offset	pexl_pll.pll_offset
};

#define PEXL_LOCK(peli)		PLL_LOCK(&(pexl)->pexl_pll)
#define PEXL_RLOCK(peli)	PLL_RLOCK(&(pexl)->pexl_pll)
#define PEXL_ULOCK(peli)	PLL_ULOCK(&(pexl)->pexl_pll)
#define PEXL_URLOCK(peli, lk)	PLL_URLOCK(&(pexl)->pexl_pll, (lk))

#endif /* _PFL_EXPLIST_H_ */
