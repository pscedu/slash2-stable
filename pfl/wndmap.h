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

#ifndef _PFL_WNDMAP_H_
#define _PFL_WNDMAP_H_

#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "pfl/lock.h"

struct pfl_wndmap {
	size_t			 pwm_min;	/* bottom edge of window */
	size_t			 pwm_nextmin;
	psc_spinlock_t		 pwm_lock;
	struct psc_lockedlist	 pwm_wmbs;
};

struct pfl_wndmap_block {
	struct psclist_head	 pwmb_lentry;
	unsigned char		 pwmb_buf[32];
};

#define WNDMAP_LOCK(wm)		spinlock(&(wm)->pwm_lock)
#define WNDMAP_ULOCK(wm)	freelock(&(wm)->pwm_lock)
#define WNDMAP_RLOCK(wm)	reqlock(&(wm)->pwm_lock)
#define WNDMAP_URLOCK(wm, lk)	ureqlock(&(wm)->pwm_lock, (lk))

void	pfl_wndmap_clearpos(struct pfl_wndmap *, size_t);
void	pfl_wndmap_free(struct pfl_wndmap *);
size_t	pfl_wndmap_getnext(struct pfl_wndmap *);
void	pfl_wndmap_init(struct pfl_wndmap *, size_t);
int	pfl_wndmap_isset(struct pfl_wndmap *, size_t);

#endif /* _PFL_WNDMAP_H_ */
