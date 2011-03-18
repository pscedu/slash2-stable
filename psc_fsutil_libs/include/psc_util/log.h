/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_LOG_H_
#define _PFL_LOG_H_

#include <sys/param.h>

#include <stdarg.h>

#ifndef PSC_SUBSYS
# define PSC_SUBSYS PSS_DEF
#endif

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/subsys.h"

struct psclog_data {
	char		pld_hostshort[HOST_NAME_MAX];
	char		pld_hostname[HOST_NAME_MAX];
	char		pld_nothrname[24];
	int		pld_rank;
	pid_t		pld_thrid;
	int		pld_flags;
};

#define PLDF_INLOG	(1 << 0)

/* Log levels. */
enum psclog_level {
/* 0 */	PLL_FATAL,			/* process termination */
/* 1 */	PLL_ERROR,			/* recoverable failure */
/* 2 */	PLL_WARN,			/* something wrong, require attention */
/* 3 */	PLL_NOTICE,			/* something unusual, recommend attention */
/* 4 */	PLL_INFO,			/* general information */
/* 5 */	PLL_DEBUG,			/* debug messages */
/* 6 */	PLL_TRACE,			/* flow */
/* 7 */	PNLOGLEVELS,
	PLL_MAX = -1			/* force log (temporary debug) */
};

#define PLL_NOTIFY	PLL_NOTICE

/* Logging options. */
#define PLO_ERRNO	(1 << 0)	/* strerror(errno) */

/*
 * The macros here avoid a call frame and argument evaluation by only
 * calling the logging routine if the log level is enabled.
 */
