/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
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

#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/cdefs.h"
#include "pfl/fmt.h"
#include "pfl/opstats.h"
#include "pfl/log.h"
#include "pfl/net.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "pfl/thread.h"
#include "pfl/time.h"
#include "pfl/timerthr.h"

#include "sdp_inet.h"

/* I/O operation flags */
#define IOF_RD		0			/* read(2) I/O */
#define IOF_WR		(1 << 0)		/* write(2) instead of read(2) */
#define Q		15			/* listen(2) queue length */
#define PORT		15420			/* IPv4 port */

#define THRT_OPSTIMER	0			/* iostats timer thread type */
#define THRT_DISPLAY	1			/* stats displayer */
#define THRT_RD		2
#define THRT_WR		3

int			 forcesdp;		/* force sockets direct */
int			 sel = 0;
int			 peersock;
const char		*progname;
char			*buf;
size_t			 bufsiz = 1024 * 1024;
struct pfl_opstat	*rdst;			/* read stats */
struct pfl_opstat	*wrst;			/* write stats */
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
	struct psc_waitq dummy = PSC_WAITQ_INIT("display");
	struct timespec tv;
	int n = 0;

	PFL_GETTIMESPEC(&tv);
	tv.tv_nsec = 500000000;
	for (;; n++) {
		tv.tv_sec++;
		psc_waitq_waitabs(&dummy, NULL, &tv);

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

		pfl_fmt_human(ratebuf, rdst->opst_intv);
		printf("%7s\t", ratebuf);
		pfl_fmt_human(ratebuf, psc_atomic64_read(&rdst->opst_lifetime));
		printf("%7s\t\t|\t", ratebuf);

		pfl_fmt_human(ratebuf, wrst->opst_intv);
		printf("%7s\t", ratebuf);
		pfl_fmt_human(ratebuf, psc_atomic64_read(&wrst->opst_lifetime));
		printf("%7s\n", ratebuf);

		if (n > 30)
			n = 0;
	}
}

void
ioloop(int ioflags)
{
	struct pfl_opstat *ist;
	ssize_t rv;
	fd_set set;
	int wr;

	wr = ioflags & IOF_WR;

	ist = wr ? wrst : rdst;
	for (;;) {
		if (sel) {
			/* Specifically test that select(2) works. */
			FD_ZERO(&set);
			FD_SET(peersock, &set);
			if (wr)
				rv = select(peersock + 1, NULL, &set,
				    NULL, NULL);
			else
				rv = select(peersock + 1, &set, NULL,
				    NULL, NULL);
			if (rv == -1)
				psc_fatal("select");
		}

		if (wr)
			rv = write(peersock, buf, bufsiz);
		else
			rv = read(peersock, buf, bufsiz);
		if (rv == -1)
			psc_fatal("%s", wr ? "send" : "recv");
		else if (rv == 0)
			psc_fatalx("%s: reached EOF", wr ? "send" : "recv");

		/* tally amount transferred */
		pfl_opstat_add(ist, rv);

		pscthr_yield();
	}
}

void
rd_main(__unusedx struct psc_thread *thr)
{
	ioloop(IOF_RD);
}

void
wr_main(__unusedx struct psc_thread *thr)
{
	ioloop(IOF_WR);
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

int
dolisten(const char *listenif)
{
	int opt, s, clisock;
	union pfl_sockaddr psa;
	struct sockaddr_in *sin;
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
	return (clisock);
}

int
doconnect(const char *addr)
{
	struct sockaddr_in *sin;
	union pfl_sockaddr psa;
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
	return (s);
}

void
setport(const char *sp)
{
	char *p;
	long l;

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
	    "\t%s [-S] [-b bufsiz] [-p port] [-R nthr] [-W nthr] -l if\n"
	    "\t%s [-S] [-b bufsiz] [-p port] [-R nthr] [-W nthr] addr\n",
	    progname, progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, i, nrthr = 0, nwthr = 0;
	const char *listenif;
	char *endp, *p;
	long l;

	pfl_init();
	listenif = NULL;
	progname = argv[0];
	while ((c = getopt(argc, argv, "b:l:p:STR:W:")) != -1)
		switch (c) {
		case 'b':
			l = strtol(optarg, &endp, 10);
			if (l < 1024 || l > 1024*1024 ||
			    endp == optarg || *endp)
				errx(1, "invalid number");
			bufsiz = l;
			break;
		case 'l':
			listenif = optarg;
			break;
		case 'p':
			setport(optarg);
			break;
		case 'S':
			forcesdp = 1;
			break;
		case 'T':
			sel = 1;
			break;
		case 'R':
		case 'W':
			l = strtol(optarg, &endp, 10);
			if (l < 0 || l > 32 ||
			    endp == optarg || *endp)
				errx(1, "invalid number");
			if (c == 'R')
				nrthr = l;
			else
				nwthr = l;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!nrthr && !nwthr)
		nrthr = nwthr = 1;

	if (listenif) {
		if (argc)
			usage();
	} else {
		if (argc != 1)
			usage();
	}

	buf = PSCALLOC(bufsiz);

	rdst = pfl_opstat_init("read");
	wrst = pfl_opstat_init("write");

	pfl_opstimerthr_spawn(THRT_OPSTIMER, "opstimerthr");

	if (listenif)
		peersock = dolisten(listenif);
	else {
		if ((p = strchr(argv[0], ':')) != NULL) {
			*p++ = '\0';
			setport(p);
		}
		peersock = doconnect(argv[0]);
	}

	pscthr_init(THRT_DISPLAY, displaythr_main, 0, "displaythr");

	for (i = 0; i < nrthr; i++)
		pscthr_init(THRT_RD, rd_main, 0, "rdthr%d", i);
	for (i = 0; i < nwthr; i++)
		pscthr_init(THRT_WR, wr_main, 0, "wrthr%d", i);

	for (;;)
		sleep(1);

	exit(0);
}
