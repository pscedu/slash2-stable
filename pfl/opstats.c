/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
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

int			pfl_opstats_sum;
struct psc_spinlock	pfl_opstats_lock = SPINLOCK_INIT;
struct psc_dynarray	pfl_opstats = DYNARRAY_INIT;
__static char		pfl_opstat_name[128];

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
	int sz, pos;
	va_list ap;
	char *name = pfl_opstat_name;

	spinlock(&pfl_opstats_lock);

	va_start(ap, namefmt);
	sz = vsnprintf(name, 128, namefmt, ap) + 1;
	va_end(ap);

	/* (gdb) p ((struct pfl_opstat *)pfl_opstats.pda_items[74]).opst_name */
	pos = psc_dynarray_bsearch(&pfl_opstats, name, _pfl_opstat_cmp);
	if (pos < psc_dynarray_len(&pfl_opstats)) {
		opst = psc_dynarray_getpos(&pfl_opstats, pos);
		if (strcmp(name, opst->opst_name) == 0) {
			psc_assert((flags & OPSTF_EXCL) == 0);
			freelock(&pfl_opstats_lock);
			return (opst);
		}
	}
	pfl_opstats_sum++;
	opst = PSCALLOC(sizeof(*opst) + sz);
	strlcpy(opst->opst_name, name, 128);
	opst->opst_flags = flags;
	psc_dynarray_splice(&pfl_opstats, pos, 0, &opst, 1);
	freelock(&pfl_opstats_lock);
	return (opst);
}

void
pfl_opstat_destroy_pos(int pos)
{
	struct pfl_opstat *opst;

	LOCK_ENSURE(&pfl_opstats_lock);
	opst = psc_dynarray_getpos(&pfl_opstats, pos);
	psc_dynarray_splice(&pfl_opstats, pos, 1, NULL, 0);
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

__static const char *
_pfl_opstats_base2_suffix(int64_t *val)
{
	const char *suffixes = "bkmgtpezyx";
	const char *suffix = suffixes;
	int i;

	if (*val < 1024)
		return ("");

	for (i = 0; i < nitems(suffixes) && *val >= 1024; i++, suffix++)
		*val /= 1024;
	return (suffix);
}

void
pfl_opstats_grad_init(struct pfl_opstats_grad *og, int flags,
    int64_t *buckets, int nbuckets, const char *fmt, ...)
{
	const char *lower_suffix = "", *upper_suffix = "";
	int64_t lower_bound, upper_bound;
	struct pfl_opstat_bucket *ob;
	char label[16];
	int rc, i;

	og->og_buckets = PSCALLOC(nbuckets * sizeof(og->og_buckets[0]));
	og->og_nbuckets = nbuckets;

	for (i = 0, ob = og->og_buckets; i < nbuckets; i++, ob++) {
		if (i)
			psc_assert(buckets[i - 1] < buckets[i]);
		else
			psc_assert(buckets[i] == 0);

		lower_bound = buckets[i];
		if (!(flags & OPSTF_BASE10))
			lower_suffix = _pfl_opstats_base2_suffix(
			    &lower_bound);

		if (i == nbuckets - 1) {
			rc = snprintf(label, sizeof(label),
			    "%d:>=%"PRId64"%.1s", i, lower_bound,
			    lower_suffix);
		} else {
			upper_bound = buckets[i + 1];
			if (!(flags & OPSTF_BASE10))
				upper_suffix =
				    _pfl_opstats_base2_suffix(
					&upper_bound);

			rc = snprintf(label, sizeof(label),
			    "%d:%"PRId64"%.1s-<%"PRId64"%.1s", i,
			    lower_bound, lower_suffix,
			    upper_bound, upper_suffix);
		}
		if (rc == -1)
			psc_fatal("snprintf");
		ob->ob_lower_bound = buckets[i];
		ob->ob_opst = pfl_opstat_initf(flags | OPSTF_BASE10,
		    fmt, label);
	}
}

void
pfl_opstats_grad_destroy(struct pfl_opstats_grad *og)
{
	struct pfl_opstat_bucket *ob;
	int i;

	for (i = 0, ob = og->og_buckets; i < og->og_nbuckets; i++, ob++)
		pfl_opstat_destroy(ob->ob_opst);
	PSCFREE(og->og_buckets);
}
