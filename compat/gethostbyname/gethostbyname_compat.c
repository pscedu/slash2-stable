/* $Id$ */

#include <stdlib.h>
#include <netdb.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = gethostbyname;
	exit(0);
}
