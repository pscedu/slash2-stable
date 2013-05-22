/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2013, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/fmtstr.h"
#include "psc_util/log.h"
#include "psc_util/odtable.h"

struct psc_dynarray myReceipts = DYNARRAY_INIT;
const char *progname;
const char *fmt;

int	create_table;
int	num_free;
int	num_puts;
int	overwrite;
int	show;
int	dump;

size_t	elem_size  = ODT_DEFAULT_ITEM_SIZE;
size_t	table_size = ODT_DEFAULT_TABLE_SIZE;

void
odtcb_show(void *data, struct odtable_receipt *odtr)
{
	char buf[LINE_MAX], *p = data;
	union {
		int	*d;
		int64_t	*q;
		void	*p;
	} u;
	size_t i;

	printf("%7zd %16"PRIx64" ", odtr->odtr_elem, odtr->odtr_key);

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
	for (i = 0, p = data; i < 10 && p; i++, p++) {
		if (!isspace(*p) && !isgraph(*p))
			goto skip;
	}
	if (i != 10)
		goto skip;
	printf("%s\n", (char *)data);
	return;

 skip:
	for (i = 0, p = data; i < elem_size; p++, i++)
		printf("%02x", *p);
	printf("\n");
}

void
odtcb_free(__unusedx void *data, struct odtable_receipt *odtr)
{
	psc_dynarray_add(&myReceipts, odtr);
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
	int c, rc, i, verbose = 0, hflg = ODTBL_OPT_CRC;
	struct odtable *odt;
	char *item, *fn;

	pfl_init();
	progname = argv[0];
	elem_size = ODT_DEFAULT_ITEM_SIZE;
	while ((c = getopt(argc, argv, "CcDe:F:n:osvX:Zz:")) != -1)
		switch (c) {
		case 'C':
			create_table = 1;
			break;
		case 'c':
			hflg |= ODTBL_OPT_CRC;
			break;
		case 'D':
			dump = 1;
			break;
		case 'e':
			elem_size = atoi(optarg);
			break;
		case 'F':
			num_free = atoi(optarg);
			break;
		case 'n':
			num_puts = atoi(optarg);
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
			hflg |= ODTBL_OPT_SYNC;
			break;
		case 'z':
			table_size = atoi(optarg);
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
		rc = odtable_create(fn, table_size, elem_size,
		    overwrite, hflg);
		if (rc)
			errx(1, "create %s: %s", fn, strerror(-rc));
		if (verbose)
			warnx("created od-table %s "
			    "(elemsize=%zu, tablesize=%zu)",
			    fn, elem_size, table_size);
		exit(0);
	}

	rc = odtable_load(&odt, fn, "%s", fn);
	if (rc) {
		char *errstr;

		errstr = strerror(rc);
		if (rc == ENODEV)
			errstr = "Underlying file system does not "
			    "support mmap(2)";
		errx(1, "load %s: %s", fn, errstr);
	}

	item = PSCALLOC(elem_size);
	for (i = 0; i < num_puts; i++) {
		snprintf(item, elem_size, "... put_number=%d ...", i);
		if (odtable_putitem(odt, item, elem_size) == NULL) {
			psclog_error("odtable_putitem() failed: table full");
			break;
		}
	}
	PSCFREE(item);

	if (num_free) {
		struct odtable_receipt *odtr = NULL;

		odtable_scan(odt, odtcb_free);

		while (psc_dynarray_len(&myReceipts) && num_free--) {
			odtr = psc_dynarray_getpos(&myReceipts, 0);
			psclog_debug("got odtr=%p key=%"PRIx64" slot=%zd",
			    odtr, odtr->odtr_key, odtr->odtr_elem);

			if (!odtable_freeitem(odt, odtr))
				psc_dynarray_remove(&myReceipts, odtr);
		}

		psclog_debug("# of items left is %d",
		    psc_dynarray_len(&myReceipts));
	}

	if (show) {
		struct odtable_hdr *h;

		h = odt->odt_hdr;
		printf("nelems\t%zu\n", h->odth_nelems);
		printf("elemsz\t%zu\n", h->odth_elemsz);
		if (dump) {
			printf("%7s %16s data\n",
			    "slot", "crc");
			odtable_scan(odt, odtcb_show);
		}
	}

	exit(odtable_release(odt));
}
