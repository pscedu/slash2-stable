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

#include <sys/types.h>
#include <sys/wait.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/bitflag.h"
#include "pfl/pfl.h"
#include "pfl/printhex.h"

#define NENTS 5000
int buf[NENTS];

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

#define CNT_ASSERT(code, xcode)					\
	switch (fork()) {					\
	case -1:						\
		psc_fatal("fork");				\
	case 0: /* child */					\
		code;						\
		exit(0);					\
	default: /* parent */					\
		if (wait(&st) == -1)				\
			psc_fatal("wait");			\
		if (!(xcode))					\
			psc_fatalx("want %s, got %d",		\
			    # xcode, st);			\
		break;						\
	}

#define CNT_ASSERT0(code)	CNT_ASSERT(code, st == 0)
#define CNT_ASSERTA(code)	CNT_ASSERT(code, WCOREDUMP(st))

#define B0	(1 << 0)
#define B1	(1 << 1)
#define B2	(1 << 2)
#define B3	(1 << 3)
#define B4	(1 << 4)
#define B5	(1 << 5)
#define B6	(1 << 6)
#define B7	(1 << 7)

#define check(a, b)						\
	do {							\
		if ((a) != UINT64_C(b)) {			\
			printf("want %016"PRIx64": ",	\
			    UINT64_C(b));			\
			printbin(UINT64_C(b));			\
			printf("got  %016"PRIx64": ", (a));	\
			printbin(a);				\
			psc_fatalx("values don't match");	\
		}						\
	} while (0)

int
main(int argc, char *argv[])
{
	uint64_t in, out;
	int st = 0, f, c;
	int64_t v;

	pfl_init();
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	f = 0;
	CNT_ASSERT0(f = B1; psc_assert( pfl_bitstr_setchk(&f, NULL, B1, 0, 0, 0, 0)));
	CNT_ASSERT0(f = B1; psc_assert(!pfl_bitstr_setchk(&f, NULL, 0, B1, 0, 0, 0)));
	CNT_ASSERT0(f = B2; psc_assert( pfl_bitstr_setchk(&f, NULL, B2, B3, 0, 0, 0)));

	CNT_ASSERT0(psc_assert( pfl_bitstr_setchk(&f, NULL, 0, 0, B1, 0, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert(!pfl_bitstr_setchk(&f, NULL, 0, 0, B1, 0, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert(!pfl_bitstr_setchk(&f, NULL, B1|B2, 0, 0, 0, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert( pfl_bitstr_setchk(&f, NULL, 0, 0, B2, 0, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert( pfl_bitstr_setchk(&f, NULL, B1|B2, 0, 0, 0, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert( pfl_bitstr_setchk(&f, NULL, B1, 0, B3, 0, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert(f==(B1|B2|B3));
		    psc_assert( pfl_bitstr_setchk(&f, NULL, B3, 0, 0, B2, PFL_BITSTR_SETCHK_STRICT));
		    psc_assert(f==(B1|B3));
		    );

	CNT_ASSERT0(psc_assert(pfl_bitstr_setchk(&f, NULL, 0, 0, B6, 0, 0)); psc_assert(f == B6));

	v = 0xf;
	psc_assert(pfl_bitstr_nset(&v, sizeof(v)) == 4);
	v = 0xf00;
	psc_assert(pfl_bitstr_nset(&v, sizeof(v)) == 4);
	v = 0xf1f0;
	psc_assert(pfl_bitstr_nset(&v, 1) == 4);
	v = 0xffff;
	psc_assert(pfl_bitstr_nset(&v, 2) == 16);
	v = UINT64_C(0xffffffffffffffff);
	psc_assert(pfl_bitstr_nset(&v, sizeof(v)) == 64);

	out = 0;
	in = 0x7fffffff;
	pfl_bitstr_copy(&out, 0, &in, 0, NBBY * 4);
	check(out, 0x7fffffff);

	out = 0;
	in = 0x7fffffff;
	pfl_bitstr_copy(&out, 0, &in, 1, NBBY * 4);
	check(out, 0x3fffffff);

	out = 0;
	in = 0x7fffffff;
	pfl_bitstr_copy(&out, 1, &in, 0, NBBY * 4);
	check(out, 0xfffffffe);

	out = 0;
	in = 0xffffffff;
	pfl_bitstr_copy(&out, 3, &in, 10, 1);
	check(out, 0x00000008);

	out = 0;
	in = 0xffffffff;
	pfl_bitstr_copy(&out, 3, &in, 10, 13);
	check(out, 0x0000fff8);

	out = 0;
	in = 0x1b;
	pfl_bitstr_copy(&out, 22, &in, 0, 6);
	check(out, 0x6c00000);

	out = 0x1300;
	in = 0;
	pfl_bitstr_copy(&out, 14, &in, 0, 8);
	check(out, 0x1300);

	out = UINT64_C(0xffffffffffffffff);
	in = 0;
	pfl_bitstr_copy(&out, 14, &in, 0, 8);
	check(out, 0xffffffffffc03fff);

	in = 0x13;
	pfl_bitstr_copy(&out, 22, &in, 0, 6);
	check(out, 0xfffffffff4c03fff);

	in = 0x15;
	pfl_bitstr_copy(&out, 34, &in, 0, 6);
	check(out, 0xffffff57f4c03fff);

	in = 0x2a;
	pfl_bitstr_copy(&out, 34, &in, 0, 6);
	check(out, 0xffffffabf4c03fff);

	in = 0;
	pfl_bitstr_copy(&out, 34, &in, 0, 6);
	check(out, 0xffffff03f4c03fff);

	out = UINT64_C(0xffffffffffffffff);
	in = 0x12;
	pfl_bitstr_copy(&out, 8, &in, 0, 6);
	check(out, 0xffffffffffffd2ff);
	pfl_bitstr_copy(&out, 8+6+8, &in, 0, 6);
	check(out, 0xfffffffff4bfd2ff);
	pfl_bitstr_copy(&out, 8+6+8+6+8, &in, 0, 6);
	check(out, 0xfffffd2ff4bfd2ff);
	pfl_bitstr_copy(&out, 8+6+8+6+8+6+8, &in, 0, 6);

	out = 0x1300;
	in = 0x13;
	pfl_bitstr_copy(&out, 22, &in, 0, 6);
	check(out, 0x4c01300);

	out = 0;
	in = 3;
	pfl_bitstr_copy(&out, 8, &in, 0, 2);
	check(out, 0x00000300);
	pfl_bitstr_copy(&out, 18, &in, 0, 2);
	check(out, 0x000c0300);
	pfl_bitstr_copy(&out, 28, &in, 0, 2);
	check(out, 0x300c0300);

	uint64_t outbuf[4];
	memset(outbuf, 0, sizeof(outbuf));
	int repl = 3;
	pfl_bitstr_copy(&outbuf, 68, &repl, 0, 4);
	check(outbuf[0], 0);
	check(outbuf[1], 0x30);
	check(outbuf[2], 0);
	check(outbuf[3], 0);

	memset(outbuf, 0, sizeof(outbuf));
	repl = 3;
	pfl_bitstr_copy(&outbuf, 63, &repl, 0, 3);
	check(outbuf[0], 0x8000000000000000);
	check(outbuf[1], 0x1);

	exit(0);
}
