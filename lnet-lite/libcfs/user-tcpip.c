/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.lustre.org/lustre/docs/GPLv2.pdf
 *
 * Please contact Xyratex Technology, Ltd., Langstone Road, Havant, Hampshire.
 * PO9 1SA, U.K. or visit www.xyratex.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2002, 2013, Xyratex Technology, Ltd . All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Some portions of Lustre® software are subject to copyrights help by Intel Corp.
 * Copyright (c) 2011-2013 Intel Corporation, Inc.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre® and the Lustre logo are registered trademarks of
 * Xyratex Technology, Ltd  in the United States and/or other countries.
 */

#if !defined(__KERNEL__) || !defined(REDSTORM)

#include <libcfs/libcfs.h>
#include <libcfs/kp30.h>

#include <sys/socket.h>
#ifdef	HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#if defined(__sun__) || defined(__sun)
#include <sys/sockio.h>
#endif
#ifndef __CYGWIN__
#include <sys/syscall.h>
#endif

#ifdef __FreeBSD__
#include <ifaddrs.h>
#endif

#include "sdp_inet.h"
#include "pfl/thread.h"

#define LNET_GETAF(nid)		(LNET_NETTYP(nid) == SDPLND ? AF_INET_SDP : AF_INET)

/*
 * Functions to get network interfaces info
 */

int
libcfs_sock_ioctl(unsigned long cmd, void *arg)
{
	int fd, rc;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (fd < 0) {
		rc = -errno;
		CERROR("socket() failed: errno==%d\n", errno);
		return rc;
	}

	rc = ioctl(fd, cmd, (void *)arg);

	close(fd);
	return rc;
}

int
libcfs_ipif_query (char *name, int *up, __u32 *ip)
{
	struct ifreq   ifr;
	int            nob;
	int            rc;
	__u32          val;
#ifdef __FreeBSD__
	char *idx;
#endif

	nob = strlen(name);
	if (nob >= IFNAMSIZ) {
		CERROR("Interface name %s too long\n", name);
		return -EINVAL;
	}

#ifdef __FreeBSD__
	idx = strchr(name, ':');
	if (idx)
		*idx++ = '\0';
#endif

	CLASSERT (sizeof(ifr.ifr_name) >= IFNAMSIZ);

	strcpy(ifr.ifr_name, name);
	rc = libcfs_sock_ioctl(SIOCGIFFLAGS, &ifr);

	if (rc != 0) {
		CERROR("Can't get flags for interface %s: %s\n", name,
		    strerror(errno));
		return rc;
	}

	if ((ifr.ifr_flags & IFF_UP) == 0) {
		CDEBUG(D_NET, "Interface %s down\n", name);
		*up = 0;
		*ip = 0;
		return 0;
	}

	*up = 1;

#ifdef __FreeBSD__
	if (idx) {
		struct ifaddrs *ifa0, *ifa;
		char *endp;
		int nidx;

		nidx = strtol(idx, &endp, 10);
		if (nidx < 0 || nidx >= INT_MAX ||
		    endp == idx || *endp)
			abort();

		rc = -1;
		errno = EADDRNOTAVAIL;

		getifaddrs(&ifa0);
		for (ifa = ifa0; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family == AF_INET &&
			    strcmp(ifa->ifa_name, name) == 0 &&
			    !nidx--) {
				rc = 0;
				memcpy(&ifr.ifr_addr, ifa->ifa_addr,
				    ifa->ifa_addr->sa_len);
				break;
			}
		}
		freeifaddrs(ifa);
	} else {
#endif

		strcpy(ifr.ifr_name, name);
		ifr.ifr_addr.sa_family = AF_INET;
		rc = libcfs_sock_ioctl(SIOCGIFADDR, &ifr);

#ifdef __FreeBSD__
	}
#endif

	if (rc != 0) {
		CERROR("Can't get IP address for interface %s\n", name);
		return rc;
	}

	val = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	*ip = ntohl(val);

	return 0;
}

void
libcfs_ipif_free_enumeration (char **names, int n)
{
	int      i;

	LASSERT (n > 0);

	for (i = 0; i < n && names[i] != NULL; i++)
		LIBCFS_FREE(names[i], IFNAMSIZ);

	LIBCFS_FREE(names, n * sizeof(*names));
}

