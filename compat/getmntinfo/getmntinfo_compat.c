/* $Id$ */

#include <fstab.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = getmntinfo;
	exit(0);
}
