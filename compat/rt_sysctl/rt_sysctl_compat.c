/* $Id$ */

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
	(void)CTL_NET;
	(void)PF_ROUTE;
	exit(0);
}
