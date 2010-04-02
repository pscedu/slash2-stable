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

/*
 * Logging routines.
 * Notes:
 *	(o) We cannot use psc_fatal() for fatal errors here.  Instead use err(3).
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "psc_util/alloc.h"
#include "psc_util/fmtstr.h"
#include "psc_util/log.h"
#include "psc_util/thread.h"

#ifndef APP_STRERROR
#define APP_STRERROR strerror
#else
char *APP_STRERROR(int);
#endif

#ifndef PSC_LOG_FMT
#define PSC_LOG_FMT "[%s:%06u %n:%i:%T:%F:%l] "
#endif

struct fuse_context {
	uid_t uid;
	gid_t gid;
	pid_t pid;
};

const char			*psc_logfmt = PSC_LOG_FMT;
__static int			 psc_loglevel = PLL_TRACE;
__static struct psclog_data	*psc_logdata;
char				 psclog_eol[8] = "\n";	/* overrideable with ncurses EOL */

void
psc_log_init(void)
{
	char *ep, *p;
	long l;

	if ((p = getenv("PSC_LOG_FILE")) != NULL)
		freopen(p, "w", stderr);
	if ((p = getenv("PSC_LOG_FORMAT")) != NULL)
		psc_logfmt = p;
	if ((p = getenv("PSC_LOG_LEVEL")) != NULL) {
		ep = NULL;
		errno = 0;
		l = strtol(p, &ep, 10);
		if (p[0] == '\0' || ep == NULL || *ep != '\0')
			errx(1, "invalid log level env: %s", p);
		if (errno == ERANGE || l < 0 || l >= PNLOGLEVELS)
			errx(1, "invalid log level env: %s", p);
		psc_loglevel = (int)l;
	}
}

__weak struct psclog_data *
psclog_getdatamem(void)
{
	return (psc_logdata);
}

__weak void
psclog_setdatamem(struct psclog_data *d)
{
	psc_logdata = d;
}

int
psc_log_getlevel_global(void)
{
	return (psc_loglevel);
}

__weak int
psc_log_getlevel_ss(__unusedx int subsys)
{
	return (psc_log_getlevel_global());
}

__weak int
psc_log_getlevel(int subsys)
{
	return (psc_log_getlevel_ss(subsys));
}

void
psc_log_setlevel_global(int newlevel)
{
	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		errx(1, "log level out of bounds (%d)", newlevel);
	psc_loglevel = newlevel;
}

__weak void
psc_log_setlevel_ss(__unusedx int subsys, int newlevel)
{
	psc_log_setlevel_global(newlevel);
}

__weak void
psc_log_setlevel(int subsys, int newlevel)
{
	psc_log_setlevel_ss(subsys, newlevel);
}

__weak struct psc_thread *
pscthr_get_canfail(void)
{
	return (NULL);
}

__weak pid_t
pfl_getsysthrid(void)
{
	return (getpid());
}

/**
 * MPI_Comm_rank - dummy overrideable MPI rank retriever.
 */
__weak int
MPI_Comm_rank(__unusedx int comm, int *rank)
{
	*rank = -1;
	return (0);
}

/**
 * fuse_get_context - dummy overrideable fuse context retriever.
 */
__weak struct fuse_context *
psclog_get_fuse_context(void)
{
	return (NULL);
}

/**
 * psc_subsys_name - dummy overrideable PFL subsystem ID -> name resolver.
 */
__weak const char *
psc_subsys_name(__unusedx int ssid)
{
	return ("<unknown>");
}

struct psclog_data *
psclog_getdata(void)
{
	struct psclog_data *d;
	char *p;

	d = psclog_getdatamem();
	if (d == NULL) {
		d = psc_alloc(sizeof(*d), PAF_NOLOG);
		if (gethostname(d->pld_hostname,
		    sizeof(d->pld_hostname)) == -1)
			err(1, "gethostname");
		strlcpy(d->pld_hostshort, d->pld_hostname,
		    sizeof(d->pld_hostshort));
		if ((p = strchr(d->pld_hostshort, '.')) != NULL)
			*p = '\0';
#ifdef HAVE_CNOS
		int cnos_get_rank(void);

		d->pld_rank = cnos_get_rank();
#else
		MPI_Comm_rank(1, &d->pld_rank); /* 1=MPI_COMM_WORLD */
#endif
		psclog_setdatamem(d);
	}
	return (d);
}