int
libcfs_ipif_enumerate (char ***namesp)
{
	/* Allocate and fill in 'names', returning # interfaces/error */
	char          **names;
	int             nalloc;
	int             nfound;
	struct ifreq   *ifr;
	struct ifconf   ifc;
	int             rc;
	int             nob;
	int             i;


	nalloc = 16;        /* first guess at max interfaces */
	for (;;) {
		LIBCFS_ALLOC(ifr, nalloc * sizeof(*ifr));
		if (ifr == NULL) {
			CERROR ("ENOMEM enumerating up to %d interfaces\n",
				nalloc);
			rc = -ENOMEM;
			goto out0;
		}

		ifc.ifc_buf = (char *)ifr;
		ifc.ifc_len = nalloc * sizeof(*ifr);

		rc = libcfs_sock_ioctl(SIOCGIFCONF, &ifc);

		if (rc < 0) {
			CERROR ("Error %d enumerating interfaces\n", rc);
			goto out1;
		}

		LASSERT (rc == 0);

		nfound = ifc.ifc_len/sizeof(*ifr);
		LASSERT (nfound <= nalloc);

		if (nfound < nalloc)
			break;

		LIBCFS_FREE(ifr, nalloc * sizeof(*ifr));
		nalloc *= 2;
	}

	if (nfound == 0)
		goto out1;

	LIBCFS_ALLOC(names, nfound * sizeof(*names));
	if (names == NULL) {
		rc = -ENOMEM;
		goto out1;
	}
	/* NULL out all names[i] */
	memset (names, 0, nfound * sizeof(*names));

	for (i = 0; i < nfound; i++) {

		nob = strlen (ifr[i].ifr_name);
		if (nob >= IFNAMSIZ) {
			/* no space for terminating NULL */
			CERROR("interface name %.*s too long (%d max)\n",
			       nob, ifr[i].ifr_name, IFNAMSIZ);
			rc = -ENAMETOOLONG;
			goto out2;
		}

		LIBCFS_ALLOC(names[i], IFNAMSIZ);
		if (names[i] == NULL) {
			rc = -ENOMEM;
			goto out2;
		}

		memcpy(names[i], ifr[i].ifr_name, nob);
		names[i][nob] = 0;
	}

	*namesp = names;
	rc = nfound;

 out2:
	if (rc < 0)
		libcfs_ipif_free_enumeration(names, nfound);
 out1:
	LIBCFS_FREE(ifr, nalloc * sizeof(*ifr));
 out0:
	return rc;
}

/*
 * Network functions used by user-land lnet acceptor
 */

int
libcfs_sock_listen(int *sockp, lnet_nid_t nid, __u32 local_ip,
    int local_port, int backlog)
{
	int                rc, af;
	int                option;
	struct sockaddr_in locaddr;

	af = LNET_GETAF(nid);
	*sockp = socket(af, SOCK_STREAM, 0);
	if (*sockp < 0) {
		rc = -errno;
		CERROR("socket() failed: errno==%d\n", errno);
		return rc;
	}

	option = 1;
	if ( setsockopt(*sockp, SOL_SOCKET, SO_REUSEADDR,
			(char *)&option, sizeof (option)) ) {
		rc = -errno;
		CERROR("setsockopt(SO_REUSEADDR) failed: errno==%d\n", errno);
		goto failed;
	}

	if (local_ip != 0 || local_port != 0) {
		memset(&locaddr, 0, sizeof(locaddr));
		locaddr.sin_family = af;
		locaddr.sin_port = htons(local_port);
		locaddr.sin_addr.s_addr = (local_ip == 0) ?
					  INADDR_ANY : htonl(local_ip);

		if ( bind(*sockp, (struct sockaddr *)&locaddr, sizeof(locaddr)) ) {
			char buf[16]; /* xxx.xxx.xxx.xxx + NUL */

			rc = -errno;
			CERROR("bind(%s:%d af %d): %s\n", inet_ntop(AF_INET,
			    &locaddr.sin_addr, buf, sizeof(buf)), local_port,
			    af, strerror(errno));
			goto failed;
		}
	}

	if ( listen(*sockp, backlog) ) {
		rc = -errno;
		CERROR("listen() with backlog==%d failed: errno==%d\n",
		       backlog, errno);
		goto failed;
	}

	return 0;

  failed:
	close(*sockp);
	return rc;
}

int
libcfs_sock_accept (int *newsockp, int sock, __u32 *peer_ip, int *peer_port)
{
	struct sockaddr_in accaddr;
	socklen_t accaddr_len = sizeof(struct sockaddr_in);
	struct psc_thread *thr;

	thr = pscthr_get();

	thr->pscthr_waitq = "accept";
	*newsockp = accept(sock, (struct sockaddr *)&accaddr, &accaddr_len);
	thr->pscthr_waitq = NULL;

	if ( *newsockp < 0 ) {
		CERROR("accept() failed: errno==%d\n", errno);
		return -errno;
	}

	*peer_ip = ntohl(accaddr.sin_addr.s_addr);
	*peer_port = ntohs(accaddr.sin_port);

	return 0;
}

