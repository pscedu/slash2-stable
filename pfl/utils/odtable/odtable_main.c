/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2009-2017, Pittsburgh Supercomputing Center (PSC).
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
#include "pfl/thread.h"

const char		*fmt;
int			 create_table;
int			 num_free;
int			 num_puts;
int			 overwrite;
int			 dump;
struct psc_dynarray	 rcpts = DYNARRAY_INIT;
size_t			 item_size = ODT_ITEM_SIZE;
size_t			 nitems = ODT_ITEM_COUNT;

void
visit(__unusedx void *data, int64_t item,
    void *arg)
{
	char buf[LINE_MAX], *p = data;
	struct pfl_odt **t = arg;
	static int dump_hdr;
	union {
		int	*d;
		int64_t	*q;
		void	*p;
	} u;
	size_t i;

	if (!dump)
		return;

	if (!dump_hdr) {
		struct pfl_odt_hdr *h;

		h = (*t)->odt_hdr;
		printf("nitems\t%u\n", h->odth_nitems);
		printf("itemsz\t%u\n", h->odth_itemsz);
		printf("%7s %16s data\n", "slot", "crc");
		dump_hdr = 1;
	}

	printf("item: %7zd", item);

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
	for (i = 0, p = data; i < item_size; p++, i++)
		printf("%02x", *p);
	printf("\n");
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr,
	    "usage: %s [-Ccdosv] [-F #frees] [-n #puts]\n"
	    "\t[-s item_size] [-X fmt] [-z table_size] file\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, i, rc, verbose = 0, oflg = ODTBL_FLG_RDONLY, tflg = ODTBL_OPT_CRC;
	struct pfl_odt *t;
	char *p, *fn;

	pfl_init();
	pscthr_init(0, NULL, 0, "odtable");

	while ((c = getopt(argc, argv, "CcdF:n:osvX:z:")) != -1)
		switch (c) {
		case 'C':
			create_table = 1;
			break;
		case 'c':
			tflg |= ODTBL_OPT_CRC;
			break;
		case 'd':
			dump = 1;
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
			item_size = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'X':
			fmt = optarg;
			break;
		case 'z':
			nitems = atoi(optarg);
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
		rc = pfl_odt_create(fn, nitems, item_size, overwrite,
		    ODT_ITEM_START, 0, tflg);
		if (!rc && verbose)
			warnx("created od-table %s "
			    "(elemsize=%zu, nitems=%zu)",
			    fn, item_size, nitems);
		exit(0);
	}

	pfl_odt_load(&t, &pfl_odtops, oflg, fn, "%s", fn);
	pfl_odt_check(t, visit, &t);

	for (i = 0; i < num_puts; i++) {
		size_t elem;

		elem = pfl_odt_allocslot(t);
		pfl_odt_allocitem(t, (void **)&p);
		snprintf(p, item_size, "... put_number=%d ...", i);
		pfl_odt_putitem(t, elem, p, 1);
		PSCFREE(p);
	}

	/* XXX find in-use slot to free */
	for (i = 0; i < num_free; i++)
		pfl_odt_freeitem(t, i);

	pfl_odt_release(t);
	exit(0);
}
