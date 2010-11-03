/* $Id$ */

#include <sys/time.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = futimens;
	exit(0);
}
