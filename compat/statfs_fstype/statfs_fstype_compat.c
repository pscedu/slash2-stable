/* $Id$ */

#include <sys/param.h>
#include <sys/mount.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)((struct statfs *)NULL)->f_fstypename;
	exit(0);
}