#define _psclog_pci(pci, lvl, flg, fmt, ...)				\
	do {								\
		if ((lvl) == PLL_FATAL)					\
			_psc_fatal((pci), (lvl), (flg), (fmt),		\
			    ## __VA_ARGS__ );				\
		else if (psc_log_getlevel(pci->pci_subsys) >= (lvl))	\
			_psclog((pci), (lvl), (flg), (fmt),		\
			    ## __VA_ARGS__);				\
	} while (0)

#define _psclogv_pci(pci, lvl, flg, fmt, ap)				\
	do {								\
		if ((lvl) == PLL_FATAL)					\
			_psc_fatalv((pci), (lvl), (flg), (fmt), (ap));	\
		else if (psc_log_getlevel(pci->pci_subsys) >= (lvl))	\
			_psclogv((pci), (lvl), (flg), (fmt), (ap));	\
	} while (0)

#define _psclogk(ss, lvl, flg, fmt, ...)				\
	_psclog_pci(PFL_CALLERINFOSS(ss), (lvl), (flg), (fmt), ## __VA_ARGS__)

#define _psclogvk(ss, lvl, flg, fmt, ap)				\
	_psclogv_pci(PFL_CALLERINFOSS(ss), (lvl), (flg), (fmt), (ap))

/* Current/default/active subsystem. */
#define psc_fatal(fmt, ...)		_psclogk(0, PLL_FATAL, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_fatalx(fmt, ...)		_psclogk(0, PLL_FATAL, 0, (fmt), ## __VA_ARGS__)
#define psc_error(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_errorx(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psc_warn(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_warnx(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psc_notice(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_notify(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_dbg(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psc_debug(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psc_info(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psc_trace(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_TRACE, 0, (fmt), ## __VA_ARGS__)

#define psclog_error(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclog_errorx(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psclog_warn(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclog_warnx(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psclog_notice(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psclog_notify(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psclog_dbg(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclog_debug(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclog_info(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psclog_trace(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_TRACE, 0, (fmt), ## __VA_ARGS__)
#define psclog_max(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_MAX, 0, (fmt), ## __VA_ARGS__)
#define psclog(lvl, fmt, ...)		_psclogk(PSC_SUBSYS, (lvl), 0, (fmt), ## __VA_ARGS__)
#define psc_log psclog

/* Override/specify subsystem. */
#define psc_errors(ss, fmt, ...)	_psclogk((ss), PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_errorxs(ss, fmt, ...)	_psclogk((ss), PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psc_warns(ss, fmt, ...)		_psclogk((ss), PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_warnxs(ss, fmt, ...)	_psclogk((ss), PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psc_notices(ss, fmt, ...)	_psclogk((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_notifys(ss, fmt, ...)	_psclogk((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_dbgs(ss, fmt, ...)		_psclogk((ss), PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psc_debugs(ss, fmt, ...)	_psclogk((ss), PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psc_infos(ss, fmt, ...)		_psclogk((ss), PLL_INFO, 0, (fmt), ## __VA_ARGS__)

#define psclogs_error(ss, fmt, ...)	_psclogk((ss), PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclogs_errorx(ss, fmt, ...)	_psclogk((ss), PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psclogs_warn(ss, fmt, ...)	_psclogk((ss), PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclogs_warnx(ss, fmt, ...)	_psclogk((ss), PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psclogs_notice(ss, fmt, ...)	_psclogk((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psclogs_notify(ss, fmt, ...)	_psclogk((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psclogs_dbg(ss, fmt, ...)	_psclogk((ss), PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclogs_debug(ss, fmt, ...)	_psclogk((ss), PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclogs_info(ss, fmt, ...)	_psclogk((ss), PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psclogs_trace(ss, fmt, ...)	_psclogk((ss), PLL_TRACE, 0, (fmt), ## __VA_ARGS__)
#define psclogs(lvl, ss, fmt, ...)	_psclogk((ss), (lvl), 0, (fmt), ## __VA_ARGS__)
#define psc_logs psclogs

/* Variable-argument list versions. */
#define psc_fatalv(fmt, ap)		_psclogvk(0, PLL_FATAL, PLO_ERRNO, (fmt), (ap))
#define psc_fatalxv(fmt, ap)		_psclogvk(0, PLL_FATAL, 0, (fmt), (ap))
#define psc_errorv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psc_errorxv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_ERROR, 0, (fmt), (ap))
#define psc_warnv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psc_warnxv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_WARN, 0, (fmt), (ap))
#define psc_noticev(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), (ap))
#define psc_notifyv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), (ap))
#define psc_dbgv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), (ap))
#define psc_debugv(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), (ap))
#define psc_infov(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_INFO, 0, (fmt), (ap))

#define psclogv_error(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psclogv_errorx(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_ERROR, 0, (fmt), (ap))
#define psclogv_warn(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psclogv_warnx(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_WARN, 0, (fmt), (ap))
#define psclogv_notice(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), (ap))
#define psclogv_notify(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), (ap))
#define psclogv_dbg(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), (ap))
#define psclogv_debug(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), (ap))
#define psclogv_info(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_INFO, 0, (fmt), (ap))
#define psclogv_trace(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_TRACE, 0, (fmt), (ap))
#define psclogv(lvl, fmt, ap)		_psclogvk(PSC_SUBSYS, (lvl), 0, (fmt), (ap))

/* Variable-argument list versions with subsystem overriding. */
#define psc_errorsv(fmt, ap)		_psclogvk((ss), PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psc_errorxsv(fmt, ap)		_psclogvk((ss), PLL_ERROR, 0, (fmt), (ap))
#define psc_warnsv(fmt, ap)		_psclogvk((ss), PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psc_warnxsv(fmt, ap)		_psclogvk((ss), PLL_WARN, 0, (fmt), (ap))
#define psc_noticesv(fmt, ap)		_psclogvk((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psc_notifysv(fmt, ap)		_psclogvk((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psc_debugsv(fmt, ap)		_psclogvk((ss), PLL_DEBUG, 0, (fmt), (ap))
#define psc_infosv(fmt, ap)		_psclogvk((ss), PLL_INFO, 0, (fmt), (ap))

#define psclogsv_error(fmt, ap)		_psclogvk((ss), PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psclogsv_errorx(fmt, ap)	_psclogvk((ss), PLL_ERROR, 0, (fmt), (ap))
#define psclogsv_warn(fmt, ap)		_psclogvk((ss), PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psclogsv_warnx(fmt, ap)		_psclogvk((ss), PLL_WARN, 0, (fmt), (ap))
#define psclogsv_notice(fmt, ap)	_psclogvk((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psclogsv_notify(fmt, ap)	_psclogvk((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psclogsv_debug(fmt, ap)		_psclogvk((ss), PLL_DEBUG, 0, (fmt), (ap))
#define psclogsv_info(fmt, ap)		_psclogvk((ss), PLL_INFO, 0, (fmt), (ap))
#define psclogsv_trace(fmt, ap)		_psclogvk((ss), PLL_TRACE, 0, (fmt), (ap))
#define psclogsv(lvl, ss, fmt, ap)	_psclogvk((ss), (lvl), 0, (fmt), (ap))

#define PSCLOG_LOCK()			flockfile(stderr)
#define PSCLOG_UNLOCK()			funlockfile(stderr)

#define PFL_RETURNX()						\
	do {							\
		psclog_trace("exit");				\
		return;						\
	} while (0)

#define PFL_RETURN(rv)						\
	do {							\
		typeof(rv) _pfl_rv = (rv);			\
								\
		psclog_trace("exit rc=%ld %p", (long)_pfl_rv,	\
		    (void *)(unsigned long)_pfl_rv);		\
		return (_pfl_rv);				\
	} while (0)

#define PFL_RETURN_LIT(rv)					\
	do {							\
		psclog_trace("exit rc=%ld", (long)(rv));	\
		return (rv);					\
	} while (0)

#define PFL_RETURN_STR(str)					\
	do {							\
		psclog_trace("exit rc='%s'", (str));		\
		return (str);					\
	} while (0)

#define psc_assert(cond)					\
	do {							\
		if (!(cond))					\
			psc_fatalx("[assert] %s", #cond);	\
	} while (0)

#define psc_assert_perror(cond)					\
	do {							\
		if (!(cond))					\
			psc_fatal("[assert] %s", #cond);	\
	} while (0)

void			 psc_log_init(void);
void			 psc_log_setlevel(int, enum psclog_level);

enum psclog_level	 psc_log_getlevel(int);
enum psclog_level	 psc_log_getlevel_global(void);
enum psclog_level	 psc_log_getlevel_ss(int);

struct psclog_data	*psclog_getdata(void);

const char		*psc_loglevel_getname(enum psclog_level);
enum psclog_level	 psc_loglevel_fromstr(const char *);

void _psclogv(const struct pfl_callerinfo *, enum psclog_level,
    int, const char *, va_list);

void _psclog(const struct pfl_callerinfo *, enum psclog_level,
    int, const char *, ...)
    __attribute__((__format__(__printf__, 4, 5)))
    __attribute__((nonnull(4, 4)));

__dead void _psc_fatalv(const struct pfl_callerinfo *,
    enum psclog_level, int, const char *, va_list);

__dead void _psc_fatal(const struct pfl_callerinfo *,
    enum psclog_level, int, const char *, ...)
    __attribute__((__format__(__printf__, 4, 5)))
    __attribute__((nonnull(4, 4)));

extern const char	*psc_logfmt;
extern char		 psclog_eol[8];

#endif /* _PFL_LOG_H_ */