void
_psclogv(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, va_list ap)
{
	char *p, prefix[LINE_MAX], fmtbuf[LINE_MAX];
	struct fuse_context *ctx;
	struct psc_thread *thr;
	struct psclog_data *d;
	struct timeval tv;
	const char *thrname;
	int rc, save_errno;
	pid_t thrid;

	save_errno = errno;

	d = psclog_getdata();
	thr = pscthr_get_canfail();
	if (thr) {
		thrid = thr->pscthr_thrid;
		thrname = thr->pscthr_name;
	} else {
		thrid = pfl_getsysthrid();
		if (d->pld_nothrname[0] == '\0')
			snprintf(d->pld_nothrname,
			    sizeof(d->pld_nothrname), "<%d>", thrid);
		thrname = d->pld_nothrname;
	}

	ctx = psclog_get_fuse_context();

	gettimeofday(&tv, NULL);
	FMTSTR(prefix, sizeof(prefix), psc_logfmt,
		FMTSTRCASE('F', prefix, sizeof(prefix), "s", func)
		FMTSTRCASE('f', prefix, sizeof(prefix), "s", fn)
		FMTSTRCASE('H', prefix, sizeof(prefix), "s", d->pld_hostname)
		FMTSTRCASE('h', prefix, sizeof(prefix), "s", d->pld_hostshort)
		FMTSTRCASE('i', prefix, sizeof(prefix), "d", thrid)
		FMTSTRCASE('L', prefix, sizeof(prefix), "d", level)
		FMTSTRCASE('l', prefix, sizeof(prefix), "d", line)
		FMTSTRCASE('n', prefix, sizeof(prefix), "s", thrname)
		FMTSTRCASE('P', prefix, sizeof(prefix), "d", ctx ? (int)ctx->pid : -1)
		FMTSTRCASE('r', prefix, sizeof(prefix), "d", d->pld_rank)
		FMTSTRCASE('s', prefix, sizeof(prefix), "lu", tv.tv_sec)
		FMTSTRCASE('T', prefix, sizeof(prefix), "s", psc_subsys_name(subsys))
		FMTSTRCASE('t', prefix, sizeof(prefix), "d", subsys)
		FMTSTRCASE('U', prefix, sizeof(prefix), "d", ctx ? (int)ctx->uid : -1)
		FMTSTRCASE('u', prefix, sizeof(prefix), "lu", tv.tv_usec)
	);

	rc = strlcpy(fmtbuf, fmt, sizeof(fmtbuf));
	if (rc >= (int)sizeof(fmtbuf)) {
		warnx("psclog error: string too long");
		rc = sizeof(fmtbuf) - 1;
	}
	for (p = fmtbuf + rc - 1; p >= fmtbuf && *p == '\n'; p--)
		*p = '\0';

	PSCLOG_LOCK();
	/* consider using fprintf_unlocked() for speed */
	fprintf(stderr, "%s", prefix);
	vfprintf(stderr, fmtbuf, ap);
	if (options & PLO_ERRNO)
		fprintf(stderr, ": %s", APP_STRERROR(save_errno));
	fprintf(stderr, "%s", psclog_eol);
	PSCLOG_UNLOCK();

	if (level == PLL_FATAL) {
		abort();
		_exit(1);
	}

	/* Restore in case app needs it after our fprintf()'s may have modified it. */
	errno = save_errno;
}

void
_psclog(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);
}

__dead void
_psc_fatal(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);

	psc_fatalx("should not reach here");
}

__dead void
_psc_fatalv(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, va_list ap)
{
	_psclogv(fn, func, line, subsys, level, options, fmt, ap);
	psc_fatalx("should not reach here");
}

/* Keep synced with LL_* constants. */
const char *psc_loglevel_names[] = {
	"fatal",
	"error",
	"warn",
	"notice",
	"info",
	"debug",
	"trace"
};

const char *
psc_loglevel_getname(int id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= PNLOGLEVELS)
		return ("<unknown>");
	return (psc_loglevel_names[id]);
}

int
psc_loglevel_getid(const char *name)
{
	struct {
		const char	*lvl_name;
		int		 lvl_value;
	} altloglevels[] = {
		{ "none",	PLL_FATAL },
		{ "fatals",	PLL_FATAL },
		{ "errors",	PLL_ERROR },
		{ "warning",	PLL_WARN },
		{ "warnings",	PLL_WARN },
		{ "notify",	PLL_NOTICE },
		{ "all",	PLL_TRACE }
	};
	size_t n;

	for (n = 0; n < PNLOGLEVELS; n++)
		if (strcasecmp(name, psc_loglevel_names[n]) == 0)
			return (n);
	for (n = 0; n < nitems(altloglevels); n++)
		if (strcasecmp(name, altloglevels[n].lvl_name) == 0)
			return (altloglevels[n].lvl_value);
	return (-1);
}
