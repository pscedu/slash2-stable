/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2006-2016, Pittsburgh Supercomputing Center
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

#ifndef _PFL_LOG_H_
#define _PFL_LOG_H_

#include <sys/param.h>

#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>

#ifndef PSC_SUBSYS
# define PSC_SUBSYS PSS_DEF
#endif

#include "pfl/cdefs.h"
#include "pfl/dynarray.h"
#include "pfl/pfl.h"
#include "pfl/subsys.h"

struct psc_thread;

#define PSC_MAX_LOG_PER_FILE	1024*1024*32

/* Log levels. */
#define PLL_FATAL	0		/* process termination */
#define PLL_ERROR	1		/* recoverable failure */
#define PLL_WARN	2		/* something wrong, require attention */
#define PLL_NOTICE	3		/* something unusual, recommend attention */
#define PLL_INFO	4		/* general information */
#define PLL_DIAG	5		/* diagnostics */
#define PLL_DEBUG	6		/* debug messages */
#define PLL_VDEBUG	7		/* verbose debug messages */
#define PLL_TRACE	8		/* flow */
#define PNLOGLEVELS	9
#define PLL_MAX		(-1)		/* force log (for temporary debugging) */

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
		else if (psc_log_shouldlog((pci), (lvl)))		\
			_psclog((pci), (lvl), (flg), (fmt),		\
			    ## __VA_ARGS__);				\
	} while (0)

#define _psclogv_pci(pci, lvl, flg, fmt, ap)				\
	do {								\
		if ((lvl) == PLL_FATAL)					\
			_psc_fatalv((pci), (lvl), (flg), (fmt), (ap));	\
		else if (psc_log_shouldlog((pci), (lvl)))		\
			_psclogv((pci), (lvl), (flg), (fmt), (ap));	\
	} while (0)

