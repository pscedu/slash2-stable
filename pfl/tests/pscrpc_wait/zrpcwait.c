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

#include <stdio.h>

//#include <time.h>
//#include <asm/param.h>
//#include "libcfs/kp30.h"

//#include "psc_util/alloc.h"
//#include "psc_util/atomic.h"
#include "zestRpc.h"
#include "psc_util/waitq.h"

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
