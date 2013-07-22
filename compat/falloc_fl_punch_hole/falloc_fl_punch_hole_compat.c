/* $Id$ */

#include <linux/falloc.h>

#include <fcntl.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)FALLOC_FL_PUNCH_HOLE;
	exit(0);
}