#define _psclogk(ss, lvl, flg, fmt, ...)				\
	_psclog_pci(PFL_CALLERINFOSS(ss), (lvl), (flg), (fmt), ## __VA_ARGS__)

#define _psclogvk(ss, lvl, flg, fmt, ap)				\
	_psclogv_pci(PFL_CALLERINFOSS(ss), (lvl), (flg), (fmt), (ap))

/* Current/default/active subsystem. */
#define psc_fatal(fmt, ...)		_psclogk(0, PLL_FATAL, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psc_fatalx(fmt, ...)		_psclogk(0, PLL_FATAL, 0, (fmt), ## __VA_ARGS__)
#define psclog_max(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_MAX, 0, (fmt), ## __VA_ARGS__)
#define psclog_error(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclog_errorx(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psclog_warn(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclog_warnx(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psclog_notice(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psclog_info(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psclog_diag(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_DIAG, 0, (fmt), ## __VA_ARGS__)
#define psclog_debug(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclog_vdebug(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_VDEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclog_trace(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_TRACE, 0, (fmt), ## __VA_ARGS__)
#define psclog(lvl, fmt, ...)		_psclogk(PSC_SUBSYS, (lvl), 0, (fmt), ## __VA_ARGS__)

#define pflog_warn(fmt, ...)		_psclogk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)

/* Specify subsystem. */
#define pflogs_max(ss, fmt, ...)	_psclogk((ss), PLL_MAX, 0, (fmt), ## __VA_ARGS__)
#define psclogs_error(ss, fmt, ...)	_psclogk((ss), PLL_ERROR, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclogs_errorx(ss, fmt, ...)	_psclogk((ss), PLL_ERROR, 0, (fmt), ## __VA_ARGS__)
#define psclogs_warn(ss, fmt, ...)	_psclogk((ss), PLL_WARN, PLO_ERRNO, (fmt), ## __VA_ARGS__)
#define psclogs_warnx(ss, fmt, ...)	_psclogk((ss), PLL_WARN, 0, (fmt), ## __VA_ARGS__)
#define psclogs_notice(ss, fmt, ...)	_psclogk((ss), PLL_NOTICE, 0, (fmt), ## __VA_ARGS__)
#define psclogs_info(ss, fmt, ...)	_psclogk((ss), PLL_INFO, 0, (fmt), ## __VA_ARGS__)
#define psclogs_diag(ss, fmt, ...)	_psclogk((ss), PLL_DIAG, 0, (fmt), ## __VA_ARGS__)
#define psclogs_debug(ss, fmt, ...)	_psclogk((ss), PLL_DEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclogs_vdebug(ss, fmt, ...)	_psclogk((ss), PLL_VDEBUG, 0, (fmt), ## __VA_ARGS__)
#define psclogs_trace(ss, fmt, ...)	_psclogk((ss), PLL_TRACE, 0, (fmt), ## __VA_ARGS__)
#define psclogs(lvl, ss, fmt, ...)	_psclogk((ss), (lvl), 0, (fmt), ## __VA_ARGS__)

/* Variable-argument list versions. */
#define psc_fatalv(fmt, ap)		_psclogvk(0, PLL_FATAL, PLO_ERRNO, (fmt), (ap))
#define psc_fatalxv(fmt, ap)		_psclogvk(0, PLL_FATAL, 0, (fmt), (ap))
#define psclogv_error(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psclogv_errorx(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_ERROR, 0, (fmt), (ap))
#define psclogv_warn(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psclogv_warnx(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_WARN, 0, (fmt), (ap))
#define psclogv_notice(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_NOTICE, 0, (fmt), (ap))
#define psclogv_info(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_INFO, 0, (fmt), (ap))
#define psclogv_diag(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_DIAG, 0, (fmt), (ap))
#define psclogv_debug(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_DEBUG, 0, (fmt), (ap))
#define psclogv_vdebug(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_VDEBUG, 0, (fmt), (ap))
#define psclogv_trace(fmt, ap)		_psclogvk(PSC_SUBSYS, PLL_TRACE, 0, (fmt), (ap))
#define psclogv(lvl, fmt, ap)		_psclogvk(PSC_SUBSYS, (lvl), 0, (fmt), (ap))

/* Variable-argument list versions with subsystem overriding. */
#define psclogsv_error(fmt, ap)		_psclogvk((ss), PLL_ERROR, PLO_ERRNO, (fmt), (ap))
#define psclogsv_errorx(fmt, ap)	_psclogvk((ss), PLL_ERROR, 0, (fmt), (ap))
#define psclogsv_warn(fmt, ap)		_psclogvk((ss), PLL_WARN, PLO_ERRNO, (fmt), (ap))
#define psclogsv_warnx(fmt, ap)		_psclogvk((ss), PLL_WARN, 0, (fmt), (ap))
#define psclogsv_notice(fmt, ap)	_psclogvk((ss), PLL_NOTICE, 0, (fmt), (ap))
#define psclogsv_info(fmt, ap)		_psclogvk((ss), PLL_INFO, 0, (fmt), (ap))
#define psclogsv_diag(fmt, ap)		_psclogvk((ss), PLL_DIAG, 0, (fmt), (ap))
#define psclogsv_debug(fmt, ap)		_psclogvk((ss), PLL_DEBUG, 0, (fmt), (ap))
#define psclogsv_vdebug(fmt, ap)	_psclogvk((ss), PLL_VDEBUG, 0, (fmt), (ap))
#define psclogsv_trace(fmt, ap)		_psclogvk((ss), PLL_TRACE, 0, (fmt), (ap))
#define psclogsv(lvl, ss, fmt, ap)	_psclogvk((ss), (lvl), 0, (fmt), (ap))

/*
 * To compile some logging statements out, add the following line
 * to mk/local.mk:
 *
 *	DEFINES+=       -DPFLOG_FROM_LEVEL=PLL_INFO
 *
 * This would compile any logging statements higher than PLL_INFO out.
 */
#ifdef PSCLOG_LEVEL
#  warning "PSCLOG_LEVEL is deprecated; use PFLOG_FROM_LEVEL"
#  define PFLOG_FROM_LEVEL PSCLOG_LEVEL
#endif

#ifndef PFLOG_FROM_LEVEL
#  define PFLOG_FROM_LEVEL		PNLOGLEVELS
#endif

#if PFLOG_FROM_LEVEL < PLL_TRACE
#  undef psclog_trace
#  undef psclogs_trace
#  undef psclogv_trace
#  undef psclogsv_trace

#  define psclog_trace(fmt, ...)	do { } while (0)
#  define psclogs_trace(ss, fmt, ...)	do { } while (0)
#  define psclogv_trace(fmt, ap)	do { } while (0)
#  define psclogsv_trace(fmt, ap)	do { } while (0)
#endif

#if PFLOG_FROM_LEVEL < PLL_VDEBUG
#  undef psclog_vdebug
#  undef psclogs_vdebug
#  undef psclogv_vdebug
#  undef psclogsv_vdebug

#  define psclog_vdebug(fmt, ...)	do { } while (0)
#  define psclogs_vdebug(ss, fmt, ...)	do { } while (0)
#  define psclogv_vdebug(fmt, ap)	do { } while (0)
#  define psclogsv_vdebug(fmt, ap)	do { } while (0)
#endif

#if PFLOG_FROM_LEVEL < PLL_DEBUG
#  undef psclog_debug
#  undef psclogs_debug
#  undef psclogv_debug
#  undef psclogsv_debug

#  define psclog_debug(fmt, ...)	do { } while (0)
#  define psclogs_debug(ss, fmt, ...)	do { } while (0)
#  define psclogv_debug(fmt, ap)	do { } while (0)
#  define psclogsv_debug(fmt, ap)	do { } while (0)
#endif

#if PFLOG_FROM_LEVEL < PLL_DIAG
#  undef psclog_diag
#  undef psclogs_diag
#  undef psclogv_diag
#  undef psclogsv_diag

#  define psclog_diag(fmt, ...)		do { } while (0)
#  define psclogs_diag(ss, fmt, ...)	do { } while (0)
#  define psclogv_diag(fmt, ap)		do { } while (0)
#  define psclogsv_diag(fmt, ap)	do { } while (0)
#endif

struct pfl_logpoint {
	char			*plogpt_key;
	int			 plogpt_idx;
	int			_pad;
//	struct pfl_hashentry	 plogpt_hentry;
};

/* Determine whether a debug/logging operation should occur. */
#define psc_log_shouldlog(pci, lvl)					\
	_PFL_RVSTART {							\
		int _rc0 = 0;						\
									\
		/* check global logging level */			\
		if ((lvl) > PFLOG_FROM_LEVEL)				\
			;						\
									\
		/* check thread logging level */			\
		else if (psc_log_getlevel((pci)->pci_subsys) >= (lvl))	\
			_rc0 = 1;					\
									\
		/* check if specific logpoint exists */			\
		else if (psc_dynarray_len(&_pfl_logpoints)) {		\
			/* XXX NUMA */					\
			static int _pfl_logpointid = -1;		\
									\
			if (_pfl_logpointid == -1)			\
				_pfl_logpointid = _pfl_get_logpointid(	\
				    __FILE__, __LINE__, 1)->plogpt_idx;	\
									\
			if (psc_dynarray_getpos(&_pfl_logpoints,	\
			    _pfl_logpointid))				\
				_rc0 = 1;				\
		}							\
									\
		(_rc0);							\
	} _PFL_RVEND

#define PSCLOG_LOCK()			flockfile(stderr)
#define PSCLOG_UNLOCK()			funlockfile(stderr)

#define PFL_RETURNX()							\
	do {								\
		psclog_trace("exit %s", __func__);			\
		return;							\
	} while (0)

#define PFL_RETURN(rv)							\
	do {								\
		typeof(rv) _pfl_rv = (rv);				\
									\
		psclog_trace("exit %s rc=%ld %p", __func__,		\
		    (long)_pfl_rv,					\
		    (void *)(uintptr_t)_pfl_rv);			\
		return (_pfl_rv);					\
	} while (0)

#define PFL_RETURN_LIT(rv)						\
	do {								\
		psclog_trace("exit %s rc=%ld", __func__,		\
		    (long)(rv));					\
		return (rv);						\
	} while (0)

#define PFL_RETURN_STR(str)						\
	do {								\
		psclog_trace("exit %s rc='%s'", __func__,		\
		    (str));						\
		return (str);						\
	} while (0)

#define PFL_RETURNX_PCI()						\
	do {								\
		psclog_trace("exit %s", __func__);			\
		_PFL_END_PCI();						\
		return;							\
	} while (0)

#define PFL_RETURN_PCI(rv)						\
	do {								\
		typeof(rv) _pfl_rv = (rv);				\
									\
		psclog_trace("exit %s rc=%ld %p", __func__,		\
		    (long)_pfl_rv,					\
		    (void *)(uintptr_t)_pfl_rv);			\
		_PFL_END_PCI();						\
		return (_pfl_rv);					\
	} while (0)

#define PFL_RETURN_LIT_PCI(rv)						\
	do {								\
		psclog_trace("exit %s rc=%ld", __func__,		\
		    (long)(rv));					\
		_PFL_END_PCI();						\
		return (rv);						\
	} while (0)

#define PFL_RETURN_STR_PCI(str)						\
	do {								\
		psclog_trace("exit %s rc='%s'", __func__, (str));	\
		_PFL_END_PCI();						\
		return (str);						\
	} while (0)

#define PFL_GOTOERR(label, code)					\
	do {								\
		int _rcval = (code);					\
									\
		if (_rcval)						\
			psclog_debug("error: "#code ": %d", (_rcval));	\
		goto label;						\
	} while (0)

#define psc_assert(cond)						\
	do {								\
		if (!(cond))						\
			psc_fatalx("[assert] %s", #cond);		\
	} while (0)

#define psc_assert_perror(cond)						\
	do {								\
		if (!(cond))						\
			psc_fatal("[assert] %s", #cond);		\
	} while (0)

void	 psc_log_init(void);
int	 psc_log_setfn(const char *, const char *);
void	 psc_log_setlevel(int, int);

int	 psc_log_getlevel(int);
int	 psc_log_getlevel_global(void);
int	 psc_log_getlevel_ss(int);

extern const char	*(*pflog_get_fsctx_uprog)(struct psc_thread *);
extern pid_t		 (*pflog_get_fsctx_pid)(struct psc_thread *);
extern uid_t		 (*pflog_get_fsctx_uid)(struct psc_thread *);
extern const char	*(*pflog_get_peer_addr)(struct psc_thread *);

const char		  *psc_loglevel_getname(int);
int			   psc_loglevel_fromstr(const char *);

struct pfl_logpoint	 *_pfl_get_logpointid(const char *, int, int);

void _psclogv(const struct pfl_callerinfo *, int, int, const char *,
    va_list);

void _psclog(const struct pfl_callerinfo *, int, int, const char *, ...)
    __attribute__((__format__(__printf__, 4, 5)))
    __attribute__((nonnull(4, 4)));

__dead void _psc_fatalv(const struct pfl_callerinfo *, int, int,
    const char *, va_list);

__dead void _psc_fatal(const struct pfl_callerinfo *, int, int,
    const char *, ...)
    __attribute__((__format__(__printf__, 4, 5)))
    __attribute__((nonnull(4, 4)));

extern int			 psc_log_console;
extern int			 psc_logfmt_error;
extern const char		*psc_logfmt;
extern char			 psclog_eol[8];
extern char		 	 psc_hostshort[64];
extern char			 psc_hostname[64];

extern void			*psc_stack_ptrbuf[32];
extern char		 	 psc_stack_symbuf[256];

extern struct psc_dynarray	_pfl_logpoints;

#endif /* _PFL_LOG_H_ */
