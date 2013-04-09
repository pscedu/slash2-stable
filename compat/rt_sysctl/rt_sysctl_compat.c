/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)NET_RT_DUMP;
	exit(0);
}
