/* $Id$ */

#include <sys/socket.h>

#include <stdlib.h>

int
main(int argc, char *argv[])
{
	struct sockaddr sa;

	(void)argc;
	(void)argv;
	sa.sa_len = 0;
	exit(0);
}
