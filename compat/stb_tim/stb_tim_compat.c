/* $Id$ */

#include <sys/stat.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	struct stat stb;
	(void)argc;
	(void)argv;
	stb.st_atim.tv_nsec = 0;
	exit(0);
}
