/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2012, Pittsburgh Supercomputing Center (PSC).
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
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "pfl/time.h"
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
#define PSC_LOG_FMT "[%s:%06u %n:%I:%T %B %F %l] "
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
int				*pfl_syslog;

int pfl_syslog_map[] = {
/* fatal */	LOG_EMERG,
/* error */	LOG_ERR,
/* warn */	LOG_WARNING,
/* notice */	LOG_NOTICE,
/* info */	LOG_INFO
};

int
psc_log_setfn(const char *p, const char *mode)
{
	char *lp, fn[PATH_MAX];
	struct timeval tv;

	PFL_GETTIMEVAL(&tv);
	FMTSTR(fn, sizeof(fn), p,
		FMTSTRCASE('t', "d", tv.tv_sec)
	);
	if (freopen(fn, mode, stderr) == NULL)
		return (errno);

	lp = getenv("PSC_LOG_FILE_LINK");
	if (lp) {
		if (unlink(lp) == -1 && errno != ENOENT)
			warn("unlink %s", lp);
		if (link(fn, lp) == -1)
			warn("link %s", lp);
	}

#if 0
	struct statvfs sfb;

	if (fstatvfs(fileno(stderr), &sfb) == -1)
		warn("statvfs stderr");
	else {
		if (nfs)
			warn("warning: error log file over NFS");
	}
#endif

	return (0);
}

void
psc_log_init(void)
{
	char *p;

	p = getenv("PSC_LOG_FILE");
	if (p && psc_log_setfn(p, "w"))
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

	if (pfl_syslog) {
		extern const char *progname;

		openlog(progname, LOG_CONS | LOG_NDELAY | LOG_PERROR |
		    LOG_PID, LOG_DAEMON);
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

pid_t
psclog_get_fuse_ctx_pid(void)
{
	struct fuse_context *ctx;

	ctx = psclog_get_fuse_context();
	return (ctx ? ctx->pid : -1);
}

uid_t
psclog_get_fuse_ctx_uid(void)
{
	struct fuse_context *ctx;

	ctx = psclog_get_fuse_context();
	return (ctx ? ctx->uid : (uid_t)-1);
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

	d = pfl_tls_get(PFL_TLSIDX_LOGDATA, sizeof(*d));
	if (d->pld_thrid == 0) {
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
		snprintf(d->pld_nothrname, sizeof(d->pld_nothrname),
		    "<%"PSCPRI_PTHRT">", pthread_self());

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

	bufp = pfl_tls_get(PFL_TLSIDX_LOGDATEBUF, LINE_MAX);

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
	char *p, buf[LINE_MAX];
	extern const char *progname;
	struct psc_thread *thr;
	struct psclog_data *d;
	struct timeval tv;
	const char *thrname;
	int rc, save_errno;
	pid_t thrid;
	size_t len;
	FILE *fp;

	save_errno = errno;

	d = psclog_getdata();
	if (d->pld_flags & PLDF_INLOG) {
//		write(); ?
		// also place line, file, etc
		vfprintf(stderr, fmt, ap); /* XXX syslog, etc */

		if (level == PLL_FATAL)
			abort();
		goto out;
	}
	d->pld_flags |= PLDF_INLOG;

	thr = pscthr_get_canfail();
	if (thr) {
		thrid = thr->pscthr_thrid;
		thrname = thr->pscthr_name;
	} else {
		thrid = d->pld_thrid;
		thrname = d->pld_nothrname;
	}

	gettimeofday(&tv, NULL);
	FMTSTR(buf, sizeof(buf), psc_logfmt,
		FMTSTRCASE('B', "s", pfl_basename(pci->pci_filename))
		FMTSTRCASE('D', "s", pfl_fmtlogdate(&tv, &_t))
		FMTSTRCASE('F', "s", pci->pci_func)
		FMTSTRCASE('f', "s", pci->pci_filename)
		FMTSTRCASE('H', "s", d->pld_hostname)
		FMTSTRCASE('h', "s", d->pld_hostshort)
		FMTSTRCASE('I', PSCPRI_PTHRT, pthread_self())
		FMTSTRCASE('i', "d", thrid)
		FMTSTRCASE('L', "d", level)
		FMTSTRCASE('l', "d", pci->pci_lineno)
		FMTSTRCASE('N', "s", progname)
		FMTSTRCASE('n', "s", thrname)
		FMTSTRCASE('P', "d", psclog_get_fuse_ctx_pid())
		FMTSTRCASE('r', "d", d->pld_rank)
//		FMTSTRCASE('S', "s", call stack)
		FMTSTRCASE('s', "lu", tv.tv_sec)
		FMTSTRCASE('T', "s", psc_subsys_name(pci->pci_subsys))
		FMTSTRCASE('t', "d", pci->pci_subsys)
		FMTSTRCASE('U', "d", psclog_get_fuse_ctx_uid())
		FMTSTRCASE('u', "lu", tv.tv_usec)
	);

	len = strlen(buf);
	rc = vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
	if (rc != -1)
		len += rc;
	/* trim newline if present, since we add our own */
	if (len && buf[len - 1] == '\n')
		buf[--len] = '\0';
	if (options & PLO_ERRNO)
		snprintf(buf + len, sizeof(buf) - len,
		    ": %s", APP_STRERROR(save_errno));

	PSCLOG_LOCK();

	/* XXX consider using fprintf_unlocked() for speed */
	fprintf(stderr, "%s%s", buf, psclog_eol);
	fflush(stderr);

	if (pfl_syslog && pfl_syslog[pci->pci_subsys] &&
	    level >= 0 && level < (int)nitems(pfl_syslog_map))
		syslog(pfl_syslog_map[level], "%s", buf);

	if (level <= PLL_WARN && !isatty(fileno(stderr))) {
		fp = fopen(_PATH_TTY, "w");
		if (fp) {
			fprintf(fp, "%s\n", buf);
			fclose(fp);
		}
	}

	if (level == PLL_FATAL) {
		p = getenv("PSC_DUMPSTACK");
		if (p && strcmp(p, "0"))
			pfl_dump_stack();
		d->pld_flags &= ~PLDF_INLOG;
		abort(); // exit(1);
	}

	PSCLOG_UNLOCK();

	d->pld_flags &= ~PLDF_INLOG;

 out:
	/*
	 * Restore in case app needs it after our printf()'s may have
	 * modified it.
	 */
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
	"diag",
	"debug",
	"vdebug",
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
