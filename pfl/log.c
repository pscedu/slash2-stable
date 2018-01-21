/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2007-2016, Pittsburgh Supercomputing Center
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
 * Debug/logging routines.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
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

#ifdef HAVE_BACKTRACE
#  include <execinfo.h>
#endif

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/err.h"
#include "pfl/fmtstr.h"
#include "pfl/fs.h"
#include "pfl/hashtbl.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "pfl/thread.h"
#include "pfl/time.h"

#ifndef PSC_LOG_FMT
/*
 * To change the log format on-the-fly, you can use log.format.  For example:
 *
 * # msctl -p log.format="[%D<%c> %n:%I:%T %B %F %l]"
 *
 */

#define PSC_LOG_FMT "[%s.%06u00 %n:%I:%T %B %F %l]"
#endif

int				 psc_log_console = 0;

const char			*psc_logfmt = PSC_LOG_FMT;
int				 psc_logfmt_error = 0;

int				 psc_loglevel = PLL_NOTICE;
__static struct psclog_data	*psc_logdata;
char				 psclog_eol[8] = "\n";	/* overrideable with ncurses EOL */

char		 		 psc_hostshort[64];
char		 		 psc_hostname[64];

psc_spinlock_t			 psc_stack_lock = SPINLOCK_INIT;
void				*psc_stack_ptrbuf[32];
char		 		 psc_stack_symbuf[256];

/*
 * A user can define one or more PSC_SYSLOG_$subsys environment
 * variables or simply the PSC_SYSLOG environment variable to select
 * some or all subsystems whose log messages should also go to
 * syslog(3).  If so, the pfl_syslog_map[] array is used to map our log
 * level to a syslog(3) priority.
 */
int				*pfl_syslog;

int pfl_syslog_map[] = {
/* fatal */	LOG_EMERG,
/* error */	LOG_ERR,
/* warn */	LOG_WARNING,
/* notice */	LOG_NOTICE,
/* info */	LOG_INFO
};

FILE				*pflog_ttyfp;

struct psc_dynarray		_pfl_logpoints = DYNARRAY_INIT_NOLOG;
struct psc_hashtbl		_pfl_logpoints_hashtbl;

static int log_cycle_count;
static int log_rotate_count;
int pfl_log_rotate = PSC_MAX_LOG_PER_FILE;
 
static char *loglk;
static char  logfn[PATH_MAX];

void psc_should_rotate_log(void)
{
	int rc;
	char newfn[PATH_MAX];

	if (logfn[0] == '\0')
		return;

	log_rotate_count++;
	if (log_rotate_count < pfl_log_rotate)
		return;

	log_rotate_count = 0;
	rc = snprintf(newfn, sizeof(newfn), "%s-%d", 
	    logfn, log_cycle_count++);
	if (rc < 0) {
		warn("log snprintf rc = %d", rc);
		return;
	}
	rc = rename(logfn, newfn);
	if (rc < 0) {
		warn("log rename rc = %d", rc);
		return;
	}

	if (freopen(logfn, "w", stderr) == NULL) {
		warn("log freopen %s, rc = %d", logfn, rc);
		return;
	}
	if (loglk) {
		if (unlink(loglk) == -1 && errno != ENOENT) 
			warn("log unlink %s, rc = %d", loglk, errno);
		if (link(logfn, loglk) == -1)
			warn("log link %s, rc = %d", loglk, errno);
	}
}

