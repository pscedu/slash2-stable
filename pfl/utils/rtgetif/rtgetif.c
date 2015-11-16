/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/net.h"

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
