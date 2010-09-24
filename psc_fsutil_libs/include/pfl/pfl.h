/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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
		if ((val) & (fl)) {					\
			pfl_print_flag(#fl, (seq));			\
			(val) &= ~(fl);					\
		}							\
	} while (0)

void psc_enter_debugger(const char *);

void pfl_dump_fflags(int);
void pfl_init(void);
void pfl_print_flag(const char *, int *);
void pfl_setprocesstitle(char **, const char *, ...);

#endif /* _PFL_PFL_H_ */
