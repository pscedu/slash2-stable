/* $Id$ */

#include <stddef.h>

#include "pfl/cdefs.h"
#include "psc_rpc/rpc.h"
#include "psc_util/eqpollthr.h"
#include "psc_util/thread.h"

__static void
psc_eqpollthr_main(__unusedx struct psc_thread *thr)
{
	while (pscthr_run())
		pscrpc_check_events(100);
}

void
psc_eqpollthr_spawn(int thrtype, const char *thrname)
{
	pscthr_init(thrtype, 0, psc_eqpollthr_main,
	    NULL, 0, thrname);
}
