/* $Id$ */

/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <libcfs/kp30.h>
#include <lnet/lib-types.h>
#include <lnet/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int tcpnal_maxsendkb;

int
psc_sock_io(int s, void *p, int nob, int timeout, int wr)
{
	struct timeval now, then, tv, totv;
	unsigned char *buf = p;
	int flags, rc;

	LASSERT(nob > 0);
	/* Caller may pass a zero timeout if she thinks the socket buffer is
	 * empty enough to take the whole message immediately */

	if (gettimeofday(&then, NULL) == -1)
		LASSERT(0);

	flags = MSG_NOSIGNAL | MSG_WAITALL;
//	if (wr && !timeout)
//		flags |= MSG_DONTWAIT;

	now = then;
	for (; nob; nob -= rc, buf += rc) {
		if (timeout) {
			/* Set I/O timeout to remaining time */
			timersub(&now, &then, &tv);
			memset(&totv, 0, sizeof(totv));
			totv.tv_sec = timeout;
			timersub(&totv, &tv, &now);
			if (setsockopt(s, SOL_SOCKET,
			    (wr ? SO_SNDTIMEO : SO_RCVTIMEO),
			    &now, sizeof(now)) == -1) {
				CERROR("Can't set socket %s timeout "
				       "%ld.%06ld: %d\n",
				       (wr ? "send" : "recv"),
				       now.tv_sec, now.tv_usec, rc);
				return -errno;
			}
		}

		if (wr)
			rc = send(s, buf, nob, flags);
		else
			rc = recv(s, buf, nob, flags);
		
		if (rc == -1) {
			CERROR("failed %s rc(%d) errno(%d)", 
			       wr ? "send" : "recv", rc, errno);
			if (errno != EAGAIN && errno != EINTR)
				return (-errno);
			else
				rc = 0;

		} else if (rc == 0) {
			CERROR ("Unexpected zero rc\n");
			return (wr ? -ECONNABORTED : -ECONNRESET);
		}
		
		if (timeout) {
			if (gettimeofday(&now, NULL) == -1)
				LASSERT(0);
			if (now.tv_sec - then.tv_sec >= timeout)
				return (wr ? -EAGAIN : -ETIMEDOUT);
		}
	}
	return (0);
}

int
psc_sock_iov(int s, struct iovec *iov, int niov, int timeout, int wr)
{
	int rc, i;

	for (i = 0; i < niov; i++) {
		rc = psc_sock_io(s, iov[i].iov_base,
		    iov[i].iov_len, timeout, wr);
		if (rc)
			return (rc);
	}
	return (0);
}
