/* $Id$ */

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
	psc_waitq_t wq;
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
