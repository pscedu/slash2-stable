/* $Id$ */

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "psc_util/odtable.h"

struct psc_dynarray myReceipts = DYNARRAY_INIT;
const char *progname;

void
my_odtcb(void *data, struct odtable_receipt *odtr)
{
	char *item = data;

	psc_warnx("found %s at slot=%"PRId64" odtr=%p",
		  item, odtr->odtr_elem, odtr);

	psc_dynarray_add(&myReceipts, odtr);
}

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-Ccls] [-e elem_size] [-f #frees] [-n #puts]\n"
	    "\t[-z table_size] -N table_file\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, rc, i;

	char *table_name = NULL;

	int create_table = 0;
	int load_table   = 1;
	int scan_table   = 1;
	int crc_enabled  = 1;
	int num_puts     = 0;
	int num_free     = 0;

	size_t table_size = 1024;
	size_t elem_size  = 128;

	struct odtable *odt;
	char *item;

	pfl_init();

	while ((c = getopt(argc, argv, "Cce:f:lN:n:sz:")) != -1)
		switch (c) {
		case 'C':
			create_table = 1;
			break;
		case 'c':
			crc_enabled = 1;
			break;
		case 'e':
			elem_size = atoi(optarg);
			break;
		case 'f':
			num_free = atoi(optarg);
			break;
		case 'l':
			load_table = 1;
			break;
		case 'N':
			table_name = optarg;
			break;
		case 'n':
			num_puts = atoi(optarg);
			break;
		case 's':
			scan_table = 1;
			break;
		case 'z':
			table_size = atoi(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	if (!table_name)
		usage();

	if (create_table &&
	    (rc = odtable_create(table_name, table_size, elem_size)))
		errx(1, "odtable_create: %s: %s", table_name,
		    strerror(-rc));

	if (load_table &&
	    (rc = odtable_load(table_name, &odt)))
		errx(1, "odtable_load: %s", strerror(-rc));

	if (scan_table)
		odtable_scan(odt, my_odtcb);

	item = psc_alloc(elem_size, 0);

	for (i=0; i < num_puts; i++) {
		snprintf(item, elem_size, "... put_number=%d ...", i);
		if (!odtable_putitem(odt, item))
			psc_errorx("odtable_putitem() failed, no slots available");
	}

	free(item);

	if (num_free) {
		struct odtable_receipt *odtr = NULL;

		while (psc_dynarray_len(&myReceipts) && num_free--) {
			odtr = psc_dynarray_getpos(&myReceipts, 0);
			psc_warnx("got odtr=%p key=%"PRIx64" slot=%"PRId64,
				  odtr, odtr->odtr_key, odtr->odtr_elem);

			if (!odtable_freeitem(odt, odtr))
				psc_dynarray_remove(&myReceipts, odtr);
		}

		psc_warnx("# of items left is %d", psc_dynarray_len(&myReceipts));
	}

	rc = odtable_release(odt);
	return (rc);
}
