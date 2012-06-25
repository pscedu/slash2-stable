/* $Id$ */

#include <sys/types.h>
#include <sys/xattr.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = getxattr;
	exit(0);
}
