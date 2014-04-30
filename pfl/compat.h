/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_COMPAT_H_
#define _PFL_COMPAT_H_

#include <sys/param.h>

#include <errno.h>

#define _PFLERR_START	500

#if defined(ELAST) && ELAST >= _PFLERR_START
#  error system error codes into application space, need to adjust and recompile
#endif

#ifndef ECOMM
#  define ECOMM		EPROTO
#endif

#ifndef HOST_NAME_MAX
#  define HOST_NAME_MAX	MAXHOSTNAMELEN
#endif

#ifdef HAVE_QSORT_R_THUNK
#  define pfl_qsort_r(base, nel, wid, cmpf, arg)			\
	qsort_r((base), (nel), (wid), (arg), (cmpf))
#else
#  define pfl_qsort_r(base, nel, wid, cmpf, arg)			\
	qsort_r((base), (nel), (wid), (cmpf), (arg))
#endif

#ifndef HAVE_QSORT_R
void qsort_r(void *, size_t, size_t,
    int (*)(const void *, const void *, void *), void *);
#endif

#endif /* _PFL_COMPAT_H_ */
