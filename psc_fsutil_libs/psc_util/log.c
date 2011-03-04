/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2011, Pittsburgh Supercomputing Center (PSC).
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
 * Debug/logging routines.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
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
#define PSC_LOG_FMT "[%s:%06u %n:%i:%T %F %l] "
#endif

struct fuse_context {
	uid_t uid;
	gid_t gid;
	pid_t pid;
};

const char			*psc_logfmt = PSC_LOG_FMT;
__static enum psclog_level	 psc_loglevel = PLL_WARN;
__static struct psclog_data	*psc_logdata;
char				 psclog_eol[8] = "\n";	/* overrideable with ncurses EOL */

void
psc_log_init(void)
{
	char *p;

	p = getenv("PSC_LOG_FILE");
	if (p && freopen(p, "w", stderr) == NULL)
		warn("%s", p);

	p = getenv("PSC_LOG_FORMAT");
	if (p)
		psc_logfmt = p;

	p = getenv("PSC_LOG_LEVEL");
	if (p) {
		psc_loglevel = psc_loglevel_fromstr(p);
		if (psc_loglevel == PNLOGLEVELS)
			errx(1, "invalid PSC_LOG_LEVEL: %s", p);
	}
}

enum psclog_level
psc_log_getlevel_global(void)
{
	return (psc_loglevel);
}

__weak enum psclog_level
psc_log_getlevel_ss(__unusedx int subsys)
{
	return (psc_log_getlevel_global());
}

__weak enum psclog_level
psc_log_getlevel(int subsys)
{
	return (psc_log_getlevel_ss(subsys));
}

void
psc_log_setlevel_global(enum psclog_level newlevel)
{
	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		errx(1, "log level out of bounds (%d)", newlevel);
	psc_loglevel = newlevel;
}

__weak void
psc_log_setlevel_ss(__unusedx int subsys, enum psclog_level newlevel)
{
	psc_log_setlevel_global(newlevel);
}

__weak void
psc_log_setlevel(int subsys, enum psclog_level newlevel)
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
 * MPI_Comm_rank - Dummy overrideable MPI rank retriever.
 */
__weak int
MPI_Comm_rank(__unusedx int comm, int *rank)
{
	*rank = -1;
	return (0);
}

/**
 * fuse_get_context - Dummy overrideable fuse context retriever.
 */
__weak struct fuse_context *
psclog_get_fuse_context(void)
{
	return (NULL);
}

/**
 * psc_subsys_name - Dummy overrideable PFL subsystem ID -> name resolver.
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

	if (pfl_tls_get(PFL_TLSIDX_LOGDATA, sizeof(*d), &d)) {
		/* XXX use psc_get_hostname() */
		if (gethostname(d->pld_hostname,
		    sizeof(d->pld_hostname)) == -1)
			err(1, "gethostname");
		strlcpy(d->pld_hostshort, d->pld_hostname,
		    sizeof(d->pld_hostshort));
		if ((p = strchr(d->pld_hostshort, '.')) != NULL)
			*p = '\0';
		/* XXX try to read this if the pscthr is available */
		d->pld_thrid = pfl_getsysthrid();
#ifdef HAVE_CNOS
		int cnos_get_rank(void);

		d->pld_rank = cnos_get_rank();
#else
		MPI_Comm_rank(1, &d->pld_rank); /* 1=MPI_COMM_WORLD */
#endif
	}
	return (d);
}

const char *
pfl_fmtlogdate(const struct timeval *tv, const char **s)
{
	char fmtbuf[LINE_MAX], *bufp;
	const char *end, *start;
	struct tm tm;
	time_t sec;

	start = *s + 1;
	if (*start != '<')
		errx(1, "invalid log prefix format: %s", start);
	for (end = start++;
	    *end && *end != '>' && end - start < LINE_MAX; end++)
		;
	if (*end != '>')
		errx(1, "invalid log prefix format: %s", end);

	memcpy(fmtbuf, start, end - start);
	fmtbuf[end - start] = '\0';

	pfl_tls_get(PFL_TLSIDX_LOGDATEBUF, LINE_MAX, &bufp);

	sec = tv->tv_sec;
	localtime_r(&sec, &tm);
	strftime(bufp, LINE_MAX, fmtbuf, &tm);

	*s = end;
	return (bufp);
}

