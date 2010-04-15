/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	struct rt_msghdr m;

	m.rtm_hdrlen = 0;
	(void)argc;
	(void)argv;
	exit(0);
}
