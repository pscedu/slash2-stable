/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright 2015, Pittsburgh Supercomputing Center
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

struct wok_module {
	const char	*wm_path;
	struct pscfs	 wm_module;
};

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

int
wokctlcmd_insert(int fd, struct psc_ctlmsghdr *mh, void *msg)
{
	void *h, (*loadf)(struct pscfs *);
	struct wokctlmsg_modspec *wcms = msg;
	struct wok_module *wm;

	h = dlopen(wcms->wcms_fn, RTLD_NOW);
	if (h == NULL)
		PFL_GOTOERR(error, 0);

	loadf = dlsym(h, "pscfs_module_load");
	if (h == NULL)
		PFL_GOTOERR(error, 0);

	wm = PSCALLOC(sizeof(*wm));
	loadf(&wm->wm_module);
	wm->wm_module->pf_private = wm;
	pflfs_module_add(wcms->wcms_pos, &wm->wm_module);

	return (0);

 error:
	if (h)
		dlclose(h);
	return (psc_ctlsenderr(fd, mh, "insert: %s",
	    slstrerror(ENOENT)));
}

int
wokctlcmd_list(int fd, struct psc_ctlmsghdr *mh, void *msg)
{
	struct wokctlmsg_modspec *wcms = msg;
	struct pscfs *m;
	int i;

	pflfs_modules_rdpin();
	DYNARRAY_FOREACH(m, i, &pscfs_modules) {
		strlcpy(wcms->wcms_path, m->pf_path,
		    sizeof(wcms->wcms_path));
		wcms->wcms_pos = i;
		rc = psc_ctlmsg_sendv(fd, mh, wcms);
		if (!rc)
			break;
	}
	pflfs_modules_rdunpin();
	return (0);
}

int
wokctlcmd_remove(__unusedx int fd, __unusedx struct psc_ctlmsghdr *mh,
    void *msg)
{
	struct wokctlmsg_modctl *wcmc = msg;

	pflfs_module_remove(pos);
}

struct psc_ctlop ctlops[] = {
	PSC_CTLDEFOPS,
	{ wokctlcmd_insert,		sizeof(wokctlmsg_modspec) },
	{ wokctlcmd_list,		sizeof(wokctlmsg_modspec) },
	{ wokctlcmd_reload,		sizeof(wokctlmsg_modctl) },
	{ wokctlcmd_remove,		sizeof(wokctlmsg_modctl) },
};

psc_ctl_thrget_t psc_ctl_thrgets[] = {
};

PFLCTL_SVR_DEFS;

void
ctlacthr_main(__unusedx struct psc_thread *thr)
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

	thr = pscthr_init(THRT_CTL, ctlacthr_main, NULL,
	    sizeof(struct psc_ctlthr), "ctlacthr");
	pscthr_setready(thr);
}
