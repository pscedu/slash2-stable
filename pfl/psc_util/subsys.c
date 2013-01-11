/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Subsystem definitions.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>

#include "psc_ds/dynarray.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "pfl/subsys.h"

struct psc_subsys {
	const char	*pss_name;
	int		 pss_loglevel;
};

extern int		*pfl_syslog;

struct psc_dynarray	 psc_subsystems = DYNARRAY_INIT_NOLOG;
int			 psc_nsubsys;

int
psc_subsys_id(const char *name)
{
	const struct psc_subsys **ss;
	int n, len;

	ss = psc_dynarray_get(&psc_subsystems);
	len = psc_dynarray_len(&psc_subsystems);
	for (n = 0; n < len; n++)
		if (strcasecmp(name, ss[n]->pss_name) == 0)
			return (n);
	return (-1);
}

const char *
psc_subsys_name(int ssid)
{
	const struct psc_subsys *ss;

	if (ssid < 0 || ssid >= psc_nsubsys)
		return ("<unknown>");
	ss = psc_dynarray_getpos(&psc_subsystems, ssid);
	return (ss->pss_name);
}

void
psc_subsys_register(int ssid, const char *name)
{
	struct psc_subsys *ss;
	char *p, buf[BUFSIZ];

	ss = psc_alloc(sizeof(*ss), PAF_NOLOG);
	ss->pss_name = name;

	snprintf(buf, sizeof(buf), "PSC_LOG_LEVEL_%s", name);
	p = getenv(buf);
	if (p) {
		ss->pss_loglevel = psc_loglevel_fromstr(p);
		if (ss->pss_loglevel == PNLOGLEVELS)
			psc_fatalx("invalid %s value", name);
	} else {
		ss->pss_loglevel = psc_log_getlevel_global();
		if (ssid == PSS_TMP)
			ss->pss_loglevel = PLL_DEBUG;
	}

	snprintf(buf, sizeof(buf), "PSC_SYSLOG_%s", name);
	if (getenv(buf) || getenv("PSC_SYSLOG")) {
		pfl_syslog = psc_realloc(pfl_syslog,
		    sizeof(*pfl_syslog) * (psc_nsubsys + 1), PAF_NOLOG);
		pfl_syslog[psc_nsubsys] = 1;
	}

	psc_dynarray_add(&psc_subsystems, ss);
	if (psc_nsubsys++ != ssid)
		psc_fatalx("bad ID %d for subsys %s [want %d], "
		    "check order", ssid, name, psc_nsubsys);
}

enum psclog_level
psc_log_getlevel_ss(int ssid)
{
	const struct psc_subsys *ss;

	if (ssid >= psc_nsubsys || ssid < 0) {
		/* don't use psclog to avoid loops */
		warnx("subsystem out of bounds (%d, max %d)", ssid,
		    psc_nsubsys);
		abort();
	}
	ss = psc_dynarray_getpos(&psc_subsystems, ssid);
	return (ss->pss_loglevel);
}

void
psc_log_setlevel_ss(int ssid, enum psclog_level newlevel)
{
	struct psc_subsys **ss;
	int i;

	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		psc_fatalx("log level out of bounds (%d, max %d)",
		    newlevel, PNLOGLEVELS);

	ss = psc_dynarray_get(&psc_subsystems);

	if (ssid == PSS_ALL)
		for (i = 0; i < psc_nsubsys; i++)
			ss[i]->pss_loglevel = newlevel;
	else if (ssid >= psc_nsubsys || ssid < 0)
		psc_fatalx("subsystem out of bounds (%d, max %d)", ssid,
		    psc_nsubsys);
	else
		ss[ssid]->pss_loglevel = newlevel;
}
