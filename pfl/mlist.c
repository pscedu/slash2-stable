/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
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

/*
 * mlists are psc_listcaches that can interface with multiwaits.
 */

#include <stdarg.h>
#include <string.h>

#include "pfl/opstats.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/mlist.h"
#include "pfl/multiwait.h"

struct psc_lockedlist psc_mlists =
    PLL_INIT(&psc_mlists, struct psc_mlist, pml_lentry);

/**
 * psc_mlist_add - Put an item on an mlist.
 * @pml: the mlist to access.
 * @p: item to return.
 */
void
_psc_mlist_add(const struct pfl_callerinfo *pci, struct psc_mlist *pml,
    void *p, int tail)
{
	int locked;

	locked = MLIST_RLOCK(pml);
	if (tail)
		pll_addtail(&pml->pml_pll, p);
	else
		pll_addhead(&pml->pml_pll, p);
	pfl_opstat_incr(pml->pml_nseen);
	psc_multiwaitcond_wakeup(&pml->pml_mwcond_empty);
	MLIST_URLOCK(pml, locked);
}

/**
 * _psc_mlist_initv - Initialize an mlist.
 * @pml: mlist to initialize.
 * @flags: multiwaitcond flags.
 * @mwcarg: multiwaitcond to use for availability notification.
 * @offset: offset into entry for the psclist_head for list mgt.
 * @fmt: printf(3) format for mlist name.
 * @ap: va_list of arguments to mlist name printf(3) format.
 */
void
_psc_mlist_initv(struct psc_mlist *pml, int flags, void *mwcarg,
    ptrdiff_t offset, const char *fmt, va_list ap)
{
	int rc;

	memset(pml, 0, sizeof(*pml));
	INIT_PSC_LISTENTRY(&pml->pml_lentry);
	_pll_initf(&pml->pml_pll, offset, NULL, 0);

	rc = vsnprintf(pml->pml_name, sizeof(pml->pml_name), fmt, ap);
	if (rc == -1)
		psc_fatal("vsnprintf");
	if (rc >= (int)sizeof(pml->pml_name))
		psc_fatalx("mlist name is too long: %s", fmt);

	psc_multiwaitcond_init(&pml->pml_mwcond_empty, mwcarg, flags,
	    "%s-empty", pml->pml_name);
	pml->pml_nseen = pfl_opstat_initf(OPSTF_BASE10,
	    "mlist.%s.seen", pml->pml_name);
}

void
_psc_mlist_init(struct psc_mlist *pml, int flags, void *mwcarg,
    ptrdiff_t offset, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psc_mlist_initv(pml, flags, mwcarg, offset, fmt, ap);
	va_end(ap);
}

/**
 * psc_mlist_reginit - Register an mlist for external control.
 * @pml: mlist to register.
 */
void
psc_mlist_register(struct psc_mlist *pml)
{
	PLL_LOCK(&psc_mlists);
	MLIST_LOCK(pml);
	pll_addtail(&psc_mlists, pml);
	MLIST_ULOCK(pml);
	PLL_ULOCK(&psc_mlists);
}

void
_psc_mlist_reginit(struct psc_mlist *pml, int flags, void *mwcarg,
    ptrdiff_t offset, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psc_mlist_initv(pml, flags, mwcarg, offset, fmt, ap);
	va_end(ap);

	psc_mlist_register(pml);
}
