/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-2013, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Exportable lists are thread-safe doubly linked list structures that
 * are exported via a daemon control interface.
 */

#ifndef _PFL_EXPLIST_H_
#define _PFL_EXPLIST_H_

#include "pfl/list.h"
#include "pfl/lockedlist.h"

#define PEXL_NAME_MAX		32

struct psc_explist {
	struct psc_lockedlist	 pexl_pll;
	struct psc_listentry	 pexl_lentry;			/* link adjoining lists */
	char			 pexl_name[PEXL_NAME_MAX];	/* for list mgt */
	size_t			 pexl_nseen;			/* stat: total #times add() */
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
