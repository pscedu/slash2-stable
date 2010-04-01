/* $Id$ */

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = posix_memalign;
	exit(0);
}
