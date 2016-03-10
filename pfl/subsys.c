/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2006-2016, Pittsburgh Supercomputing Center
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
 * Subsystem definitions.  This is used exclusively for granular
 * debug/runtime logging of different components.  Note that most of
 * this code is not thread safe.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>
#include <syslog.h>

#include "pfl/alloc.h"
#include "pfl/dynarray.h"
#include "pfl/fmtstr.h"
#include "pfl/log.h"
#include "pfl/subsys.h"
#include "pfl/thread.h"

struct pfl_subsys {
	const char	*pss_name;
	int		 pss_loglevel;
};

extern int		*pfl_syslog;

struct psc_dynarray	 pfl_subsystems = DYNARRAY_INIT_NOLOG;

int
pfl_subsys_id(const char *name)
{
	const struct pfl_subsys *ss;
	int n;

	DYNARRAY_FOREACH(ss, n, &pfl_subsystems)
		if (strcasecmp(name, ss->pss_name) == 0)
			return (n);
	return (-1);
}

const char *
pfl_subsys_name(int ssid)
{
	const struct pfl_subsys *ss;

	if (ssid < 0 || ssid >= psc_dynarray_len(&pfl_subsystems))
		return ("<unknown>");
	ss = psc_dynarray_getpos(&pfl_subsystems, ssid);
	return (ss->pss_name);
}

void
_psc_threads_rebuild_subsys(int init)
{
	struct psc_thread *thr;
	int nss;

	PLL_LOCK(&psc_threads);
	nss = psc_dynarray_len(&pfl_subsystems);
	PLL_FOREACH(thr, &psc_threads) {
		thr->pscthr_loglevels = psc_realloc(
		    thr->pscthr_loglevels,
		    sizeof(*thr->pscthr_loglevels) * nss, PAF_NOLOG);
		if (init != -1)
			thr->pscthr_loglevels[nss - 1] = init;
	}
	PLL_ULOCK(&psc_threads);
}

void
pfl_subsys_register(int ssid, const char *name)
{
	struct pfl_subsys *ss;
	char *p, buf[BUFSIZ];
	int nss;

	nss = psc_dynarray_len(&pfl_subsystems);
	ss = psc_alloc(sizeof(*ss), PAF_NOLOG);
	ss->pss_name = name;

	snprintf(buf, sizeof(buf), "PSC_LOG_LEVEL_%s", name);
	p = getenv(buf);
	if (p) {
		ss->pss_loglevel = psc_loglevel_fromstr(p);
		if (ss->pss_loglevel == PNLOGLEVELS)
			errx(1, "invalid %s value", name);
	} else {
		ss->pss_loglevel = psc_log_getlevel_global();
		if (ssid == PSS_TMP)
			ss->pss_loglevel = PLL_DEBUG;
	}

	snprintf(buf, sizeof(buf), "PSC_SYSLOG_%s", name);
	if (getenv(buf) || getenv("PSC_SYSLOG")) {
		static int init;

		if (!init) {
			extern const char *__progname;
			const char *ident = __progname;

			init = 1;
			p = getenv("PFL_SYSLOG_IDENT");
			if (p) {
				static char idbuf[32];

				ident = idbuf;
				(void)FMTSTR(idbuf, sizeof(idbuf), p,
				    FMTSTRCASE('n', "s", __progname)
				);
			}
			openlog(ident, LOG_CONS | LOG_NDELAY | LOG_PID,
			    LOG_DAEMON);
		}

		pfl_syslog = psc_realloc(pfl_syslog,
		    sizeof(*pfl_syslog) * (nss + 1), PAF_NOLOG);
		pfl_syslog[nss] = 1;
	}

	if (ssid != nss)
		errx(1, "bad ID %d for subsys %s [want %d], "
		    "check order", ssid, name, nss);
	psc_dynarray_add(&pfl_subsystems, ss);

	_psc_threads_rebuild_subsys(ss->pss_loglevel);
}

void
pfl_subsys_unregister(int ssid)
{
	struct pfl_subsys *ss;

	psc_assert(ssid == psc_dynarray_len(&pfl_subsystems) - 1);
	ss = psc_dynarray_getpos(&pfl_subsystems, ssid);
	psc_dynarray_removepos(&pfl_subsystems, ssid);
	PSCFREE(ss);

	_psc_threads_rebuild_subsys(-1);
}

int
psc_log_getlevel_ss(int ssid)
{
	const struct pfl_subsys *ss;

	if (ssid >= psc_dynarray_len(&pfl_subsystems) || ssid < 0) {
		/* don't use psclog to avoid loops */
		warnx("subsystem out of bounds (%d, max %d)", ssid,
		    psc_dynarray_len(&pfl_subsystems));
		abort();
	}
	ss = psc_dynarray_getpos(&pfl_subsystems, ssid);
	return (ss->pss_loglevel);
}

void
psc_log_setlevel_ss(int ssid, int newlevel)
{
	struct pfl_subsys *ss;
	int i;

	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		psc_fatalx("log level out of bounds (%d, max %d)",
		    newlevel, PNLOGLEVELS);

	if (ssid == PSS_ALL) {
		DYNARRAY_FOREACH(ss, i, &pfl_subsystems)
			ss->pss_loglevel = newlevel;
	} else if (ssid >= psc_dynarray_len(&pfl_subsystems) ||
	    ssid < 0)
		psc_fatalx("subsystem out of bounds (%d, max %d)", ssid,
		    psc_dynarray_len(&pfl_subsystems));
	else {
		ss = psc_dynarray_getpos(&pfl_subsystems, ssid);
		ss->pss_loglevel = newlevel;
	}
}
