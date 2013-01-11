/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/net.h"

const char *dst = "0.0.0.0";
const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [addr]\n", progname);
	exit(0);
}

int
main(int argc, char *argv[])
{
	union pfl_sockaddr psa;
	struct ifaddrs *ifa;
	char ifn[IFNAMSIZ];

	progname = argv[0];
	    pfl_init();
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	argv += optind;
	if (argc == 1)
		dst = argv[0];
	else if (argc)
		usage();

	memset(&psa, 0, sizeof(psa));
	psa.sin.sin_family = AF_INET;
#ifdef SA_LEN
	psa.sin.sin_len = sizeof(psa.sin);
#endif
	inet_pton(AF_INET, dst, &psa.sin.sin_addr.s_addr);

	pflnet_getifaddrs(&ifa);
	pflnet_getifnfordst(ifa, &psa.sa, ifn);
	pflnet_freeifaddrs(ifa);

	printf("%s\n", ifn);
	exit(0);
}
