/* $Id: log.c 1924 2007-10-19 16:04:41Z yanovich $ */

/*
 * Zestion logging routines.
 */

#include "subsys.h"
#define SUBSYS S_LOG

#include <sys/param.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/log.h"
#include "psc_util/fmtstr.h"
#include "psc_util/cdefs.h"

/*
 * The XT3 NoSys Port doesn't have this
 */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define DEF_LOGFMT "[%s:%06u %n:%F:%l]"

static const char *logFormat = DEF_LOGFMT;

/*
 * Global Zest logging level.
 * May be overridden in individual utilities (such as zestiond).
 */
static int defaultLogVal = LL_TRACE;

int
psc_setloglevel(int new)
{
	int old;

	old = defaultLogVal;
	if (new < 0)
		new = 0;
	else if (new >= NLOGLEVELS)
		new = NLOGLEVELS - 1;
	defaultLogVal = new;
	return (old);
}

int
psc_getloglevel(void)
{
	static int readenv;
	const char *p;
	char *ep;
	long l;

	if (!readenv) {
		readenv = 1;
		if ((p = getenv("PSC_LOG_LEVEL")) != NULL) {
			ep = NULL;
			errno = 0;
			l = strtol(p, &ep, 10);
			if (p[0] == '\0' || ep == NULL || *ep != '\0')
				errx(1, "invalid log level env: %s", p);
			if (errno == ERANGE || l <= 0 || l >= NLOGLEVELS)
				errx(1, "invalid log level env: %s", p);
			defaultLogVal = (int)l;
		}
	}
	return (defaultLogVal);
}

__weak int
pscthr_getloglevel(__unusedx int subsys)
{
	return (psc_getloglevel());
}

__weak const char *
pscthr_getname(void)
{
	return (NULL);
}

const char *
psc_getlogformat(void)
{
	static int readenv;
	const char *p;

	if (!readenv) {
		readenv = 1;
		if ((p = getenv("PSC_LOG_FORMAT")) != NULL)
			logFormat = p;
	}
	return (logFormat);
}

void
vpsclog(__unusedx const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, va_list ap)
{
	char hostname[HOST_NAME_MAX], emsg[LINE_MAX], umsg[LINE_MAX];
	char *p, prefix[LINE_MAX], nothrname[20];
	const char *thrname, *logfmt;
	struct timeval tv;
	int save_errno;

	save_errno = errno;

	if (pscthr_getloglevel(subsys) < level)
		return;

	logfmt = psc_getlogformat();
	thrname = pscthr_getname();
	if (thrname == NULL) {
		snprintf(nothrname, sizeof(nothrname), "<%d>", getpid());
		thrname = nothrname;
	}

	if (gethostname(hostname, sizeof(hostname)) == -1)
		err(1, "gethostname");
	if ((p = strchr(hostname, '.')) != NULL)
		*p = '\0';

	gettimeofday(&tv, NULL);
	FMTSTR(prefix, sizeof(prefix), logfmt,
		FMTSTRCASE('n', prefix, sizeof(prefix), "s", thrname)
		FMTSTRCASE('L', prefix, sizeof(prefix), "d", level)
		FMTSTRCASE('l', prefix, sizeof(prefix), "d", line)
		FMTSTRCASE('s', prefix, sizeof(prefix), "lu", tv.tv_sec)
		FMTSTRCASE('u', prefix, sizeof(prefix), "lu", tv.tv_usec)
		FMTSTRCASE('f', prefix, sizeof(prefix), "s", fn)
		FMTSTRCASE('F', prefix, sizeof(prefix), "s", func)
		FMTSTRCASE('h', prefix, sizeof(prefix), "s", hostname)
	);

	/*
	 * Write into intermediate buffers and send it all at once
	 * to prevent threads weaving between printf() calls.
	 */
	vsnprintf(umsg, sizeof(umsg), fmt, ap);
	if (umsg[strlen(umsg)-1] == '\n')
		umsg[strlen(umsg)-1] = '\0';

	if (options & LO_ERRNO)
		snprintf(emsg, sizeof(emsg), ": %s",
		    strerror(save_errno));
	else
		emsg[0] = '\0';
	fprintf(stderr, "%s %s%s\n", prefix, umsg, emsg);
	errno = save_errno; /* Restore in case it is needed further. */
}

void
_psclog(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vpsclog(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);
}

__dead void
_psc_fatal(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vpsclog(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);

	abort();
}

/* Keep synced with LL_* constants. */
const char *loglevel_names[] = {
	"fatal",
	"error",
	"warn",
	"notice",
	"info",
	"debug",
	"trace"
};

const char *
psclog_name(int id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= NLOGLEVELS)
		return ("<unknown>");
	return (loglevel_names[id]);
}

int
psclog_id(const char *name)
{
	struct {
		const char	*lvl_name;
		int		 lvl_value;
	} altloglevels[] = {
		{ "none",	LL_FATAL },
		{ "fatals",	LL_FATAL },
		{ "errors",	LL_ERROR },
		{ "warning",	LL_WARN },
		{ "warnings",	LL_WARN },
		{ "notify",	LL_NOTICE },
		{ "all",	LL_DEBUG }
	};
	int n;

	for (n = 0; n < NLOGLEVELS; n++)
		if (strcasecmp(name, loglevel_names[n]) == 0)
			return (n);
	for (n = 0; n < NENTRIES(altloglevels); n++)
		if (strcasecmp(name, altloglevels[n].lvl_name) == 0)
			return (altloglevels[n].lvl_value);
	return (-1);
}
