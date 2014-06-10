/* $Id$ */

#include <stdlib.h>

int
cmpf(void *t, const void *a, const void *b)
{
	(void)t;
	(void)a;
	(void)b;
	return 0;
}

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	qsort_r(NULL, 0, 0, cmpf, cmpf);
	exit(0);
}
