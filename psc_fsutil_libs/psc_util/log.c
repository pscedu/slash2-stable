/* $Id$ */

/*
 * Logging routines.
 * Notes:
 *	(o) We cannot use psc_fatal() for fatal errors here.  Instead use err(3).
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

#include "psc_util/alloc.h"
#include "psc_util/cdefs.h"
#include "psc_util/fmtstr.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

#ifndef PSC_LOG_FMT
#define PSC_LOG_FMT "[%s:%06u %n:%F:%l]"
#endif

#define PSC_LOG_INIT()				\
	do {					\
		if (!psc_loginit)		\
			psc_log_init();		\
	} while (0)

__static int		 psc_loginit;
__static const char	*psc_logfmt = PSC_LOG_FMT;
__static int		 psc_loglevel = PLL_TRACE;

__static void
psc_log_init(void)
{
	static psc_spinlock_t lock = LOCK_INITIALIZER;
	char *ep, *p;
	long l;

	spinlock(&lock);
	if (psc_loginit == 0) {
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
		psc_loginit = 1;
	}
	freelock(&lock);
}

int
psc_log_getlevel_global(void)
{
	PSC_LOG_INIT();
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

__weak const char *
pscthr_getname(void)
{
	return (NULL);
}

const char *
psc_log_getformat(void)
{
	return (psc_logfmt);
}

void
psclogv(__unusedx const char *fn, const char *func, int line, int subsys,
    int level, int options, const char *fmt, va_list ap)
{
	static char hostname[HOST_NAME_MAX];
	static int rank=-1;

	char prefix[LINE_MAX], emsg[LINE_MAX], umsg[LINE_MAX], nothrname[20], *p;
	const char *thrname, *logfmt;
	struct timeval tv;
	int save_errno;

	PSC_LOG_INIT();

	save_errno = errno;

	if (psc_log_getlevel(subsys) < level)
		return;

	logfmt = psc_log_getformat();
	thrname = pscthr_getname();
	if (thrname == NULL) {
		snprintf(nothrname, sizeof(nothrname), "<%d>", getpid());
		thrname = nothrname;
	}

	if (*hostname == '\0') {
		if (gethostname(hostname, sizeof(hostname)) == -1)
			err(1, "gethostname");
		if ((p = strchr(hostname, '.')) != NULL)
			*p = '\0';
	}

	if (rank == -1)
#ifdef LINUX
		rank = getpid();
#else
	        rank = cnos_get_rank();
#endif

	gettimeofday(&tv, NULL);
	FMTSTR(prefix, sizeof(prefix), logfmt,
		FMTSTRCASE('F', prefix, sizeof(prefix), "s", func)
		FMTSTRCASE('f', prefix, sizeof(prefix), "s", fn)
		FMTSTRCASE('h', prefix, sizeof(prefix), "s", hostname)
		FMTSTRCASE('L', prefix, sizeof(prefix), "d", level)
		FMTSTRCASE('l', prefix, sizeof(prefix), "d", line)
		FMTSTRCASE('n', prefix, sizeof(prefix), "s", thrname)
		FMTSTRCASE('r', prefix, sizeof(prefix), "d", rank)
		FMTSTRCASE('s', prefix, sizeof(prefix), "lu", tv.tv_sec)
		FMTSTRCASE('u', prefix, sizeof(prefix), "lu", tv.tv_usec)
// XXX fuse_get_context()->pid
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
	int n;

	for (n = 0; n < PNLOGLEVELS; n++)
		if (strcasecmp(name, psc_loglevel_names[n]) == 0)
			return (n);
	for (n = 0; n < NENTRIES(altloglevels); n++)
		if (strcasecmp(name, altloglevels[n].lvl_name) == 0)
			return (altloglevels[n].lvl_value);
	return (-1);
}
