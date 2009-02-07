/* $Id: fmtstr.c 2344 2007-12-07 17:48:02Z yanovich $ */

#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "psc_util/cdefs.h"

/* I/O operation flags */
#define IOF_WR	(1 << 0)		/* write(2) instead of read(2) */

struct sockarg {
	int s;				/* socket */
};

int		 forcesdp;		/* force sockets direct */
const char	*progname;
char		*buf;
size_t		 bufsiz = 1024 * 1024;
struct iostats	 rdst;			/* read stats */
struct iostats	 wrst;			/* write stats */

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
			psc_fatalx("select: unexpected value (%d)", rv);

		/* transfer a chunk of data */
		if (ioflags & IOF_WR)
			rv = send(s, buf, bufsiz, MSG_NOSIGNAL | MSG_WAITALL);
		else
			rv = recv(s, buf, bufsiz, MSG_NOSIGNAL | MSG_WAITALL);
		if (rv == -1)
			psc_fatal("recv");
		else if (rv == -1)
			psc_fatalx("recv: unexpected EOF");
		else if (rv != bufsiz)
			psc_fatalx("recv: short write");

		/* tally amount transferred */
		atomic_inc(rv, &ist->ist_bytes_intv);
	}
}

__dead void *
worker_main(void *arg)
{
	struct sockarg *sarg = arg;

	ioloop(sarg.s, 0);
}

__dead void
dolisten(const char *listenif)
{
	struct sockaddr_storage ss;
	struct sockaddr_in *sin;
	struct sockarg sarg;
	struct ifreq ifr;
	char addrbuf[50];
	pthread_t rdthr;
	socklen_t salen;
	int s;

	memset(&ifr, 0, sizeof(ifr));
	if (strlcpy(ifr.ifr_name, listenif,
	    sizeof(ifr.ifr_name)) >= sizeof(ifr.ifr_name))
		psc_fatalx("interface name too long: %s", listenif);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFADDR, &ifr) == -1)
		psc_fatal("ioctl GIFADDR %s", listenif);
	close(s);

	sin = &ifr.ifr_addr;
	if (sin->sin_family != AF_INET)
		psc_fatalx("non-ipv4 not supported");

	if (forcesdp)
		sin->sin_family = AF_INET_SDP;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(s, sin, sizeof(*sin)) == -1)
		psc_fatal("bind");
	if (listen(s, Q) == -1)
		psc_fatal("bind");
	psc_notice("listening on %s", inet_ntop(AF_INET,
	    &sin->sin_addr.s_addr, addrbuf, sizeof(addrbuf)));
	clisock = accept(s, (struct sockaddr *)&ss, sizeof(ss));
	close(s);

	sin = &ss;
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
	pthread_t rdthr;

	AF_INET_SDP;

	memset(&sarg, 0, sizeof(sarg));
	sarg.s = clisock;
	if ((rc = pthread_create(&rdthr, NULL, worker_main, &sarg)) != 0)
		psc_fatalx("pthread_create: %s", strerror(rc));
	ioloop(clisock, IOF_WR);
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
