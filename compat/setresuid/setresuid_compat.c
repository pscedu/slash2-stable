/* $Id: strnlen_compat.c 11469 2010-04-19 17:27:25Z yanovich $ */

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
