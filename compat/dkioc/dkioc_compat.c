/* $Id$ */

#include <sys/types.h>
#include <sys/disk.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	int n;

	(void)argc;
	(void)argv;
	n = DKIOCEJECT;
	exit(0);
}
