/* $Id$ */

#include <stdlib.h>

#include "fuse.h"

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = fuse_req_getgroups;
	exit(0);
}
