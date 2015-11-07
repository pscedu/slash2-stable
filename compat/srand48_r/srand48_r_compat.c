/* $Id$ */

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = srand48_r;
	exit(0);
}
