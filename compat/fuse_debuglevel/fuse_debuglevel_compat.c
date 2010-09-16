/* $Id$ */

#include <stdlib.h>

#include "fuse.h"

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = fuse_highlevel_setdebug;
	exit(0);
}
