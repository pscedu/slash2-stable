/* $Id$ */
/* %PSC_COPYRIGHT% */

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

PFLCTL_SVR_DEFS;

void
lrctlthr_begin(__unusedx struct psc_thread *thr)
{
	psc_ctlthr_main(ctlsockfn, lrctlops, nitems(lrctlops),
	    LRTHRT_CTLAC);
}

void
lrctlthr_spawn(void)
{
	struct psc_thread *thr;

//	psc_ctlparam_register("faults", psc_ctlparam_faults);
	psc_ctlparam_register("log.file", psc_ctlparam_log_file);
	psc_ctlparam_register("log.format", psc_ctlparam_log_format);
	psc_ctlparam_register("log.level", psc_ctlparam_log_level);
	psc_ctlparam_register("pause", psc_ctlparam_pause);
	psc_ctlparam_register("pool", psc_ctlparam_pool);
	psc_ctlparam_register("rlim.nofile", psc_ctlparam_rlim_nofile);
	psc_ctlparam_register("run", psc_ctlparam_run);

	thr = pscthr_init(LRTHRT_CTL, 0, lrctlthr_begin, NULL,
	    sizeof(struct psc_ctlthr), "lrctlthr");
	pscthr_setready(thr);
}
