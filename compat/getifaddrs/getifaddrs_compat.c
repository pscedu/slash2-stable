/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <ifaddrs.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = getifaddrs;
	exit(0);
}