void
_psclogv(const struct pfl_callerinfo *pci, enum psclog_level level,
    int options, const char *fmt, va_list ap)
{
	char *p, prefix[LINE_MAX], fmtbuf[LINE_MAX];
	extern const char *progname;
	struct fuse_context *ctx;
	struct psc_thread *thr;
	struct psclog_data *d;
	struct timeval tv;
	const char *thrname;
	int rc, save_errno;
	va_list apd;
	pid_t thrid;
	FILE *fp;

	save_errno = errno;

	d = psclog_getdata();
	thr = pscthr_get_canfail();
	if (thr) {
		thrid = thr->pscthr_thrid;
		thrname = thr->pscthr_name;
	} else {
		thrid = d->pld_thrid;
		if (d->pld_nothrname[0] == '\0')
			snprintf(d->pld_nothrname,
			    sizeof(d->pld_nothrname), "<%d>", thrid);
		thrname = d->pld_nothrname;
	}

	ctx = psclog_get_fuse_context();

	gettimeofday(&tv, NULL);
	FMTSTR(prefix, sizeof(prefix), psc_logfmt,
		FMTSTRCASE('B', "s", pfl_basename(pci->pci_filename))
		FMTSTRCASE('D', "s", pfl_fmtlogdate(&tv, &_t))
		FMTSTRCASE('F', "s", pci->pci_func)
		FMTSTRCASE('f', "s", pci->pci_filename)
		FMTSTRCASE('H', "s", d->pld_hostname)
		FMTSTRCASE('h', "s", d->pld_hostshort)
		FMTSTRCASE('i', "d", thrid)
		FMTSTRCASE('L', "d", level)
		FMTSTRCASE('l', "d", pci->pci_lineno)
		FMTSTRCASE('N', "s", progname)
		FMTSTRCASE('n', "s", thrname)
		FMTSTRCASE('P', "d", ctx ? (int)ctx->pid : -1)
		FMTSTRCASE('r', "d", d->pld_rank)
		FMTSTRCASE('s', "lu", tv.tv_sec)
		FMTSTRCASE('T', "s", psc_subsys_name(pci->pci_subsys))
		FMTSTRCASE('t', "d", pci->pci_subsys)
		FMTSTRCASE('U', "d", ctx ? (int)ctx->uid : -1)
		FMTSTRCASE('u', "lu", tv.tv_usec)
	);

	rc = strlcpy(fmtbuf, fmt, sizeof(fmtbuf));
	if (rc >= (int)sizeof(fmtbuf)) {
		warnx("psclog error: string too long");
		rc = sizeof(fmtbuf) - 1;
	}
	for (p = fmtbuf + rc - 1; p >= fmtbuf && *p == '\n'; p--)
		*p = '\0';

	va_copy(apd, ap);

	PSCLOG_LOCK();
	/* consider using fprintf_unlocked() for speed */
	fprintf(stderr, "%s", prefix);
	vfprintf(stderr, fmtbuf, ap);
	if (options & PLO_ERRNO)
		fprintf(stderr, ": %s", APP_STRERROR(save_errno));
	fprintf(stderr, "%s", psclog_eol);
	fflush(stderr);

	if (level <= PLL_ERROR) {
		if (!isatty(fileno(stderr))) {
			fp = fopen(_PATH_TTY, "w");
			if (fp) {
				fprintf(fp, "%s", prefix);
				vfprintf(fp, fmtbuf, apd);
				if (options & PLO_ERRNO)
					fprintf(fp, ": %s", APP_STRERROR(save_errno));
				fprintf(fp, "\n");
				fclose(fp);
			}
		}
		if (level == PLL_FATAL) {
//			exit(1);
			abort();
			_exit(1);
		}
	}

	PSCLOG_UNLOCK();

	/* Restore in case app needs it after our fprintf()'s may have modified it. */
	errno = save_errno;
}

void
_psclog(const struct pfl_callerinfo *pci,
    enum psclog_level level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(pci, level, options, fmt, ap);
	va_end(ap);
}

__dead void
_psc_fatal(const struct pfl_callerinfo *pci,
    enum psclog_level level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(pci, level, options, fmt, ap);
	va_end(ap);

	errx(1, "should not reach here");
}

__dead void
_psc_fatalv(const struct pfl_callerinfo *pci,
    enum psclog_level level, int options, const char *fmt, va_list ap)
{
	_psclogv(pci, level, options, fmt, ap);
	errx(1, "should not reach here");
}

/* Keep synced with PLL_* constants. */
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
psc_loglevel_getname(enum psclog_level id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= PNLOGLEVELS)
		return ("<unknown>");
	return (psc_loglevel_names[id]);
}

enum psclog_level
psc_loglevel_fromstr(const char *name)
{
	struct {
		const char		*lvl_name;
		enum psclog_level	 lvl_value;
	} altloglevels[] = {
		{ "none",		PLL_FATAL },
		{ "fatals",		PLL_FATAL },
		{ "errors",		PLL_ERROR },
		{ "warning",		PLL_WARN },
		{ "warnings",		PLL_WARN },
		{ "notify",		PLL_NOTICE },
		{ "all",		PLL_TRACE }
	};
	char *endp;
	size_t n;
	long l;

	for (n = 0; n < PNLOGLEVELS; n++)
		if (strcasecmp(name, psc_loglevel_names[n]) == 0)
			return (n);
	for (n = 0; n < nitems(altloglevels); n++)
		if (strcasecmp(name, altloglevels[n].lvl_name) == 0)
			return (altloglevels[n].lvl_value);

	l = strtol(name, &endp, 10);
	if (endp == name || *endp != '\0' || l < 0 || l >= PNLOGLEVELS)
		return (PNLOGLEVELS);
	return (l);
}
