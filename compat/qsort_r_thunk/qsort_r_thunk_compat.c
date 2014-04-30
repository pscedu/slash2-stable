/* $Id$ */

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	qsort_r(NULL, 0, 0, cmpf, NULL);
	exit(0);
}