int
psc_log_setfn(const char *p, const char *mode)
{
	static int logger_pid = -1;
	struct timeval tv;
	int rc;
	char *lp;

	PFL_GETTIMEVAL(&tv);
	(void)FMTSTR(logfn, sizeof(logfn), p,
		FMTSTRCASE('t', "d", tv.tv_sec)
	);
	if (freopen(logfn, mode, stderr) == NULL)
		return (errno);

	loglk = getenv("PSC_LOG_FILE_LINK");
	if (loglk) {
		if (unlink(loglk) == -1 && errno != ENOENT)
			warn("unlink %s", loglk);
		if (link(logfn, loglk) == -1)
			warn("link %s", loglk);
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

	lp = getenv("PFL_SYSLOG_PIPE");
	if (lp) {
		/* cleanup old */
		if (logger_pid != -1)
			kill(logger_pid, SIGINT);

		/* launch new */
		logger_pid = fork();
		switch (logger_pid) {
		case -1:
			warn("fork");
			break;
		case 0: {
			char cmdbuf[LINE_MAX];

			rc = snprintf(cmdbuf, sizeof(cmdbuf),
			    "tail -f %s | %s", logfn, lp);
			if (rc < 0 || rc > (int)sizeof(cmdbuf))
				errx(1, "snprintf");
			exit(system(cmdbuf));
		    }
		default:
			rc = waitpid(logger_pid, NULL, 0);
			break;
		}
	}

	return (0);
}

void
psc_log_init(void)
{
	char *p;

	p = getenv("PSC_LOG_FILE");
	if (p && psc_log_setfn(p, "w"))
		errx(1, "%s", p);

	p = getenv("PSC_LOG_FORMAT");
	if (p)
		psc_logfmt = p;

	p = getenv("PSC_LOG_LEVEL");
	if (p) {
		psc_loglevel = psc_loglevel_fromstr(p);
		if (psc_loglevel == PNLOGLEVELS)
			errx(1, "invalid PSC_LOG_LEVEL: %s", p);
	}

	_psc_hashtbl_init(&_pfl_logpoints_hashtbl, PHTF_STRP |
	    PHTF_NOLOG, offsetof(struct pfl_logpoint, plogpt_key),
	    sizeof(struct pfl_logpoint), 3067, NULL, "logpoints");

	if (!isatty(fileno(stderr)))
		pflog_ttyfp = fopen(_PATH_TTY, "w");

	if (gethostname(psc_hostname, sizeof(psc_hostname)) == -1)
		err(1, "gethostname");
	strlcpy(psc_hostshort, psc_hostname, sizeof(psc_hostshort));
	if ((p = strchr(psc_hostshort, '.')) != NULL)
		*p = '\0';
}

int
psc_log_getlevel_global(void)
{
	return (psc_loglevel);
}

void
psc_log_setlevel_global(int newlevel)
{
	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		errx(1, "log level out of bounds (%d)", newlevel);
	psc_loglevel = newlevel;
}

#ifndef HAVE_LIBPTHREAD

int
psc_log_getlevel(int subsys)
{
	return (psc_log_getlevel_ss(subsys));
}

void
psc_log_setlevel(int subsys, int newlevel)
{
	psc_log_setlevel_ss(subsys, newlevel);
}

#endif

uid_t
pflog_get_fsctx_uid_stub(__unusedx struct psc_thread *thr)
{
	return (-1);
}

pid_t
pflog_get_fsctx_pid_stub(__unusedx struct psc_thread *thr)
{
	return (-1);
}

const char *
pflog_get_peer_addr_stub(__unusedx struct psc_thread *thr)
{
	return ("");
}

pid_t		 (*pflog_get_fsctx_pid)(struct psc_thread *) =
		    pflog_get_fsctx_pid_stub;
uid_t		 (*pflog_get_fsctx_uid)(struct psc_thread *) =
		    pflog_get_fsctx_uid_stub;
const char	*(*pflog_get_peer_addr)(struct psc_thread *) =
		    pflog_get_peer_addr_stub;

const char *
pfl_fmtlogdate(const struct timeval *tv, const char **s, char *bufp)
{
	char fmtbuf[LINE_MAX];
	const char *end, *start;
	struct tm tm;
	time_t sec;

	start = *s + 1;
	if (*start != '<') {
		if (psc_logfmt_error < 5) {
			psc_logfmt_error++;
			warnx("invalid log prefix format: %s", start);
		}
		*s = *s + 1;
		return ("");
	}
	for (end = start++;
	    *end && *end != '>' && end - start < LINE_MAX; end++)
		;
	if (*end != '>') {
		if (psc_logfmt_error < 5) {
			psc_logfmt_error++;
			warnx("invalid log suffix format: %s", end);
		}
		*s = *s + 1;
		return ("");
	}

	memcpy(fmtbuf, start, end - start);
	fmtbuf[end - start] = '\0';

	sec = tv->tv_sec;
	localtime_r(&sec, &tm);
	strftime(bufp, LINE_MAX, fmtbuf, &tm);

	*s = end;
	return (bufp);
}

const char *
pflog_get_stacktrace()
{
#ifdef HAVE_BACKTRACE
	char **symv, *sym, *name, *end;
	int rc, i, n, adj = 0, len;

	spinlock(&psc_stack_lock);
	n = backtrace(psc_stack_ptrbuf, nitems(psc_stack_ptrbuf));
	symv = backtrace_symbols(psc_stack_ptrbuf, n);
	if (symv == NULL) {
		freelock(&psc_stack_lock);
		return ("");
	}

	for (i = 2; i < n; i++) {
		sym = symv[i];
		name = strchr(sym, '(');
		if (name == NULL)
			goto out;
		name++;
		end = strchr(name, '+');
		if (end == NULL)
			goto out;
		len = end - name;

		if ((len == 4 && strncmp(name, "main", len) == 0) ||
		    (len == 5 && strncmp(name, "clone", len) == 0))
			break;
	}
	i--;
	for (; i > 2; i--) {
		sym = symv[i];
		name = strchr(sym, '(') + 1;
		end = strchr(name, '+');
		len = end - name;

		if (len == 7 && strncmp(name, "_psclog", len) == 0)
			break;

		rc = snprintf(psc_stack_symbuf + adj,
		    sizeof(psc_stack_symbuf) - adj,
		    "%s%.*s", adj ? ":" : "", len, name);
		if (rc == -1)
			goto out;
		adj += rc;
	}

	if (0)
 out:
		printf("bail\n");
	free(symv);
	freelock(&psc_stack_lock);
	return (psc_stack_symbuf);
#else
	return ("");
#endif
}

void
_psclogv(const struct pfl_callerinfo *pci, int level, int options,
    const char *fmt, va_list ap)
{
	char bufp[LINE_MAX];
	char *p, buf[BUFSIZ];
	extern const char *__progname;
	struct psc_thread *thr;
	struct timeval tv;
	const char *thrname;
	int rc, save_errno;
	pid_t thrid;
	size_t len;

	save_errno = errno;

	thr = pscthr_get();
	/*
	 * XXX Set log level 5 crashes right away.
	 */
	if (!thr)
		return;
	thrid = thr->pscthr_thrid;
	thrname = thr->pscthr_name;

	gettimeofday(&tv, NULL);
	(void)FMTSTR(buf, sizeof(buf), psc_logfmt,
		FMTSTRCASE('A', "s", pflog_get_peer_addr(thr))
		FMTSTRCASE('B', "s", pfl_basename(pci->pci_filename))
		FMTSTRCASE('D', "s", pfl_fmtlogdate(&tv, &_t, bufp))
		FMTSTRCASE('F', "s", pci->pci_func)
		FMTSTRCASE('f', "s", pci->pci_filename)
		FMTSTRCASE('H', "s", psc_hostname)
		FMTSTRCASE('h', "s", psc_hostshort)
		FMTSTRCASE('I', PSCPRI_PTHRT, pthread_self())
		FMTSTRCASE('i', "d", thrid)
		FMTSTRCASE('L', "d", level)
		FMTSTRCASE('l', "d", pci->pci_lineno)
		FMTSTRCASE('N', "s", __progname)
		FMTSTRCASE('n', "s", thrname)
		FMTSTRCASE('P', "d", pflog_get_fsctx_pid(thr))
		FMTSTRCASE('S', "s", pflog_get_stacktrace())
		FMTSTRCASE('s', "lu", tv.tv_sec)
		FMTSTRCASE('T', "s", pfl_subsys_name(pci->pci_subsys))
		FMTSTRCASE('t', "d", pci->pci_subsys)
		FMTSTRCASE('U', "d", pflog_get_fsctx_uid(thr))
		FMTSTRCASE('u', "lu", tv.tv_usec)
	);

	len = strlen(buf);
	rc = vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
	if (rc != -1)
		len = strlen(buf);
	/* trim newline if present, since we add our own */
	if (len && buf[len - 1] == '\n')
		buf[--len] = '\0';
	if (options & PLO_ERRNO)
		snprintf(buf + len, sizeof(buf) - len,
		    ": %s", pfl_strerror(save_errno));

	PSCLOG_LOCK();
	psc_should_rotate_log();

	/* XXX consider using fprintf_unlocked() for speed */
	rc = fprintf(stderr, "%s%s", buf, psclog_eol);
	if (rc < 0)
		pfl_abort();
		
	
	rc = fflush(stderr);
	if (rc)
		pfl_abort();

	if (pfl_syslog && pfl_syslog[pci->pci_subsys] &&
	    level >= 0 && level < (int)nitems(pfl_syslog_map))
		syslog(pfl_syslog_map[level], "%s", buf);

	/*
 	 * On 2.6.32-131.12.1.el6.x86_64-netboot, a spate of log
 	 * messages cause the thread to hang in the following
 	 * fprintf(), dragging everyone down.
 	 */
	if (level <= PLL_WARN && psc_log_console && pflog_ttyfp)
		fprintf(pflog_ttyfp, "%s\n", buf);

	if (level == PLL_FATAL) {
		p = getenv("PSC_DUMPSTACK");
		if (p && strcmp(p, "0"))
			pfl_dump_stack();
		pfl_abort();
	}

	PSCLOG_UNLOCK();

	/*
	 * Restore in case app needs it after our printf()'s may have
	 * modified it.
	 */
	errno = save_errno;
}

void
_psclog(const struct pfl_callerinfo *pci, int level, int options,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(pci, level, options, fmt, ap);
	va_end(ap);
}

__dead void
_psc_fatal(const struct pfl_callerinfo *pci, int level, int options,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_psclogv(pci, level, options, fmt, ap);
	va_end(ap);

	errx(1, "should not reach here");
}

__dead void
_psc_fatalv(const struct pfl_callerinfo *pci, int level, int options,
    const char *fmt, va_list ap)
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
psc_loglevel_getname(int id)
{
	if (id < 0)
		return ("<unknown>");
	else if (id >= PNLOGLEVELS)
		return ("<unknown>");
	return (psc_loglevel_names[id]);
}

int
psc_loglevel_fromstr(const char *name)
{
	struct {
		const char		*lvl_name;
		int			 lvl_value;
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

struct pfl_logpoint *
_pfl_get_logpointid(const char *fn, int line, int create)
{
	static struct psc_spinlock lock = SPINLOCK_INIT_NOLOG;
	struct psc_hashtbl *t = &_pfl_logpoints_hashtbl;
	struct pfl_logpoint *pt;
	struct psc_hashbkt *b;
	char *key;

	if (asprintf(&key, "%s:%d", pfl_basename(fn), line) == -1)
		err(1, NULL);
	pt = psc_hashtbl_search(t, key);
	if (pt || create == 0)
		goto out;

	b = psc_hashbkt_get(t, key);
	pt = psc_hashbkt_search(t, b, key);
	if (pt == NULL) {
		pt = psc_alloc(sizeof(*pt) + sizeof(struct pfl_hashentry),
		    PAF_NOLOG);
		pt->plogpt_key = key;
		key = NULL;
		psc_hashent_init(t, pt);

		psc_hashbkt_add_item(t, b, pt);

		spinlock(&lock);
		pt->plogpt_idx = psc_dynarray_len(&_pfl_logpoints);
		psc_dynarray_add(&_pfl_logpoints, NULL);
		freelock(&lock);
	}
	psc_hashbkt_put(t, b);

 out:
	free(key);
	return (pt);
}
