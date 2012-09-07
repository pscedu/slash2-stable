/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011-2012, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/cdefs.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlsvr.h"

#include "lnrtd.h"

struct psc_ctlop lrctlops[] = {
	PSC_CTLDEFOPS
};

psc_ctl_thrget_t psc_ctl_thrgets[] = {
/* CTL		*/ psc_ctlthr_get,
/* CTLAC	*/ psc_ctlacthr_get,
/* EQPOLL	*/ NULL,
/* LNETAC	*/ NULL,
/* TIOS		*/ NULL,
/* USKLNDPL	*/ NULL
};

struct pfl_opstat pflctl_opstats[] = { };

PFLCTL_SVR_DEFS;

int
lrctlthr_main(void)
{
//	psc_ctlparam_register("faults", psc_ctlparam_faults);
	psc_ctlparam_register("log.file", psc_ctlparam_log_file);
	psc_ctlparam_register("log.format", psc_ctlparam_log_format);
	psc_ctlparam_register("log.level", psc_ctlparam_log_level);
	psc_ctlparam_register("pause", psc_ctlparam_pause);
	psc_ctlparam_register("pool", psc_ctlparam_pool);
	psc_ctlparam_register("rlim", psc_ctlparam_rlim);
	psc_ctlparam_register("run", psc_ctlparam_run);

	psc_ctlthr_main(ctlsockfn, lrctlops, nitems(lrctlops),
	    LRTHRT_CTLAC);
}
