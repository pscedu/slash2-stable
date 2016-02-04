/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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

#include <sys/param.h>

#include <string.h>

#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/lock.h"
#include "pfl/wndmap.h"

#define WMBSZ (sizeof(((struct psc_wndmap_block *)NULL)->pwmb_buf) * NBBY)

__static struct psc_wndmap_block *
psc_wndmap_addblock(struct psc_wndmap *wm)
{
	struct psc_wndmap_block *wb;

	wb = PSCALLOC(sizeof(*wb));
	pll_addtail(&wm->pwm_wmbs, wb);
	return (wb);
}

__static int
psc_wndmap_block_empty(const struct psc_wndmap_block *wb)
{
	size_t n;

	for (n = 0; n < sizeof(*wb); n++)
		if (wb->pwmb_buf[n])
			return (0);
	return (1);
}

void
psc_wndmap_init(struct psc_wndmap *wm, size_t min)
{
	wm->pwm_min = wm->pwm_nextmin = min;
	INIT_SPINLOCK(&wm->pwm_lock);
	pll_init(&wm->pwm_wmbs, struct psc_wndmap_block,
	    pwmb_lentry, &wm->pwm_lock);
	psc_wndmap_addblock(wm);
}

int
psc_wndmap_find(struct psc_wndmap *wm, size_t pos,
    struct psc_wndmap_block **wbp, size_t *np)
{
	size_t bmin, bmax, ymin, ymax, n;
	struct psc_wndmap_block *wb;
	int j;

	LOCK_ENSURE(&wm->pwm_lock);
	bmin = wm->pwm_min;
	PLL_FOREACH(wb, &wm->pwm_wmbs) {
		bmax = bmin + (WMBSZ - 1);

		/* check if pos falls in this block */
		if ((bmin > bmax && (bmin <= pos || pos <= bmax)) ||
		    (bmin < bmax && (bmin <= pos && pos <= bmax))) {
			ymin = bmin;
			for (n = 0; n < WMBSZ; n++, ymin += NBBY) {
				ymax = ymin + (NBBY - 1);

				/* check if pos falls in this byte */
				if ((ymin > ymax && (ymin <= pos || pos <= ymax)) ||
				    (ymin < ymax && (ymin <= pos && pos <= ymax))) {
					for (j = 0; j < NBBY; j++, n++) {
						if (n == pos) {
							*wbp = wb;
							*np = n;
							return (1);
						}
					}
					psc_fatalx("shouldn't reach here");
				}
			}
			psc_fatalx("shouldn't reach here");
		}
	}
	return (0);
}

int
psc_wndmap_isset(struct psc_wndmap *wm, size_t pos)
{
	struct psc_wndmap_block *wb;
	size_t n;
	int rc = 0;

	WNDMAP_LOCK(wm);
	if (psc_wndmap_find(wm, pos, &wb, &n) &&
	    wb->pwmb_buf[n / NBBY] & (1 << (n % NBBY - 1)))
		rc = 1;
	WNDMAP_ULOCK(wm);
	return (rc);
}

void
psc_wndmap_clearpos(struct psc_wndmap *wm, size_t pos)
{
	struct psc_wndmap_block *wb;
	size_t n;

	WNDMAP_LOCK(wm);
	psc_assert(psc_wndmap_find(wm, pos, &wb, &n));
	psc_assert(wb->pwmb_buf[n / NBBY] & (1 << (n % NBBY - 1)));
	wb->pwmb_buf[n / NBBY] &= ~(1 << (n % NBBY - 1));

	if (pos == wm->pwm_nextmin)
		wm->pwm_nextmin++;

	/* if the first block is now empty, advance window */
	if (psc_wndmap_block_empty(wb)) {
		wm->pwm_min += WMBSZ;

		pll_remove(&wm->pwm_wmbs, wb);
		pll_addtail(&wm->pwm_wmbs, wb);
	}
	WNDMAP_ULOCK(wm);
}

size_t
psc_wndmap_getnext(struct psc_wndmap *wm)
{
	struct psc_wndmap_block *wb;
	size_t pos, n, j;

	WNDMAP_LOCK(wm);
	pos = wm->pwm_nextmin;
	PLL_FOREACH(wb, &wm->pwm_wmbs)
		for (n = 0; n < WMBSZ; n++) {
			if (wb->pwmb_buf[n / NBBY] != 0xff) {
				j = ffs(~wb->pwmb_buf[n / NBBY]);
				psc_assert(j != NBBY + 1);
				pos += j;
				wb->pwmb_buf[n / NBBY] |=
				    1 << (n % NBBY - 1);
				goto out;
			} else
				pos += NBBY;
		}
	/* no space available, return first bit in a new block */
	wb = psc_wndmap_addblock(wm);
	wb->pwmb_buf[0] = 1;
 out:
	WNDMAP_ULOCK(wm);
	return (pos);
}

void
psc_wndmap_free(struct psc_wndmap *wm)
{
	struct psc_wndmap_block *wb, *next;

	WNDMAP_LOCK(wm);
	PLL_FOREACH_SAFE(wb, next, &wm->pwm_wmbs) {
		pll_remove(&wm->pwm_wmbs, wb);
		PSCFREE(wb);
	}
	WNDMAP_ULOCK(wm);
}