ssize_t
libcfs_sock_read(struct lnet_xport *lx, void *buffer, size_t nob,
    int timeout)
{
	ssize_t rc;
	struct pollfd pfd;
	cfs_time_t start_time = cfs_time_current();

	pfd.fd = lx->lx_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	/* poll(2) measures timeout in msec */
	timeout *= 1000;

	while (nob != 0 && timeout > 0) {
		rc = poll(&pfd, 1, timeout);
		if (rc < 0)
			return -errno;
		if (rc == 0)
			return -ETIMEDOUT;
		if ((pfd.revents & POLLIN) == 0)
			return -EIO;

		rc = read(lx->lx_fd, buffer, nob);
		if (rc < 0)
			return -errno;
		if (rc == 0)
			return -EIO;

		buffer = ((char *)buffer) + rc;
		nob -= rc;

		timeout -= cfs_duration_sec(cfs_time_sub(cfs_time_current(),
							start_time));
	}

	if (nob == 0)
		return 0;
	else
		return -ETIMEDOUT;
}

/* Just try to connect to localhost to wake up entity that are
 * sleeping in accept() */
void
libcfs_sock_abort_accept(lnet_nid_t nid, __u16 port)
{
	int                fd, rc, af;
	struct sockaddr_in locaddr;

	af = LNET_GETAF(nid);
	memset(&locaddr, 0, sizeof(locaddr));
	locaddr.sin_family = af;
	locaddr.sin_port = htons(port);
	locaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	fd = socket(af, SOCK_STREAM, 0);
	if ( fd < 0 ) {
		CERROR("socket() failed: errno==%d\n", errno);
		return;
	}

	rc = connect(fd, (struct sockaddr *)&locaddr, sizeof(locaddr));
	if ( rc != 0 ) {
		if ( errno != ECONNREFUSED )
			CERROR("connect() failed: errno==%d\n", errno);
		else
			CDEBUG(D_NET, "Nobody to wake up at %d\n", port);
	}

	close(fd);
}

/*
 * Network functions of common use
 */

int
libcfs_getpeername(int sock_fd, __u32 *ipaddr_p, __u16 *port_p)
{
	int                rc;
	struct sockaddr_in peer_addr;
	socklen_t          peer_addr_len = sizeof(peer_addr);

	rc = getpeername(sock_fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
	if (rc != 0)
		return -errno;

	if (ipaddr_p != NULL)
		*ipaddr_p = ntohl(peer_addr.sin_addr.s_addr);
	if (port_p != NULL)
		*port_p = ntohs(peer_addr.sin_port);

	return 0;
}

int
libcfs_socketpair(int *fdp)
{
	int rc, i;

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fdp);
	if (rc != 0) {
		rc = -errno;
		CERROR ("Cannot create socket pair: %s\n", strerror(errno));
		return rc;
	}

	for (i = 0; i < 2; i++) {
		rc = libcfs_fcntl_nonblock(fdp[i]);
		if (rc) {
			close(fdp[0]);
			close(fdp[1]);
			return rc;
		}
	}

	return 0;
}

int
libcfs_fcntl_nonblock(int fd)
{
	int rc, flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		rc = -errno;
		CERROR ("Cannot get socket flags\n");
		return rc;
	}

	rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (rc != 0) {
		rc = -errno;
		CERROR ("Cannot set socket flags\n");
		return rc;
	}

	return 0;
}

int
libcfs_sock_set_keepalive(int fd, int keepalive, int cnt, int idle,
    int intv)
{
	int rc, option = keepalive ? 1 : 0;

	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &option,
	    sizeof(option)) == -1) {
		rc = -errno;
		CERROR("Cannot set KEEPALIVE socket option\n");
		return rc;
	}

#ifdef SOL_TCP
	if (cnt && setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &cnt,
	    sizeof(cnt)) == -1) {
		rc = -errno;
		CERROR ("Cannot set KEEPALIVE socket option\n");
		return rc;
	}

	if (idle && setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &idle,
	    sizeof(idle)) == -1) {
		rc = -errno;
		CERROR ("Cannot set KEEPALIVE socket option\n");
		return rc;
	}

	if (intv && setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &intv,
	    sizeof(intv)) == -1) {
		rc = -errno;
		CERROR ("Cannot set KEEPALIVE socket option\n");
		return rc;
	}
#else
	(void)cnt;
	(void)idle;
	(void)intv;
#endif

	return 0;
}

int
libcfs_sock_set_maxseg(int fd, int maxseg)
{
	int rc;

	rc = setsockopt(fd, IPPROTO_TCP, TCP_MAXSEG, &maxseg,
	    sizeof(maxseg));
	if (rc != 0) {
		rc = -errno;
		CERROR ("Cannot set MAXSEG socket option\n");
		return rc;
	}

	return 0;
} 

