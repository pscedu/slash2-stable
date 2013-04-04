/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "pfl/time.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/fmt.h"
#include "psc_util/iostats.h"
#include "psc_util/log.h"
#include "psc_util/net.h"
#include "psc_util/thread.h"
#include "psc_util/timerthr.h"

#include "sdp_inet.h"

/* I/O operation flags */
#define IOF_RD		0			/* read(2) I/O */
#define IOF_WR		(1 << 0)		/* write(2) instead of read(2) */
#define Q		15			/* listen(2) queue length */
#define PORT		15420			/* IPv4 port */

#define THRT_TIOS	0			/* iostats timer thread type */
#define THRT_DISPLAY	1			/* stats displayer */

struct sockarg {
	int s;					/* socket */
};

int			 forcesdp;		/* force sockets direct */
const char		*progname;
char			*buf;
size_t			 bufsiz = 1024 * 1024;
struct psc_iostats	 rdst;			/* read stats */
struct psc_iostats	 wrst;			/* write stats */
pthread_t		 rdthr;
in_port_t		 port = PORT;

void
center(const char *s, int width)
{
	int len;

	len = (width - strlen(s)) / 2;
	printf("%*s%s%*s", len, "", s, len, "");
}

void
displaythr_main(__unusedx struct psc_thread *thr)
{
	char ratebuf[PSCFMT_HUMAN_BUFSIZ];
	struct psc_iostats myist;
	struct timeval tv;
	int n = 0;

	for (;; n++) {
		PFL_GETTIMEVAL(&tv);
		usleep(1000000 - tv.tv_usec);
		PFL_GETTIMEVAL(&tv);

		if (n == 0) {
			center("-- read --", 8 * 3);
			printf("\t|\t");
			center("-- write --", 8 * 3);
			printf("\n"
			    "%7s\t%7s\t%7s\t\t|\t"
			    "%7s\t%7s\t%7s\n",
			    "time", "intvamt", "total",
			    "time", "intvamt", "total");
			printf("================================="
			    "==============================\n");
		}

		memcpy(&myist, &rdst, sizeof(myist));
		psc_fmt_human(ratebuf,
		    psc_iostats_getintvrate(&myist, 0));
		printf("%6.2fs\t%7s\t",
		    psc_iostats_getintvdur(&myist, 0), ratebuf);
		psc_fmt_human(ratebuf, myist.ist_len_total);
		printf("%7s\t\t|\t", ratebuf);

		memcpy(&myist, &wrst, sizeof(myist));
		psc_fmt_human(ratebuf,
		    psc_iostats_getintvrate(&myist, 0));
		printf("%6.2fs\t%7s\t",
		    psc_iostats_getintvdur(&myist, 0), ratebuf);
		psc_fmt_human(ratebuf, myist.ist_len_total);
		printf("%7s\n", ratebuf);

		if (n > 30)
			n = 0;
	}
}

__dead void
ioloop(int s, int ioflags)
{
	struct psc_iostats *ist;
	int wr, rem;
	ssize_t rv;
	fd_set set;

	wr = ioflags & IOF_WR;

	if (wr)
		pscthr_init(THRT_DISPLAY, 0, displaythr_main,
		    NULL, 0, "displaythr");

	ist = wr ? &wrst : &rdst;
	for (;;) {
		/* Specifically test that select(2) works. */
		FD_ZERO(&set);
		FD_SET(s, &set);
		if (wr)
			rv = select(s + 1, NULL, &set, NULL, NULL);
		else
			rv = select(s + 1, &set, NULL, NULL, NULL);

		if (rv == -1)
			psc_fatal("select");
		if (rv != 1)
			psc_fatalx("select: unexpected value (%zd)", rv);

		/* transfer a chunk of data */
		rem = bufsiz;
		do {
			if (wr)
				rv = send(s, buf, rem, PFL_MSG_NOSIGNAL | MSG_WAITALL);
			else
				rv = recv(s, buf, rem, PFL_MSG_NOSIGNAL | MSG_WAITALL);
			if (rv == -1)
				psc_fatal("%s", wr ? "send" : "recv");
			else if (rv == 0)
				psc_fatalx("%s: reached EOF", wr ? "send" : "recv");

			/* tally amount transferred */
			psc_iostats_intv_add(ist, rv);
			rem -= rv;
		} while (rem);
	}
}

__dead void *
worker_main(void *arg)
{
	struct sockarg *sarg = arg;

	ioloop(sarg->s, IOF_RD);
}

