/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/opstats.h"
#include "pfl/str.h"
#include "pfl/time.h"

struct psc_spinlock	pfl_opstats_lock = SPINLOCK_INIT;
struct psc_dynarray	pfl_opstats = DYNARRAY_INIT;

int
_pfl_opstat_cmp(const void *a, const void *b)
{
	const struct pfl_opstat *opst = b;
	const char *name = a;

	return (strcmp(name, opst->opst_name));
}

struct pfl_opstat *
pfl_opstat_initf(int flags, const char *namefmt, ...)
{
	struct pfl_opstat *opst;
	int pos, locked;
	va_list ap;
	char *name;

	va_start(ap, namefmt);
	pfl_vasprintf(&name, namefmt, ap);
	va_end(ap);

	locked = reqlock(&pfl_opstats_lock);
	pos = psc_dynarray_bsearch(&pfl_opstats, name, _pfl_opstat_cmp);
	if (pos < psc_dynarray_len(&pfl_opstats)) {
		opst = psc_dynarray_getpos(&pfl_opstats, pos);
		if (strcmp(name, opst->opst_name) == 0) {
			psc_assert((flags & OPSTF_EXCL) == 0);
			ureqlock(&pfl_opstats_lock, locked);
			PSCFREE(name);
			return (opst);
		}
	}
	opst = PSCALLOC(sizeof(*opst));
	opst->opst_name = name;
	opst->opst_flags = flags;
	psc_dynarray_splice(&pfl_opstats, pos, 0, &opst, 1);
	ureqlock(&pfl_opstats_lock, locked);
	return (opst);
}

void
pfl_opstat_destroy_pos(int pos)
{
	struct pfl_opstat *opst;

	LOCK_ENSURE(&pfl_opstats_lock);
	opst = psc_dynarray_getpos(&pfl_opstats, pos);
	psc_dynarray_splice(&pfl_opstats, pos, 1, NULL, 0);
	PSCFREE(opst->opst_name);
	PSCFREE(opst);
}

void
pfl_opstat_destroy(struct pfl_opstat *opst)
{
	int pos;

	spinlock(&pfl_opstats_lock);
	pos = psc_dynarray_bsearch(&pfl_opstats, opst->opst_name,
	    _pfl_opstat_cmp);
	psc_assert(psc_dynarray_getpos(&pfl_opstats, pos) == opst);
	pfl_opstat_destroy_pos(pos);
	freelock(&pfl_opstats_lock);
}

void
pfl_iostats_grad_init(struct pfl_iostats_grad *ist0, int flags,
    const char *prefix)
{
	const char *suf, *nsuf, *mode = "rd";
	struct pfl_iostats_grad *ist;
	struct pfl_opstat **opst;
	uint64_t sz, nsz;
	int i, label;

	for (i = 0; i < 2; i++) {
		sz = 0;
		suf = "";
		nsuf = "K";
		label = 0;
		for (ist = ist0; ist->size; ist++, sz = nsz, label++) {
			nsz = ist->size / 1024;

			if (nsz == 1024) {
				nsuf = "M";
				nsz = 1;
			}
			opst = i ? &ist->rw.wr : &ist->rw.rd;
			*opst = pfl_opstat_initf(flags,
			    "%s-%s:%d:%d%s-<%d%s", prefix, mode, label,
			    sz, suf, nsz, nsuf);

			suf = "K";
		}

		opst = i ? &ist->rw.wr : &ist->rw.rd;
		*opst = pfl_opstat_initf(flags, "%s-%s:%d:>=%d%s", prefix,
		    mode, label, sz, nsuf);

		mode = "wr";
	}
}

void
pfl_iostats_grad_destroy(struct pfl_iostats_grad *ist0)
{
	struct pfl_iostats_grad *ist;
	struct pfl_opstat *opst;
	int i;

	for (i = 0; i < 2; i++)
		for (ist = ist0; ; ist++) {
			opst = i ? ist->rw.wr : ist->rw.rd;
			pfl_opstat_destroy(opst);
			if (!ist->size)
				break;
		}
}
