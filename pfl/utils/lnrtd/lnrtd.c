/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2011-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pfl/str.h"
#include "pfl/list.h"
#include "pfl/rpc.h"
#include "pfl/ctlsvr.h"
#include "pfl/lock.h"
#include "pfl/log.h"

#include "lnet/lnet.h"

#include "lnrtd.h"

const char		*ctlsockfn = PATH_CTLSOCK;
const char		*progname;

struct psc_lockedlist	psc_odtables;
struct psc_lockedlist	pfl_mlists;
struct psc_lockedlist	pfl_meters;
struct psc_lockedlist	psc_pools;

PSCLIST_HEAD(pscrpc_all_services);
psc_spinlock_t pscrpc_all_services_lock;

int
psc_usklndthr_get_type(const char *namefmt)
{
	if (strstr(namefmt, "lracthr"))
		return (LRTHRT_LNETAC);
	return (LRTHRT_USKLNDPL);
}

void
psc_usklndthr_get_namev(char buf[PSC_THRNAME_MAX], const char *namefmt,
    va_list ap)
{
	size_t n;

	n = strlcpy(buf, "lr", PSC_THRNAME_MAX);
	if (n < PSC_THRNAME_MAX)
		vsnprintf(buf + n, PSC_THRNAME_MAX - n, namefmt, ap);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int rc;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	pscthr_init(LRTHRT_CTL, NULL,
	    sizeof(struct psc_ctlthr), "lrctlthr");

	rc = LNetInit(2048);
	if (rc)
		psc_fatalx("failed to initialize LNET rc=%d", rc);
	lnet_server_mode();
	rc = LNetNIInit(PSCRPC_SVR_PID);
	if (rc)
		psc_fatalx("LNetNIInit failed rc=%d", rc);
	lrctlthr_main();
	exit(0);
}