int
libcfs_sock_set_nagle(int fd, int nagle)
{
	int rc;
	int option = nagle ? 0 : 1;

	rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
	if (rc != 0) {
		rc = -errno;
		CERROR ("Cannot set NODELAY socket option\n");
		return rc;
	}

	return 0;
}

int
libcfs_sock_set_bufsiz(int fd, int bufsiz)
{
	int rc, option;

	LASSERT (bufsiz != 0);

	option = bufsiz;
	rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &option, sizeof(option));
	if (rc != 0) {
		rc = -errno;
		CERROR ("Cannot set SNDBUF socket option\n");
		return rc;
	}

	option = bufsiz;
	rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &option, sizeof(option));
	if (rc != 0) {
		rc = -errno;
		CERROR ("Cannot set RCVBUF socket option\n");
		return rc;
	}

	return 0;
}

int
libcfs_sock_create(int *fdp, lnet_nid_t nid)
{
	int rc, fd, option;

	fd = socket(LNET_GETAF(nid), SOCK_STREAM, 0);
	if (fd < 0) {
		rc = -errno;
		CERROR ("Cannot create socket\n");
		return rc;
	}

	option = 1;
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			&option, sizeof(option));
	if (rc != 0) {
		rc = -errno;
		CERROR  ("Cannot set SO_REUSEADDR for socket\n");
		close(fd);
		return rc;
	}

	*fdp = fd;
	return 0;
}

int
libcfs_sock_bind_to_port(int fd, lnet_nid_t nid, __u32 ip, __u16 port)
{
	int                rc;
	struct sockaddr_in locaddr;

	memset(&locaddr, 0, sizeof(locaddr));
	locaddr.sin_family = LNET_GETAF(nid);
	locaddr.sin_addr.s_addr = htonl(ip);
	locaddr.sin_port = htons(port);

	rc = bind(fd, (struct sockaddr *)&locaddr, sizeof(locaddr));
	if (rc != 0) {
		rc = -errno;
		CERROR("Cannot bind to %u.%u.%u.%u:%d: %s\n",
		    HIPQUAD(ip), (int)port, strerror(errno));
		return rc;
	}

	return 0;
}

int
libcfs_sock_connect(int fd, __u32 ip, __u16 port)
{
	int                rc;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port        = htons(port);

	rc = connect(fd, (struct sockaddr *)&addr,
		     sizeof(struct sockaddr_in));

	if(rc != 0 && errno != EINPROGRESS) {
		rc = -errno;
		if (rc != -EADDRINUSE && rc != -EADDRNOTAVAIL)
			CERROR ("Cannot connect to %u.%u.%u.%u:%d (err=%d)\n",
				HIPQUAD(ip), port, errno);
		return rc;
	}

	return 0;
}

/* NB: EPIPE and ECONNRESET are considered as non-fatal
 * because:
 * 1) it still makes sense to continue reading &&
 * 2) anyway, poll() will set up POLLHUP|POLLERR flags */
ssize_t
libcfs_sock_writev(struct lnet_xport *lx, const struct iovec *vector,
    int count)
{
	ssize_t rc;

	rc = syscall(SYS_writev, lx->lx_fd, vector, count);

	if (rc == 0) /* write nothing */
		return 0;

	if (rc < 0) {
		if (errno == EAGAIN ||   /* write nothing   */
		    errno == EPIPE ||    /* non-fatal error */
		    errno == ECONNRESET) /* non-fatal error */
			return 0;
		else
			return -errno;
	}

	return rc;
}

ssize_t
libcfs_sock_readv(struct lnet_xport *lx, const struct iovec *vector,
    int count)
{
	ssize_t rc;

	rc = syscall(SYS_readv, lx->lx_fd, vector, count);

	if (rc == 0) /* EOF */
		return -EIO;

	if (rc < 0) {
		if (errno == EAGAIN) /* read nothing */
			return 0;
		else
			return -errno;
	}

	return rc;
}

struct lnet_xport *
lx_new(struct lnet_xport_int *lxi)
{
	struct lnet_xport *lx;

	LIBCFS_ALLOC(lx, sizeof(*lx));
	if (lx)
		lx->lx_tab = lxi;
	return (lx);
}

void
lx_destroy(struct lnet_xport *lx)
{
	LIBCFS_FREE(lx, sizeof(*lx));
}

struct lnet_xport_int libcfs_sock_lxi = {
	NULL,
	NULL,
	NULL,
	NULL,
	libcfs_sock_read,
	libcfs_sock_readv,
	libcfs_sock_writev
};

#endif /* !__KERNEL__ || !defined(REDSTORM) */
