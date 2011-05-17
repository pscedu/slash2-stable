/* $Id$ */

#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	const void *p;

	(void)argc;
	(void)argv;
	p = setproctitle;
	exit(0);
}
