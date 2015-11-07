/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	exit(sizeof(((struct stat *)NULL)->st_ino) == 8);
}
