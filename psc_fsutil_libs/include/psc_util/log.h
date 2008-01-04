/* $Id$ */

#include <stdarg.h>

#ifndef PSC_SUBSYS
# define PSC_SUBSYS PSS_OTHER
#endif

#include "psc_util/subsys.h"
#include "psc_util/cdefs.h"

/* Log levels. */
#define PLL_FATAL	0 /* process/thread termination */
#define PLL_ERROR	1 /* recoverable failure */
#define PLL_WARN	2 /* something wrong, require attention */
#define PLL_NOTICE	3 /* something unusual, recommend attention */
#define PLL_INFO	4 /* general information */
#define PLL_DEBUG	5 /* debug messages */
#define PLL_TRACE	6 /* flow */
#define PNLOGLEVELS	7

/* Logging options. */
#define PLO_ERRNO	(1<<0)	/* strerror(errno) */

#define _FFL		__FILE__, __func__, __LINE__

/* Default subsystem. */
#define psc_fatal(fmt, ...)	_psc_fatal(_FFL, PSC_SUBSYS, PLL_FATAL, PLO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_fatalx(fmt, ...)	_psc_fatal(_FFL, PSC_SUBSYS, PLL_FATAL, 0, fmt, ## __VA_ARGS__)
#define psc_error(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_errorx(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_ERROR, 0, fmt, ## __VA_ARGS__)
#define psc_warn(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_WARN, PLO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_warnx(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_WARN, 0, fmt, ## __VA_ARGS__)
#define psc_notice(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_notify(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_dbg(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_DEBUG, 0, fmt, ## __VA_ARGS__)
#define psc_info(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_INFO, 0, fmt, ## __VA_ARGS__)
#define psc_trace(fmt, ...)	_psclog(_FFL, PSC_SUBSYS, PLL_TRACE, 0, fmt, ## __VA_ARGS__)
#define psc_log(lvl, fmt, ...)	_psclog(_FFL, PSC_SUBSYS, lvl, 0, fmt, ## __VA_ARGS__)

/* Override subsystem. */
#define psc_fatals(ss, fmt, ...)	_psc_fatal(_FFL, ss, PLL_FATAL, PLO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_fatalxs(ss, fmt, ...)	_psc_fatal(_FFL, ss, PLL_FATAL, 0, fmt, ## __VA_ARGS__)
#define psc_errors(ss, fmt, ...)	_psclog(_FFL, ss, PLL_ERROR, PLO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_errorxs(ss, fmt, ...)	_psclog(_FFL, ss, PLL_ERROR, 0, fmt, ## __VA_ARGS__)
#define psc_warns(ss, fmt, ...)		_psclog(_FFL, ss, PLL_WARN, PLO_ERRNO, fmt, ## __VA_ARGS__)
#define psc_warnxs(ss, fmt, ...)	_psclog(_FFL, ss, PLL_WARN, 0, fmt, ## __VA_ARGS__)
#define psc_notices(ss, fmt, ...)	_psclog(_FFL, ss, PLL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_notifys(ss, fmt, ...)	_psclog(_FFL, ss, PLL_NOTICE, 0, fmt, ## __VA_ARGS__)
#define psc_dbgs(ss, fmt, ...)		_psclog(_FFL, ss, PLL_DEBUG, 0, fmt, ## __VA_ARGS__)
#define psc_infos(ss, fmt, ...)		_psclog(_FFL, ss, PLL_INFO, 0, fmt, ## __VA_ARGS__)
#define psc_traces(ss, fmt, ...)	_psclog(_FFL, ss, PLL_TRACE, 0, fmt, ## __VA_ARGS__)
#define psc_logs(lvl, ss, fmt, ...)	_psclog(_FFL, ss, lvl, 0, fmt, ## __VA_ARGS__)

/* Variable-argument list versions. */
#define psc_fatalv(fmt, ap)	psc_fatalv(_FFL, PSC_SUBSYS, PLL_FATAL, PLO_ERRNO, fmt, ap)
#define psc_fatalxv(fmt, ap)	psc_fatalv(_FFL, PSC_SUBSYS, PLL_FATAL, 0, fmt, ap)
#define psc_errorv(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, fmt, ap)
#define psc_errorxv(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_ERROR, 0, fmt, ap)
#define psc_warnv(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_WARN, PLO_ERRNO, fmt, ap)
#define psc_warnxv(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_WARN, 0, fmt, ap)
#define psc_noticev(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_NOTICE, 0, fmt, ap)
#define psc_notifyv(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_NOTICE, 0, fmt, ap)
#define psc_dbgv(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_DEBUG, 0, fmt, ap)
#define psc_infov(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_INFO, 0, fmt, ap)
#define psc_tracev(fmt, ap)	psclogv(_FFL, PSC_SUBSYS, PLL_TRACE, 0, fmt, ap)

#define ENTRY_MARKER psc_trace("entry_marker")
#define EXIT_MARKER  psc_trace("exit_marker")

#define RETURN_MARKER(v)					\
	do {							\
		psc_trace("exit_marker");			\
		return v;					\
	} while (0)

int psc_setloglevel(int);
int psc_getloglevel(void);
int psclog_id(const char *);
const char *psclog_name(int);

void psclogv(const char *, const char *, int, int, int, int, const char *,
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
