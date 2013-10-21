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

#ifndef _PFL_WNDMAP_H_
#define _PFL_WNDMAP_H_

#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "psc_util/lock.h"

struct psc_wndmap {
	size_t			 pwm_min;	/* bottom edge of window */
	size_t			 pwm_nextmin;
	psc_spinlock_t		 pwm_lock;
	struct psc_lockedlist	 pwm_wmbs;
};

struct psc_wndmap_block {
	struct psclist_head	 pwmb_lentry;
	unsigned char		 pwmb_buf[32];
};

#define WNDMAP_LOCK(wm)		spinlock(&(wm)->pwm_lock)
#define WNDMAP_ULOCK(wm)	freelock(&(wm)->pwm_lock)
#define WNDMAP_RLOCK(wm)	reqlock(&(wm)->pwm_lock)
#define WNDMAP_URLOCK(wm, lk)	ureqlock(&(wm)->pwm_lock, (lk))

void	psc_wndmap_clearpos(struct psc_wndmap *, size_t);
void	psc_wndmap_free(struct psc_wndmap *);
size_t	psc_wndmap_getnext(struct psc_wndmap *);
void	psc_wndmap_init(struct psc_wndmap *, size_t);
int	psc_wndmap_isset(struct psc_wndmap *, size_t);

#endif /* _PFL_WNDMAP_H_ */
