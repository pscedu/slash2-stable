/* $Id$ */

#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "pfl.h"
#include "psc_ds/dynarray.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/odtable.h"

struct dynarray myReceipts;

__dead void
usage(void)
{
	fprintf(stderr, "Usage: odtable -N table [-C create_table] "
		"[-l load_table] [-c enable_crc] [-s scan_table] "
		"[-n # to put] [-z table_size] [-e element_size] "
		"[-f # to free]\n");
	exit(1);
}

void
my_odtcb(void *data, struct odtable_receipt *odtr)
{
	char *item = data;

	psc_warnx("found ;%s; at slot=%"_P_U64"d odtr=%p",
		  item, odtr->odtr_elem, odtr);

	dynarray_add(&myReceipts, odtr);
}

int
main(int argc, char *argv[])
{
	int rc, i;
	char c;

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
	char item[elem_size];

	pfl_init();

	while ((c = getopt(argc, argv, "Ccls:N:n:z:e:f:")) != -1)
		switch (c) {
                case 'C':
			create_table = 1;
                        break;
                case 'l':
			load_table = 1;
                        break;
                case 'c':
			crc_enabled = 1;
                        break;
                case 'N':
			table_name = optarg;
                        break;
		case 's':
			scan_table = atoi(optarg);
			break;
		case 'n':
			num_puts = atoi(optarg);
			break;
		case 'z':
			table_size = atoi(optarg);
			break;
		case 'e':
			elem_size = atoi(optarg);
			break;
		case 'f':
			num_free = atoi(optarg);
			break;
		default:
			usage();
		}

	dynarray_init(&myReceipts);

	if (!table_name)
		usage();

	if (create_table &&
	    (rc = odtable_create(table_name, table_size, elem_size)))
		psc_fatal("odtable_create() failed on ;%s; rc=%d",
			  table_name, rc);

	if (load_table &&
	    (rc = odtable_load(table_name, &odt)))
		psc_fatal("odtable_load() failed rc=%d", rc);


	if (scan_table)
		odtable_scan(odt, my_odtcb);


	for (i=0; i < num_puts; i++) {
		snprintf(item, elem_size, "... put_number=%d ...", i);
		if (!odtable_putitem(odt, (void *)item))
			psc_errorx("odtable_putitem() failed, no slots available");
	}

	if (num_free) {
		struct odtable_receipt *odtr = NULL;

		while (dynarray_len(&myReceipts) && num_free--) {
			odtr = dynarray_getpos(&myReceipts, 0);
			psc_warnx("got odtr=%p key=%"_P_U64"x slot=%"_P_U64"d",
				  odtr, odtr->odtr_key, odtr->odtr_elem);

			if (!odtable_freeitem(odt, odtr))
				dynarray_remove(&myReceipts, odtr);
		}

		psc_warnx("# of items left is %d", dynarray_len(&myReceipts));
	}

	rc = odtable_release(odt);
	return (rc);
}
