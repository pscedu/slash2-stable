/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>

#include "psc_util/log.h"

void
pfl_socket_setnosig(int sock)
{
#ifdef SO_NOSIGPIPE
	socklen_t optsiz;
	int optval;

	optval = 1;
	optsiz = sizeof(optval);
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE,
	    &optval, optsiz) == -1)
		psc_error("setsockopt");
#else
	(void)sock;
#endif
}
