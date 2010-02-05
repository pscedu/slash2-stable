/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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
# define PSC_SUBSYS PSS_GEN
#endif

#include "psc_util/subsys.h"
#include "pfl/cdefs.h"

struct psclog_data {
	char	pld_hostshort[HOST_NAME_MAX];
	char	pld_hostname[HOST_NAME_MAX];
	char	pld_nothrname[20];
	int	pld_rank;
};

/* Log levels. */
#define PLL_FATAL	0	/* process/thread termination */
#define PLL_ERROR	1	/* recoverable failure */
#define PLL_WARN	2	/* something wrong, require attention */
#define PLL_NOTICE	3	/* something unusual, recommend attention */
#define PLL_NOTIFY	PLL_NOTICE
#define PLL_INFO	4	/* general information */
#define PLL_DEBUG	5	/* debug messages */
#define PLL_TRACE	6	/* flow */
#define PLL_MAX		(-1)	/* force log */
#define PNLOGLEVELS	7

/* Logging options. */
#define PLO_ERRNO	(1 << 0)	/* strerror(errno) */

#define _psclogck(ss, lvl, flg, fmt, ...)				\
	do {								\
		if (psc_log_getlevel(ss) >= (lvl))			\
			_psclog(__FILE__, __func__, __LINE__, (ss),	\
			    (lvl), (flg), (fmt), ## __VA_ARGS__);	\
	} while (0)

#define _psclogvck(ss, lvl, flg, fmt, ap)				\
	do {								\
		if (psc_log_getlevel(ss) >= (lvl))			\
			_psclogv(__FILE__, __func__, __LINE__, (ss),	\
			    (lvl), (flg), (fmt), (ap));			\
	} while (0)

#define _psclogft(ss, lvl, flg, fmt, ...)				\
	_psc_fatal(__FILE__, __func__, __LINE__, (ss),			\
	    (lvl), (flg), (fmt), ## __VA_ARGS__)

#define _psclogvft(ss, lvl, flg, fmt, ...)				\
	_psc_fatalv(__FILE__, __func__, __LINE__, (ss),			\
	    (lvl), (flg), (fmt), (ap))

#define psclog(file, fn, ln, ss, lvl, flg, fmt, ...)			\
	do {								\
		if ((lvl) == PLL_FATAL)					\
			_psc_fatal((file), (fn), (ln), (ss), (lvl),	\
			    (flg), (fmt), ## __VA_ARGS__ );		\
		else if (psc_log_getlevel(ss) >= (lvl))			\
			_psclog((file), (fn), (ln), (ss), (lvl),	\
			    (flg), (fmt), ## __VA_ARGS__);		\
	} while (0)

#define psclogv(file, fn, ln, ss, lvl, flg, fmt, ap)			\
	do {								\
		if ((lvl) == PLL_FATAL)					\
			_psc_fatalv((file), (fn), (ln), (ss),		\
			    (lvl), (flg), (fmt), (ap));			\
		else if (psc_log_getlevel(ss) >= (lvl))			\
			_psclogv((file), (fn), (ln), (ss),		\
			    (lvl), (flg), (fmt), (ap));			\
	} while (0)

/* Current/default/active subsystem. */
#define psc_fatal(fmt, ...)		_psclogft(PSC_SUBSYS, PLL_FATAL, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_fatalx(fmt, ...)		_psclogft(PSC_SUBSYS, PLL_FATAL, 0, (fmt), ## __VA_ARGS__)
#define psc_error(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_errorx(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psc_warn(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_warnx(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psc_notice(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_notify(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_dbg(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psc_info(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psc_trace(fmt, ...)		_psclogck(PSC_SUBSYS, PLL_TRACE, 0, (fmt), ## __VA_ARGS__)
#define psc_log(lvl, fmt, ...)		_psclogck(PSC_SUBSYS, (lvl), 0, (fmt), ## __VA_ARGS__)
#define psc_logx(lvl, fmt, ...)		_psclogck(PSC_SUBSYS, (lvl), PLO_ERRNO, (fmt), ## __VA_ARGS__)

/* Override/specify subsystem. */
#define psc_fatals(ss, fmt, ...)	_psclogft((ss), PLL_FATAL, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_fatalxs(ss, fmt, ...)	_psclogft((ss), PLL_FATAL, 0, (fmt), ## __VA_ARGS__)
#define psc_errors(ss, fmt, ...)	_psclogck((ss), PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_errorxs(ss, fmt, ...)	_psclogck((ss), PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psc_warns(ss, fmt, ...)		_psclogck((ss), PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_warnxs(ss, fmt, ...)	_psclogck((ss), PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psc_notices(ss, fmt, ...)	_psclogck((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_notifys(ss, fmt, ...)	_psclogck((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psc_dbgs(ss, fmt, ...)		_psclogck((ss), PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psc_infos(ss, fmt, ...)		_psclogck((ss), PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psc_traces(ss, fmt, ...)	_psclogck((ss), PLL_TRACE, 0, (fmt), ## __VA_ARGS__)
#define psc_logs(lvl, ss, fmt, ...)	_psclogck((ss), (lvl), 0, (fmt), ## __VA_ARGS__)
#define psc_logxs(lvl, ss, fmt, ...)	_psclogck((ss), (lvl), PLO_ERRNO, (fmt), ## __VA_ARGS__)

/* Variable-argument list versions. */
#define psc_fatalv(fmt, ap)		_psclogvft(PSC_SUBSYS, PLL_FATAL, PLO_ERRNO, (fmt), ap)
#define psc_fatalxv(fmt, ap)		_psclogvft(PSC_SUBSYS, PLL_FATAL, 0, (fmt), ap)
#define psc_errorv(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), ap)
#define psc_errorxv(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_ERROR, 0, (fmt), ap)
#define psc_warnv(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), ap)
#define psc_warnxv(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_WARN, 0, (fmt), ap)
#define psc_noticev(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ap)
#define psc_notifyv(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ap)
#define psc_dbgv(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ap)
#define psc_infov(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_INFO, 0, (fmt), ap)
#define psc_tracev(fmt, ap)		_psclogvck(PSC_SUBSYS, PLL_TRACE, 0, (fmt), ap)
#define psc_logv(lvl, fmt, ap)		_psclogvck(PSC_SUBSYS, (lvl), 0, (fmt), ap)
#define psc_logxv(lvl, fmt, ap)		_psclogvck(PSC_SUBSYS, (lvl), PLO_ERRNO, (fmt), ap)

/* Variable-argument list versions with subsystem overriding. */
#define psc_fatalsv(fmt, ap)		_psclogvft((ss), PLL_FATAL, PLO_ERRNO, (fmt), (ap))
#define psc_fatalxsv(fmt, ap)		_psclogvft((ss), PLL_FATAL, 0, (fmt), (ap))
#define psc_errorsv(fmt, ap)		_psclogvck((ss), PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psc_errorxsv(fmt, ap)		_psclogvck((ss), PLL_ERROR, 0, (fmt), (ap))
#define psc_warnsv(fmt, ap)		_psclogvck((ss), PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psc_warnxsv(fmt, ap)		_psclogvck((ss), PLL_WARN, 0, (fmt), (ap))
#define psc_noticesv(fmt, ap)		_psclogvck((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psc_notifysv(fmt, ap)		_psclogvck((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psc_dbgsv(fmt, ap)		_psclogvck((ss), PLL_DEBUG, 0, (fmt), (ap))
#define psc_infosv(fmt, ap)		_psclogvck((ss), PLL_INFO, 0, (fmt), (ap))
#define psc_tracesv(fmt, ap)		_psclogvck((ss), PLL_TRACE, 0, (fmt), (ap))
#define psc_logsv(lvl, ss, fmt, ap)	_psclogvck((ss), (lvl), 0, (fmt), (ap))
#define psc_logxsv(lvl, ss, fmt, ap)	_psclogvck((ss), (lvl), PLO_ERRNO, (fmt), (ap))

#define PSCLOG_LOCK()			flockfile(stderr)
#define PSCLOG_UNLOCK()			funlockfile(stderr)

#define PFL_ENTER()			psc_trace("enter")

#define PFL_RETURNX()						\
	do {							\
		psc_trace("exit");				\
		return;						\
	} while (0)

#define PFL_RETURN(rv)						\
	do {							\
		typeof(rv) _pfl_rv = (rv);			\
		psc_trace("exit rc=%ld %p", (long)_pfl_rv,	\
		    (void *)(unsigned long)_pfl_rv);		\
		return (_pfl_rv);				\
	} while (0)

#define PFL_RETURN_LIT(rv)					\
	do {							\
		psc_trace("exit rc=%ld", (long)(rv));		\
		return (rv);					\
	} while (0)

#define PFL_RETURN_STRLIT(str)					\
	do {							\
		psc_trace("exit rc='%s'", (str));		\
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

void	psc_log_init(void);
void	psc_log_setlevel(int, int);
int	psc_log_getlevel(int);
int	psc_log_getlevel_global(void);
int	psc_log_getlevel_ss(int);

struct psclog_data *
	psclog_getdata(void);

const char *
	psc_loglevel_getname(int);
int	psc_loglevel_getid(const char *);

void _psclogv(const char *, const char *, int, int, int, int, const char *,
    va_list);

void _psclog(const char *, const char *, int, int, int, int, const char *,
    ...)
    __attribute__((__format__(__printf__, 7, 8)))
    __attribute__((nonnull(7, 7)));

__dead void _psc_fatalv(const char *, const char *, int, int, int, int,
    const char *, va_list);

__dead void _psc_fatal(const char *, const char *, int, int, int, int,
    const char *, ...)
    __attribute__((__format__(__printf__, 7, 8)))
    __attribute__((nonnull(7, 7)));

extern const char	*psc_logfmt;
extern char		 psclog_eol[8];

#endif /* _PFL_LOG_H_ */
