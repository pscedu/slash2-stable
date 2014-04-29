/* $Id$ */

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = qsort_r;
	exit(0);
}
