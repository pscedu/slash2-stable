/* $Id$ */

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)setresuid;
	exit(0);
}
