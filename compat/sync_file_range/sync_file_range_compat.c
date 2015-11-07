/* $Id$ */

#include <fcntl.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = sync_file_range;
	exit(0);
}
