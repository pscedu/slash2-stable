/* $Id$ */

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	const void *p;

	(void)argc;
	(void)argv;
	p = setproctitle;
	exit(0);
}