void
sock_setoptions(int s)
{
	socklen_t sz;
	int opt;

	sz = sizeof(opt);

	opt = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &opt, sz) == -1)
		psc_fatal("setsockopt");
	opt = bufsiz;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &opt, sz) == -1)
		psclog_error("setsockopt");
	opt = bufsiz;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &opt, sz) == -1)
		psclog_error("setsockopt");

	pfl_socket_setnosig(s);
}

void *
get_ifr_addr(struct ifreq *ifr)
{
	return (&ifr->ifr_addr);
}

__dead void
dolisten(const char *listenif)
{
	int opt, rc, s, clisock;
	union pfl_sockaddr psa;
	struct sockaddr_in *sin;
	struct sockarg sarg;
	struct ifreq ifr;
	char addrbuf[50];
	socklen_t salen;

	memset(&ifr, 0, sizeof(ifr));
	if (strlcpy(ifr.ifr_name, listenif,
	    sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name))
		psc_fatalx("interface name too long: %s", listenif);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFADDR, &ifr) == -1)
		psc_fatal("ioctl GIFADDR %s", listenif);
	close(s);

	sin = get_ifr_addr(&ifr);
	if (sin->sin_family != AF_INET)
		psc_fatalx("non-ipv4 not supported");

	if (forcesdp)
		sin->sin_family = AF_INET_SDP;
	sin->sin_port = htons(port);

	s = socket(sin->sin_family, SOCK_STREAM, 0);
	opt = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    &opt, sizeof(opt)) == -1)
		psc_fatal("setsockopt");
	if (bind(s, (struct sockaddr *)sin, sizeof(*sin)) == -1)
		psc_fatal("bind");
	if (listen(s, Q) == -1)
		psc_fatal("bind");
	printf("listening on %s:%d\n", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)),
	    ntohs(sin->sin_port));
	salen = sizeof(psa);
	clisock = accept(s, &psa.sa, &salen);
	close(s);

	sin = (struct sockaddr_in *)&psa.sin;
	if (sin->sin_family != AF_INET &&
	    sin->sin_family != AF_INET_SDP)
		psc_fatalx("accept: impossible address family %d",
		    sin->sin_family);

	printf("accepted connection from %s:%d\n", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)),
	    ntohs(sin->sin_port));

	sock_setoptions(clisock);

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = clisock;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(clisock, IOF_WR);
}

__dead void
doconnect(const char *addr)
{
	struct sockaddr_in *sin;
	union pfl_sockaddr psa;
	struct sockarg sarg;
	char addrbuf[50];
	int rc, s;

	memset(&psa, 0, sizeof(psa));
	sin = (struct sockaddr_in *)&psa.sin;
	rc = inet_pton(AF_INET, addr, &sin->sin_addr.s_addr);
	if (rc == -1)
		psc_fatal("inet_pton: %s", addr);
	if (rc == 0)
		psc_fatalx("address cannot be parsed: %s", addr);
	sin->sin_port = htons(port);
	sin->sin_family = AF_INET;

	s = socket(forcesdp ? AF_INET_SDP : AF_INET, SOCK_STREAM, 0);
	if (s == -1)
		psc_fatal("socket");
	if (connect(s, &psa.sa, sizeof(*sin)) == -1)
		psc_fatal("connect");
	printf("connected to %s:%d\n", inet_ntop(sin->sin_family,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)),
	    ntohs(sin->sin_port));

	sock_setoptions(s);

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = s;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(s, IOF_WR);
}

void
setport(const char *sp)
{
	long l;
	char *p;

	l = strtol(sp, &p, 10);
	if (l < 0 || l >= USHRT_MAX ||
	    p == sp || *p != '\0')
		errx(1, "invalid port: %s", sp);
	port = (in_port_t)l;
}

__dead void
usage(void)
{
	fprintf(stderr, "usage:"
	    "\t%s [-S] [-p port] -l if\n"
	    "\t%s [-S] [-p port] addr\n",
	    progname, progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *listenif;
	char *p;
	int c;

	pfl_init();
	listenif = NULL;
	progname = argv[0];
	while ((c = getopt(argc, argv, "l:p:S")) != -1)
		switch (c) {
		case 'l':
			listenif = optarg;
			break;
		case 'p':
			setport(optarg);
			break;
		case 'S':
			forcesdp = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (listenif) {
		if (argc)
			usage();
	} else {
		if (argc != 1)
			usage();
	}

	buf = PSCALLOC(bufsiz);

	psc_iostats_init(&rdst, "read");
	psc_iostats_init(&wrst, "write");

	psc_tiosthr_spawn(THRT_TIOS, "tiosthr");

	if (listenif)
		dolisten(listenif);
	else {
		if ((p = strchr(argv[0], ':')) != NULL) {
			*p++ = '\0';
			setport(p);
		}
		doconnect(argv[0]);
	}
	exit(0);
}
