/* $Id$ */

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = setresuid;
	exit(0);
}
