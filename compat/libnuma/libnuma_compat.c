/* $Id$ */

#include <stdlib.h>
#include <numa.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = numa_bitmask_free;
	exit(0);
}
