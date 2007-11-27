/* $Id: zestSymtable.h 2195 2007-11-08 16:35:41Z yanovich $ */

#include <stddef.h> /* offsetof() */

#include "zestConfig.h"

#define TABENT(name, type, max, field) \
	{ name, ZEST_VARIABLE, type, max, offsetof(znode_prof_t, field) }

/* declare and initialize the global table */
struct symtable sym_table[] = {
	TABENT("bdcon_size", ZEST_SIZET, PATH_MAX, znprof_bdcon_sz),
	TABENT("block_size", ZEST_SIZET, PATH_MAX, znprof_block_sz),
	TABENT("directio", ZEST_BOOL, PATH_MAX, znprof_dio),
	TABENT("disks",	ZEST_STRING, DEVNAMEMAX, znprof_devices),
	TABENT("ndisks_needed", ZEST_INT, PATH_MAX, znprof_ndisks),
	TABENT("num_blocks", ZEST_INT, PATH_MAX, znprof_nblocks),
	TABENT("object_root", ZEST_STRING, PATH_MAX, znprof_objroot),
	TABENT("parity_device", ZEST_STRING, DEVNAMEMAX, znprof_prtydev),
	TABENT("sector_size", ZEST_SIZET, PATH_MAX, znprof_sect_sz),
	TABENT("setuuid", ZEST_HEXU64, 16, znprof_setuuid),
	TABENT("sgio", ZEST_BOOL, PATH_MAX, znprof_sgio),
	TABENT("sgiosz", ZEST_SIZET, PATH_MAX, znprof_sgio_sz),
	{ NULL, 0, 0, 0, 0 }
};
