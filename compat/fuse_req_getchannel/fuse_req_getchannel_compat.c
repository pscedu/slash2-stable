/* $Id$ */

#include <stdlib.h>

#include "fuse_lowlevel.h"

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = fuse_req_getchannel;
	exit(0);
}
