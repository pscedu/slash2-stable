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

#include <stdio.h>

//#include <time.h>
//#include <asm/param.h>
//#include "libcfs/kp30.h"

//#include "pfl/alloc.h"
//#include "pfl/atomic.h"
#include "zestRpc.h"
#include "pfl/waitq.h"

extern unsigned long zobd_timeout;

int
//main(int argc, char *argv[])
main(void)
{
	struct l_wait_info lwi;
	struct psc_waitq wq;
	int rc;

	if ( pscrpc_ni_init() ) {
		fprintf(stderr, "pscrpc_ni_init failed\n");
		exit(1);
	}
	psc_waitq_init(&wq);

	//t = time(NULL);

	//clock_gettime(CLOCK_REALTIME, &tv);
	//printf("t=%d\ngettime=%d\n", t, tv.tv_sec);

	lwi = LWI_TIMEOUT(1, NULL, NULL);
	//rc = psc_svr_wait_event(&wq, 0, &lwi, NULL);
	rc = psc_cli_wait_event(&wq, 0, &lwi);

	printf("rc=%d [ETIMEDOUT=%d, EINVAL=%d, EPERM=%d]\n%s\n",
	       rc, ETIMEDOUT, EINVAL, EPERM, strerror(-rc));

	return 0;
}

#if 0
1179962711 - LWI_TIMEOUT(10000..)
1179952712 - LWI_TIMEOUT(1..)

#endif
