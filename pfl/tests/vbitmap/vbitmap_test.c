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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/vbitmap.h"

const char *progname;

#define ENSURE(vb, fmt, ...)						\
	do {								\
		char *_got_str, *_want_str, *_p;			\
		int _rc;						\
									\
		_rc = asprintf(&_want_str, fmt, ##__VA_ARGS__);		\
		if (_rc == -1)						\
			psc_fatal("asprintf");				\
		for (_p = _want_str; *_p; _p++)				\
			if (*_p == ' ')					\
				*_p = '1';				\
		_got_str = pfl_vbitmap_getbinstring(vb);		\
		if (strcmp(_got_str, _want_str))			\
		    psc_fatalx("test failed; got=%s expected=%s",	\
			_got_str, _want_str);				\
		PSCFREE(_got_str);					\
		PSCFREE(_want_str);					\
	} while (0)

#define NELEM 524288	/* # of 2MB blocks in 1TG. */

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct psc_vbitmap *vb, vba = VBITMAP_INIT_AUTO;
	size_t elem, j, cap, len, off;
	int i, c, u, t;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}

	argc -= optind;
	if (argc)
		usage();

	for (i = 0; i < 79; i++)
		if (psc_vbitmap_next(&vba, &j) != 1)
			psc_fatalx("psc_vbitmap_next failed with auto");
		else if (j != (size_t)i)
			psc_fatalx("elem %d is not supposed to be %zu", i, j);

	if ((vb = psc_vbitmap_new(213)) == NULL)
		psc_fatal("psc_vbitmap_new");

	psc_vbitmap_setrange(vb, 13, 9);
	psc_vbitmap_printbin1(vb);
	for (i = 0; i < 13; i++)
		psc_assert(psc_vbitmap_get(vb, i) == 0);
	for (j = 0; j < 9; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 1);
	for (j = 0; j < 25; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	psc_vbitmap_clearall(vb);
	for (i = 0; i < 213; i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	psc_vbitmap_setrange(vb, 25, 3);
	for (i = 0; i < 25; i++)
		psc_assert(psc_vbitmap_get(vb, i) == 0);
	for (j = 0; j < 3; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 1);
	for (j = 0; j < 25; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	psc_vbitmap_clearall(vb);
	for (i = 0; i < 213; i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);

	for (i = 0; i < 213; i++)
		if (!psc_vbitmap_next(vb, &elem))
			psc_fatalx("out of elements at pos %d", i);

	if (psc_vbitmap_next(vb, &elem))
		psc_fatalx("an unexpected extra unused elem was found; pos=%zu", elem);

	psc_vbitmap_getstats(vb, &u, &t);
	if (u != 213 || t != 213)
		psc_fatalx("wrong size, got %d,%d want %d", u, t, 213);

	psc_vbitmap_unsetrange(vb, 13, 2);
	for (i = 0; i < 13; i++)
		psc_assert(psc_vbitmap_get(vb, i) == 1);
	for (j = 0; j < 2; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 0);
	for (j = 0; j < 25; j++, i++)
	    psc_assert(psc_vbitmap_get(vb, i) == 1);

	if (psc_vbitmap_resize(vb, NELEM) == -1)
		psc_fatal("psc_vbitmap_resize");

	psc_assert(psc_vbitmap_getsize(vb) == NELEM);

	/* fill up bitmap */
	for (i = 0; i < NELEM - 211; i++)
		if (!psc_vbitmap_next(vb, &elem))
			psc_fatalx("out of elements at iter %d", i);

	/* try one past end of filled bitmap */
	if (psc_vbitmap_next(vb, &elem))
		psc_fatalx("an unexpected extra unused elem was found; pos=%zu", elem);

	/* free some slots */
	for (i = 0, elem = 0; elem < NELEM; i++, elem += NELEM / 10)
		psc_vbitmap_unset(vb, elem);

	t = psc_vbitmap_nfree(vb);
	if (t != i)
		psc_fatalx("wrong number of free elements; has=%d want=%d", t, i);
	psc_vbitmap_invert(vb);
	t = psc_vbitmap_nfree(vb);
	if (t != NELEM - i)
		psc_fatalx("wrong number of inverted elements; has=%d want=%d",
		    t, NELEM - i);
	psc_vbitmap_invert(vb);
	t = psc_vbitmap_nfree(vb);
	if (t != i)
		psc_fatalx("wrong number of original elements; has=%d want=%d", t, i);

	/* try to re-grab the freed slots */
	for (i = 0; i <= 10; i++)
		if (!psc_vbitmap_next(vb, &elem))
			psc_fatalx("out of elements, request %d/%d", i, 10);

	/* try one past end of filled bitmap */
	if (psc_vbitmap_next(vb, &elem))
		psc_fatalx("an unexpected extra unused elem was found; pos=%zu", elem);

	psc_vbitmap_setval_range(vb, 0, NELEM, 0);
	psc_assert(pfl_vbitmap_israngeset(vb, 0, 581, 371));
	psc_assert(pfl_vbitmap_israngeset(vb, 1, 581, 371) == 0);
	psc_assert(pfl_vbitmap_israngeset(vb, 0, 581, 1));
	psc_assert(pfl_vbitmap_israngeset(vb, 1, 581, 1) == 0);
	psc_assert(pfl_vbitmap_isempty(vb));

	psc_vbitmap_setval_range(vb, 0, NELEM, 1);
	psc_assert(pfl_vbitmap_israngeset(vb, 1, 581, 371));
	psc_assert(pfl_vbitmap_israngeset(vb, 0, 581, 371) == 0);
	psc_assert(pfl_vbitmap_israngeset(vb, 1, 581, 1));
	psc_assert(pfl_vbitmap_israngeset(vb, 0, 581, 1) == 0);
	psc_assert(psc_vbitmap_isfull(vb));

	psc_vbitmap_free(vb);

	vb = psc_vbitmap_newf(0, PVBF_AUTO);
	psc_assert(vb);
	psc_assert(pfl_vbitmap_isempty(vb));
	psc_assert(psc_vbitmap_getsize(vb) == 0);
	psc_assert(psc_vbitmap_resize(vb, 6) == 0);
	psc_assert(psc_vbitmap_getsize(vb) == 6);
	psc_assert(pfl_vbitmap_isempty(vb));
	psc_vbitmap_free(vb);

	vb = psc_vbitmap_newf(0, PVBF_AUTO);
	cap = psc_vbitmap_getsize(vb);
	off = 0;
	len = 6;
	if (off + len > cap)
		psc_vbitmap_resize(vb, off + len);
	psc_vbitmap_setrange(vb, off, len);
	ENSURE(vb, "111111");
	psc_assert(pfl_vbitmap_israngeset(vb, 1, 2, 4));
	psc_assert(!pfl_vbitmap_israngeset(vb, 0, 2, 4));

	psc_vbitmap_clearall(vb);
	psc_assert(psc_vbitmap_setval_range(vb, 2, 4, 1) == 0);
	psc_assert(!pfl_vbitmap_israngeset(vb, 0, 2, 4));
	ENSURE(vb, "001111");
	psc_vbitmap_free(vb);

	vb = psc_vbitmap_new(0);
	for (i = 1; i < 101; i++) {
		if (psc_vbitmap_resize(vb, i) == -1)
			psc_fatal("psc_vbitmap_new");
		psc_vbitmap_setval(vb, i - 1, i % 2);
		psc_assert(psc_vbitmap_get(vb, i - 1) == i % 2);
	}
	psc_vbitmap_free(vb);

	for (cap = 0; cap < 100; cap++) {
		for (off = 1; off < cap; off++) {
			for (len = 1; off + len < cap; len++) {
				size_t last;

				last = cap - off - len;
				vb = psc_vbitmap_new(cap);
				psc_vbitmap_setrange(vb, off, len);
				ENSURE(vb, "%0*d%*d%0*d", (int)off, 0,
				    (int)len, 1, (int)last, 0);
				psc_assert(pfl_vbitmap_israngeset(vb, 0,
				    0, off));
				psc_assert(pfl_vbitmap_israngeset(vb, 1,
				    off, len));
				psc_assert(pfl_vbitmap_israngeset(vb, 0,
				    off+len, last));
				psc_vbitmap_free(vb);
			}
		}
	}

	exit(0);
}
