/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <stddef.h>

#include "pfl/cdefs.h"
#include "pfl/rpc.h"
#include "pfl/eqpollthr.h"
#include "pfl/thread.h"

__static void
psc_eqpollthr_main(struct psc_thread *thr)
{
	while (pscthr_run(thr))
		pscrpc_check_events(100);
}

void
psc_eqpollthr_spawn(int thrtype, const char *thrname)
{
	pscthr_init(thrtype, psc_eqpollthr_main, NULL, 0, thrname);
}
