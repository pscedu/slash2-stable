/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = getpeereid;
	exit(0);
}
