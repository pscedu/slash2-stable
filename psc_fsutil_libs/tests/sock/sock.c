/* $Id: fmtstr.c 2344 2007-12-07 17:48:02Z yanovich $ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/iostats.h"
#include "psc_util/log.h"
#include "psc_util/strlcpy.h"

#include "sdp_inet.h"

/* I/O operation flags */
#define IOF_WR	(1 << 0)			/* write(2) instead of read(2) */

#define Q 15

struct sockarg {
	int s;					/* socket */
};

int			 forcesdp;		/* force sockets direct */
const char		*progname;
char			*buf;
size_t			 bufsiz = 1024 * 1024;
struct iostats	 	rdst;			/* read stats */
struct iostats	 	wrst;			/* write stats */

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s -l if\n\t%s addr\n", progname, progname);
	exit(1);
}

__dead void
ioloop(int s, int ioflags)
{
	struct iostats *ist;
	ssize_t rv;
	fd_set set;

	ist = ioflags & IOF_WR ? &wrst : &rdst;
	for (;;) {
		/* test select(2) */
		FD_ZERO(&set);
		FD_SET(s, &set);
		if (ioflags & IOF_WR)
			rv = select(1, &set, NULL, NULL, NULL);
		else
			rv = select(1, NULL, &set, NULL, NULL);
		if (rv == -1)
			psc_fatal("select");
		if (rv != 1)
			psc_fatalx("select: unexpected value (%zd)", rv);

		/* transfer a chunk of data */
		if (ioflags & IOF_WR)
			rv = send(s, buf, bufsiz, MSG_NOSIGNAL | MSG_WAITALL);
		else
			rv = recv(s, buf, bufsiz, MSG_NOSIGNAL | MSG_WAITALL);
		if (rv == -1)
			psc_fatal("recv");
		else if (rv == -1)
			psc_fatalx("recv: unexpected EOF");
		else if (rv != (int)bufsiz)
			psc_fatalx("recv: short write");

		/* tally amount transferred */
		atomic_add(rv, &ist->ist_bytes_intv);
	}
}

__dead void *
worker_main(void *arg)
{
	struct sockarg *sarg = arg;

	ioloop(sarg->s, 0);
}

__dead void
dolisten(const char *listenif)
{
	struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	struct sockarg sarg;
	struct ifreq ifr;
	int rc, s, clisock;
	char addrbuf[50];
	pthread_t rdthr;
	socklen_t salen;

	memset(&ifr, 0, sizeof(ifr));
	if (strlcpy(ifr.ifr_name, listenif,
	    sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name))
		psc_fatalx("interface name too long: %s", listenif);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFADDR, &ifr) == -1)
		psc_fatal("ioctl GIFADDR %s", listenif);
	close(s);

	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	if (sin->sin_family != AF_INET)
		psc_fatalx("non-ipv4 not supported");

	if (forcesdp)
		sin->sin_family = AF_INET_SDP;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(s, (struct sockaddr *)sin, sizeof(*sin)) == -1)
		psc_fatal("bind");
	if (listen(s, Q) == -1)
		psc_fatal("bind");
	psc_notice("listening on %s", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)));
	clisock = accept(s, (struct sockaddr *)&ss, &salen);
	close(s);

	if (ss.ss_family != AF_INET &&
	    ss.ss_family != AF_INET_SDP)
		psc_fatalx("accept: impossible address family");

	sin = (struct sockaddr_in *)&ss;
	psc_notice("accepted connection from %s", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)));

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = clisock;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(clisock, IOF_WR);
}

__dead void
doconnect(const char *addr)
{
	struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	struct sockarg sarg;
	pthread_t rdthr;
	int rc, s;

	sin = (struct sockaddr_in *)&ss;
	if (inet_pton(AF_INET, addr, &sin->sin_addr.s_addr) != 1)
		psc_fatal("inet_pton");

	if (forcesdp && sin->sin_family == AF_INET)
		sin->sin_family = AF_INET_SDP;

	s = socket(sin->sin_family, SOCK_STREAM, 0);
	if (s == -1)
		psc_fatal("socket");
	if (connect(s, (struct sockaddr *)sin, sizeof(*sin)) == -1)
		psc_fatal("connect");

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = s;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(s, IOF_WR);
}

int
main(int argc, char *argv[])
{
	const char *listenif;
	int c;

	progname = argv[0];
	while ((c = getopt(argc, argv, "l:s")) != -1)
		switch (c) {
		case 'l':
			listenif = optarg;
			break;
		case 's':
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

	if (listenif)
		dolisten(listenif);
	else
		doconnect(argv[0]);
}
