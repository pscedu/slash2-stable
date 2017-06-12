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

#ifndef _PFL_PFL_H_
#define _PFL_PFL_H_

#include "pfl/compat.h"

#include "pfl/err.h"

#if HAVE_PTHREAD_YIELD
#  include <pthread.h>
#  define pscthr_yield()	pthread_yield()
#else
#  define pscthr_yield()	sched_yield()
#endif

#define PFL_PRFLAG(fl, val, seq)					\
	do {								\
		if (*(val) & (fl)) {					\
			pfl_print_flag(#fl, (seq));			\
			*(val) &= ~(fl);				\
		}							\
	} while (0)

#define PFL_CALLERINFOSS(ss)	(_pfl_callerinfo ? _pfl_callerinfo :	\
				    _pfl_callerinfo_get(__FILE__,	\
				    __func__, __LINE__, (ss)))
#define PFL_CALLERINFO()	PFL_CALLERINFOSS(PSC_SUBSYS)

struct pfl_callerinfo {
	const char	*pci_filename;
	const char	*pci_func;
	int		 pci_lineno;
	int		 pci_subsys;
};

void	 pfl_abort(void);
void	 pfl_atexit(void (*)(void));
void	 pfl_dump_fflags(int);
void	 pfl_dump_stack(void);
pid_t	 pfl_getsysthrid(void);
void	 pfl_init(void);
void	 pfl_print_flag(const char *, int *);
void	 pfl_setprocesstitle(char **, const char *, ...);


#ifdef HAVE_TLS
/*
 * We use a stack so the most immediate caller wins; otherwise when
 * calls are deeply nested, the shallow callers will get used by
 * PFL_CALLERINFOSS() and ignore deeper callers.
 */
# define _PFL_START_PCI(pci)						\
	do {								\
		_pfl_callerinfo = (pci);				\
		_pfl_callerinfo_lvl++;					\
	} while (0)
# define _PFL_END_PCI()							\
	do {								\
		if (--_pfl_callerinfo_lvl == 0)				\
			_pfl_callerinfo = NULL;				\
	} while (0)
# define _PFL_RETURN_PCI(rv)						\
	do {								\
		_PFL_END_PCI();						\
		return rv;						\
	} while (0)
#else
# define _PFL_START_PCI(pci)
# define _PFL_END_PCI()
# define _PFL_RETURN_PCI(rv)	return rv
#endif

extern
#ifdef HAVE_TLS
__thread
#endif
const struct pfl_callerinfo	*_pfl_callerinfo;

extern
#ifdef HAVE_TLS
__thread
#endif
int				 _pfl_callerinfo_lvl;

extern pid_t			  pfl_pid;

#ifdef HAVE_TLS
# define __callerinfo const struct pfl_callerinfo *pci
#else
# define __callerinfo __unusedx const struct pfl_callerinfo *pci
#endif

struct pfl_callerinfo * pscthr_get_callerinfo();

static __inline const struct pfl_callerinfo *
_pfl_callerinfo_get(const char *fn, const char *func, int lineno, int subsys)
{
	struct pfl_callerinfo *pci;

	pci = pscthr_get_callerinfo();

	pci->pci_filename = fn;
	pci->pci_func = func;
	pci->pci_lineno = lineno;
	pci->pci_subsys = subsys;

	return (pci);
}

#endif /* _PFL_PFL_H_ */
