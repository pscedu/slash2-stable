/* $Id$ */

#include <stdlib.h>
#include <time.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = clock_gettime;
	exit(0);
}
