/* $Id: psc_util/log.h 1924 2007-10-19 16:04:41Z yanovich $ */

#include <stdarg.h>

#ifndef SUBSYS
# include "psc_util/subsys.h"
# define SUBSYS ZS_OTHER
# define ZSUBSYS SUBSYS
#endif

#include "psc_util/cdefs.h"

/* log levels. */
#define LL_FATAL   0 /* process/thread termination */
#define LL_ERROR   1 /* recoverable failure */
#define LL_WARN	   2 /* something wrong, require attention */
#define LL_NOTICE  3 /* something unusual, recommend attention */
#define LL_INFO	   4 /* general information */
#define LL_DEBUG   5 /* debug messages */
#define LL_TRACE   6 /* flow */
#define NLOGLEVELS 7

/* logging options. */
#define LO_ERRNO	(1<<0)	/* strerror(errno) */

#define _FFL		__FILE__, __func__, __LINE__

#define psc_fatal(fmt, ...)		_psc_fatal(_FFL, SUBSYS, LL_FATAL, LO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_fatals(subsys, fmt, ...)	_psc_fatal(_FFL,  subsys, LL_FATAL, LO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_fatalv(fmt, ap)		vpsc_fatal(_FFL, SUBSYS, LL_FATAL, LO_ERRNO, fmt, ap)

#define psc_fatalx(fmt, ...)		_psc_fatal(_FFL, SUBSYS, LL_FATAL, 0, fmt, ## __VA_ARGS__)
#define psc_fatalxs(subsys, fmt, ...)	_psc_fatal(_FFL,  subsys, LL_FATAL, 0, fmt, ## __VA_ARGS__)
#define psc_fatalxv(fmt, ap)		vpsc_fatal(_FFL, SUBSYS, LL_FATAL, 0, fmt, ap)

#define psc_error(fmt, ...)		_psclog(_FFL, SUBSYS, LL_ERROR, LO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_errors(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_ERROR, LO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_errorv(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_ERROR, LO_ERRNO, fmt, ap)

#define psc_errorx(fmt, ...)		_psclog(_FFL, SUBSYS, LL_ERROR, 0, fmt, ## __VA_ARGS__)
#define psc_errorxs(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_ERROR, 0, fmt, ## __VA_ARGS__)
#define psc_errorxv(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_ERROR, 0, fmt, ap)

#define psc_warn(fmt, ...)		_psclog(_FFL, SUBSYS, LL_WARN, LO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_warns(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_WARN, LO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_warnv(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_WARN, LO_ERRNO, fmt, ap)

#define psc_warnx(fmt, ...)		_psclog(_FFL, SUBSYS, LL_WARN, 0, fmt, ## __VA_ARGS__)
#define psc_warnxs(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_WARN, 0, fmt, ## __VA_ARGS__)
#define psc_warnxv(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_WARN, 0, fmt, ap)

#define psc_notice(fmt, ...)		_psclog(_FFL, SUBSYS, LL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_notices(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_noticev(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_NOTICE, 0, fmt, ap)

#define psc_notify(fmt, ...)		_psclog(_FFL, SUBSYS, LL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_notifys(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_notifyv(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_NOTICE, 0, fmt, ap)

#define psc_dbg(fmt, ...)		_psclog(_FFL, SUBSYS, LL_DEBUG, 0, fmt, ## __VA_ARGS__)
#define psc_dbgs(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_DEBUG, 0, fmt, ## __VA_ARGS__)
#define psc_dbgv(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_DEBUG, 0, fmt, ap)

#define psc_info(fmt, ...)		_psclog(_FFL, SUBSYS, LL_INFO, 0, fmt, ## __VA_ARGS__)
#define psc_infos(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_INFO, 0, fmt, ## __VA_ARGS__)
#define psc_infov(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_INFO, 0, fmt, ap)

#define psc_trace(fmt, ...)		_psclog(_FFL, SUBSYS, LL_TRACE, 0, fmt, ## __VA_ARGS__)
#define psc_traces(subsys, fmt, ...)	_psclog(_FFL,  subsys, LL_TRACE, 0, fmt, ## __VA_ARGS__)
#define psc_tracev(fmt, ap)		vpsclog(_FFL, SUBSYS, LL_TRACE, 0, fmt, ap)

#define psc_logs(level, subsys, fmt, ...) _psclog(_FFL,  subsys, level, 0, fmt, ## __VA_ARGS__)
#define psc_log(level, fmt, ...)		_psclog(_FFL, SUBSYS, level, 0, fmt, ## __VA_ARGS__)

#define ENTRY_MARKER psctrace("entry_marker")
#define EXIT_MARKER  psctrace("exit_marker")

#define RETURN_MARKER(v)					\
	do {							\
		psctrace("exit_marker");			\
		return v;					\
	} while (0)

int psc_setloglevel(int);
int psc_getloglevel(void);
int psclog_id(const char *);
const char *psclog_name(int);

void vpsclog(const char *, const char *, int, int, int, int, const char *,
    va_list);

void _psclog(const char *, const char *, int, int, int, int, const char *,
    ...)
    __attribute__((__format__(__printf__, 7, 8)))
    __attribute__((nonnull(7, 7)));

__dead void _psc_fatal(const char *, const char *, int, int, int, int,
    const char *, ...)
    __attribute__((__format__(__printf__, 7, 8)))
    __attribute__((nonnull(7, 7)));

#ifdef CDEBUG
# undef CDEBUG
# define CDEBUG(mask, format, ...)				\
	do {							\
		switch (mask) {					\
		case D_ERROR:					\
		case D_NETERROR:				\
			psc_errorx(format, ## __VA_ARGS__);	\
			break;					\
		case D_WARNING:					\
			psc_warnx(format, ## __VA_ARGS__);	\
			break;					\
		case D_NET:					\
		case D_INFO:					\
		case D_CONFIG:					\
			psc_info(format, ## __VA_ARGS__);	\
			break;					\
		case D_RPCTRACE:				\
		case D_TRACE:					\
			psc_trace(format, ## __VA_ARGS__);	\
			break;					\
		default:					\
			psc_warnx("Unknown lustre mask %d", mask); \
			psc_warnx(format, ## __VA_ARGS__);	\
			break;					\
		}						\
	} while (0)
#endif
