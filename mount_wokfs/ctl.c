/* $Id$ */
/* %PSC_COPYRIGHT% */

/*
 * Interface for controlling live operation of a wokfs instance.
 */

#include <sys/param.h>
#include <sys/socket.h>

#include "pfl/cdefs.h"
#include "pfl/ctl.h"
#include "pfl/ctlsvr.h"
#include "pfl/fs.h"
#include "pfl/net.h"
#include "pfl/str.h"

#include "mount_wokfs.h"

int
wokctl_getcreds(int s, struct pscfs_creds *pcrp)
{
	uid_t uid;
	gid_t gid;
	int rc;

	rc = pfl_socket_getpeercred(s, &uid, &gid);
	pcrp->pcr_uid = uid;
	pcrp->pcr_gid = gid;
	pcrp->pcr_ngid = 1;
	return (rc);
}

int
wokctl_getclientctx(__unusedx int s, struct pscfs_clientctx *pfcc)
{
	pfcc->pfcc_pid = -1;
	return (0);
}

struct psc_ctlop ctlops[] = {
	PSC_CTLDEFOPS
};

psc_ctl_thrget_t psc_ctl_thrgets[] = {
};

PFLCTL_SVR_DEFS;

void
ctlthr_main(__unusedx struct psc_thread *thr)
{
	psc_ctlthr_main(ctlsockfn, ctlops, nitems(ctlops),
	    THRT_CTLAC);
}

void
ctlthr_spawn(void)
{
	struct psc_thread *thr;

	psc_ctlparam_register("faults", psc_ctlparam_faults);
	psc_ctlparam_register("log.file", psc_ctlparam_log_file);
	psc_ctlparam_register("log.format", psc_ctlparam_log_format);
	psc_ctlparam_register("log.level", psc_ctlparam_log_level);
	psc_ctlparam_register("log.points", psc_ctlparam_log_points);
	psc_ctlparam_register("opstats", psc_ctlparam_opstats);
	psc_ctlparam_register("pause", psc_ctlparam_pause);
	psc_ctlparam_register("pool", psc_ctlparam_pool);
	psc_ctlparam_register("rlim", psc_ctlparam_rlim);
	psc_ctlparam_register("run", psc_ctlparam_run);
	psc_ctlparam_register("rusage", psc_ctlparam_rusage);

	psc_ctlparam_register_var("sys.mountpoint", PFLCTL_PARAMT_STR,
	    0, mountpoint);
//	psc_ctlparam_register_simple("sys.uptime", ctlparam_uptime_get,
//	    NULL);
//	psc_ctlparam_register_simple("sys.version",
//	    ctlparam_version_get, NULL);

	thr = pscthr_init(THRT_CTL, ctlthr_main, NULL,
	    sizeof(struct psc_ctlthr), "ctlthr0");
	pscthr_setready(thr);
}
