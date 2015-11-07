/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/dynarray.h"
#include "pfl/fmtstr.h"
#include "pfl/log.h"
#include "pfl/odtable.h"
#include "pfl/pfl.h"

const char		*progname;

const char		*fmt;
int			 create_table;
int			 num_free;
int			 num_puts;
int			 overwrite;
int			 show;
struct psc_dynarray	 rcpts = DYNARRAY_INIT;
size_t			 elem_size = 128;
size_t			 nelems = 1024 * 128;

void
visit(__unusedx void *data, struct pfl_odt_receipt *r,
    void *arg)
{
	char buf[LINE_MAX], *p = data;
	struct pfl_odt **t = arg;
	static int shown_hdr;
	union {
		int	*d;
		int64_t	*q;
		void	*p;
	} u;
	size_t i;

	if (num_free) {
		struct pfl_odt_receipt *rdup;

		rdup = PSCALLOC(sizeof(*rdup));
		memcpy(rdup, r, sizeof(*r));
		psc_dynarray_add(&rcpts, rdup);
		num_free--;
	}

	if (!show)
		return;

	if (!shown_hdr) {
		struct pfl_odt_hdr *h;

		h = (*t)->odt_hdr;
		printf("nelems\t%u\n", h->odth_nelems);
		printf("elemsz\t%u\n", h->odth_objsz);
		printf("%7s %16s data\n", "slot", "crc");
		shown_hdr = 1;
	}

	printf("%7zd %16"PRIx64" ", r->odtr_elem, r->odtr_crc);

	if (fmt) {
		(void)FMTSTR(buf, sizeof(buf), fmt,
		    FMTSTRCASE('d', "d",	(u.p = p, p += sizeof(int),	*u.d))
		    FMTSTRCASE('u', "u",	(u.p = p, p += sizeof(int),	*u.d))
		    FMTSTRCASE('x', "x",	(u.p = p, p += sizeof(int),	*u.d))
		    FMTSTRCASE('q', PRId64,	(u.p = p, p += sizeof(int64_t),	*u.q))
		    FMTSTRCASE('Q', PRIu64,	(u.p = p, p += sizeof(int64_t),	*u.q))
		    FMTSTRCASE('X', PRIx64,	(u.p = p, p += sizeof(int64_t),	*u.q))
		);
		printf("%s\n", buf);
		return;
	}

	/*
	 * If the first 10 characters aren't ASCII, don't display as
	 * such.
	 */
	for (i = 0, p = data; i < 10 && p; i++, p++)
		if (!isspace(*p) && !isgraph(*p))
			goto skip;
	if (i != 10)
		goto skip;
	printf("%s\n", (char *)data);
	return;

 skip:
	for (i = 0, p = data; i < elem_size; p++, i++)
		printf("%02x", *p);
	printf("\n");
}

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-CcDosvZ] [-e elem_size] [-F #frees] [-n #puts]\n"
	    "\t[-X fmt] [-z table_size] file\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, i, verbose = 0, oflg = ODTBL_FLG_RDONLY,
	    tflg = ODTBL_OPT_CRC;
	struct pfl_odt_receipt *r;
	struct pfl_odt *t;
	char *p, *fn;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "CcDe:F:n:osvX:Zz:")) != -1)
		switch (c) {
		case 'C':
			create_table = 1;
			break;
		case 'c':
			tflg |= ODTBL_OPT_CRC;
			break;
		case 'e':
			elem_size = atoi(optarg);
			break;
		case 'F':
			num_free = atoi(optarg);
			oflg &= ~ODTBL_FLG_RDONLY;
			break;
		case 'n':
			num_puts = atoi(optarg);
			oflg &= ~ODTBL_FLG_RDONLY;
			break;
		case 'o':
			overwrite = 1;
			break;
		case 's':
			show = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'X':
			fmt = optarg;
			break;
		case 'Z':
			tflg |= ODTBL_OPT_SYNC;
			break;
		case 'z':
			nelems = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();
	fn = argv[0];

	if (create_table) {
		pfl_odt_create(fn, nelems, elem_size, overwrite,
		    0x1000, 0, tflg);
		if (verbose)
			warnx("created od-table %s "
			    "(elemsize=%zu, nelems=%zu)",
			    fn, elem_size, nelems);
		exit(0);
	}

	pfl_odt_load(&t, &pfl_odtops_mmap, oflg, fn, "%s", fn);
	pfl_odt_check(t, visit, &t);

	for (i = 0; i < num_puts; i++) {
		size_t elem;

		elem = pfl_odt_allocslot(t);
		pfl_odt_mapitem(t, elem, &p);
		snprintf(p, elem_size, "... put_number=%d ...", i);
		pfl_odt_putitem(t, elem, p);
		pfl_odt_freebuf(t, p, NULL);
	}

	DYNARRAY_FOREACH(r, i, &rcpts)
		pfl_odt_freeitem(t, r);

	pfl_odt_release(t);
	psc_dynarray_free(&rcpts);
	exit(0);
}
