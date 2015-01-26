/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/iostats.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/str.h"
#include "pfl/time.h"

struct psc_spinlock	pfl_opstats_lock = SPINLOCK_INIT;
struct psc_dynarray	pfl_opstats = DYNARRAY_INIT;

struct pfl_opstat *
pfl_opstat_initf(int flags, const char *namefmt, ...)
{
	static psc_spinlock_t lock = SPINLOCK_INIT;
	struct pfl_opstat *opst;
	va_list ap;
	char *name;
	int i;

	va_start(ap, namefmt);
	pfl_vasprintf(&name, namefmt, ap);
	va_end(ap);

	spinlock(&lock);
	DYNARRAY_FOREACH(opst, i, &pfl_opstats)
		if (strcmp(name, opst->opst_name) == 0) {
			freelock(&lock);
			PSCFREE(name);
			return (opst);
		}
	opst = PSCALLOC(sizeof(*opst));
	opst->opst_name = name;
	opst->opst_flags = flags;
	psc_dynarray_add(&pfl_opstats, opst);
	freelock(&lock);
	return (opst);
}

void
pfl_opstat_destroy(struct pfl_opstat *opst)
{
	spinlock(&pfl_opstats_lock);
	psc_dynarray_remove(&pfl_opstats, opst);
	freelock(&pfl_opstats_lock);
	PSCFREE(opst);
}

void
pfl_iostats_grad_init(struct pfl_iostats_grad *ist0, int flags,
    const char *prefix)
{
	const char *suf, *nsuf, *mode = "rd";
	struct pfl_iostats_grad *ist;
	struct pfl_opstat **opst;
	uint64_t sz, nsz;
	int i;

	for (i = 0; i < 2; i++) {
		sz = 0;
		suf = "";
		nsuf = "K";
		for (ist = ist0; ist->size; ist++, sz = nsz) {
			nsz = ist->size / 1024;

			if (nsz == 1024) {
				nsuf = "M";
				nsz = 1;
			}
			opst = i ? &ist->rw.wr : &ist->rw.rd;
			*opst = pfl_opstat_initf(flags,
			    "%s-%s:%d%s-<%d%s", prefix, mode, sz, suf,
			    nsz, nsuf);

			suf = "K";
		}

		opst = i ? &ist->rw.wr : &ist->rw.rd;
		*opst = pfl_opstat_initf(flags, "%s-%s:>=%d%s", prefix,
		    mode, sz, nsuf);

		mode = "wr";
	}
}
