/* $Id$ */

#include <stdlib.h>
#include <ucred.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = getpeerucred;
	exit(0);
}
