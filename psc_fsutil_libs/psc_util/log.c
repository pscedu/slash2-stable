/* $Id$ */

/*
 * Logging routines.
 */

#define PSC_SUBSYS PSS_LOG

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

#define DEF_LOGFMT "[%s:%06u %n:%F:%l]"

const char *pscLogFormat = DEF_LOGFMT;

/* Global logging level. */
__static int pscLogLevel = PLL_TRACE;

void
psc_setlogformat(const char *fmt)
{
	pscLogFormat = fmt;
}

int
psc_setloglevel(int new)
{
	int old;

	old = pscLogLevel;
	if (new < 0)
		new = 0;
	else if (new >= PNLOGLEVELS)
		new = PNLOGLEVELS - 1;
	pscLogLevel = new;
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
		if ((p = getenv("PSC_LOG_LEVEL")) != NULL) {
			ep = NULL;
			errno = 0;
			l = strtol(p, &ep, 10);
			if (p[0] == '\0' || ep == NULL || *ep != '\0')
				errx(1, "invalid log level env: %s", p);
			if (errno == ERANGE || l <= 0 || l >= PNLOGLEVELS)
				errx(1, "invalid log level env: %s", p);
			pscLogLevel = (int)l;
		}
		readenv = 1;
	}
	return (pscLogLevel);
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
		if ((p = getenv("PSC_LOG_FORMAT")) != NULL)
			pscLogFormat = p;
		readenv = 1;
	}
	return (pscLogFormat);
}

void
psclogv(__unusedx const char *fn, const char *func, int line, int subsys,
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

	if (options & PLO_ERRNO)
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
	psclogv(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);
}

__dead void
_psc_fatal(const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	psclogv(fn, func, line, subsys, level, options, fmt, ap);
	va_end(ap);

	abort();
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
psclog_name(int id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= PNLOGLEVELS)
		return ("<unknown>");
	return (psc_loglevel_names[id]);
}

int
psclog_id(const char *name)
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
		{ "all",	PLL_DEBUG }
	};
	int n;

	for (n = 0; n < PNLOGLEVELS; n++)
		if (strcasecmp(name, psc_loglevel_names[n]) == 0)
			return (n);
	for (n = 0; n < NENTRIES(altloglevels); n++)
		if (strcasecmp(name, altloglevels[n].lvl_name) == 0)
			return (altloglevels[n].lvl_value);
	return (-1);
}
