/* $Id$ */

#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = &optreset;
	exit(0);
}
