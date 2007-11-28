/* $Id: subsys.c 2137 2007-11-05 18:41:27Z yanovich $ */

/*
 * zestiond subsystem definitions.
 */

#include "subsys.h"

#include <strings.h>

/* Must stay sync'd with ZS_* constants. */
const char *subsys_names[] = {
/* 0 */	"addrcache",
/* 1 */	"chunkmap",
/* 2 */	"fileops",
/* 3 */	"inode",
/* 4 */	"log",
/* 5 */	"read",
/* 6 */	"sync",
/* 7 */	"ciod",
/* 8 */	"rpc",
/* 9 */	"lnet",
/*10 */	"pty",
/*11 */	"other"
};

int
psc_subsys_id(const char *name)
{
	int n;

	for (n = 0; n < ZNSUBSYS; n++)
		if (strcasecmp(name, subsys_names[n]) == 0)
			return (n);
	return (-1);
}

const char *
psc_subsys_name(int id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= ZNSUBSYS)
		return ("<unknown>");
	return (subsys_names[id]);
}
