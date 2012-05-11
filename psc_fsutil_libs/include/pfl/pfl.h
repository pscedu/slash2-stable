/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_PFL_H_
#define _PFL_PFL_H_

#include "pfl/compat.h"

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

void	 psc_enter_debugger(const char *);

void	 pfl_dump_fflags(int);
void	 pfl_dump_stack(void);
pid_t	 pfl_getsysthrid(void);
void	 pfl_init(void);
void	 pfl_print_flag(const char *, int *);
void	 pfl_setprocesstitle(char **, const char *, ...);
void	*pfl_tls_get(int, size_t);

#define PFL_TLSIDX_LOGDATA	0
#define PFL_TLSIDX_CALLERINFO	1
#define PFL_TLSIDX_MEMNODE	2
#define PFL_TLSIDX_LOGDATEBUF	3
#define PFL_TLSIDX_NIDBUF	4
#define PFL_TLSIDX_IDBUF	5
#define PFL_TLSIDX_LASTRESERVED	6
#define PFL_TLSIDX_MAX		8

#ifdef HAVE_TLS
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

#ifdef HAVE_TLS
# define __callerinfo const struct pfl_callerinfo *pci
#else
# define __callerinfo __unusedx const struct pfl_callerinfo *pci
#endif

static __inline const struct pfl_callerinfo *
_pfl_callerinfo_get(const char *fn, const char *func, int lineno,
    int subsys)
{
	struct pfl_callerinfo *pci;

	pci = pfl_tls_get(PFL_TLSIDX_CALLERINFO, sizeof(*pci));
	pci->pci_filename = fn;
	pci->pci_func = func;
	pci->pci_lineno = lineno;
	pci->pci_subsys = subsys;
	return (pci);
}

#endif /* _PFL_PFL_H_ */
