/* $Id$ */

#include <sys/param.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	int n;

	(void)argc;
	(void)argv;
	n = HOST_NAME_MAX;
	exit(0);
}
