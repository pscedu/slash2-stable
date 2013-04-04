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

#ifndef _PFL_CDEFS_H_
#define _PFL_CDEFS_H_

#ifdef HAVE_SYS_CDEFS_H
#  include <sys/cdefs.h>
#else
#  include <sys/sysmacros.h>
#endif

#include <stddef.h>

#ifndef nitems
#  define nitems(t)		((int)(sizeof(t) / sizeof(t[0])))
#endif

#ifndef offsetof
#  define offsetof(type, memb)	((size_t)&((type *)NULL)->memb)
#endif

#ifndef CMP
#  define CMP(a, b)		((a) < (b) ? -1 : ((a) > (b) ? 1 : 0))
#endif

#ifndef SWAP
#define SWAP(a, b, t)							\
	do {								\
		(t) = (a);						\
		(a) = (b);						\
		(b) = (t);						\
	} while (0)
#endif

#ifndef __GNUC_PREREQ__
# ifdef __GNUC__
#  define __GNUC_PREREQ__(maj, min)					\
	((__GNUC__ > (maj)) || (__GNUC__ == (maj) && __GNUC_MINOR__ >= (min)))
# else
#  define __GNUC_PREREQ__(maj, min) 0
# endif
#endif

/* no-op __attribute if old/non gcc */
#if !__GNUC_PREREQ__(2, 5)
# define __attribute__(x)
#endif

#undef __dead
#define __dead			__attribute__((__noreturn__))

#undef __packed
#define __packed		__attribute__((__packed__))

#undef __unusedx
#define __unusedx		__attribute__((__unused__))

#undef __weak
#define __weak			__attribute__((__weak__))

#undef __aligned
#define __aligned(n)		__attribute__((__aligned__(n)))

#ifdef HAVE_TLS
#  define __threadx		__thread
#else
#  define __threadx
#endif

/* For marking something as file-scoped without side effects of `static'. */
#define __static

#define ATTR_HASALL(c, a)	(((c) & (a)) == (a))
#define ATTR_HASANY(c, a)	((c) & (a))
#define ATTR_TEST(c, a)		((c) & (a))
#define ATTR_SET(c, a)		((void)((c) |= (a)))
#define ATTR_ISSET(c, a)	ATTR_TEST((c), (a))
#define ATTR_UNSET(c, a)	((void)((c) &= ~(a)))
#define ATTR_NONESET(c, a)	(((c) & (a)) == 0)
#define ATTR_NOTSET(c, a)	ATTR_NONESET((c), (a))
#define ATTR_RESET(c)		((c) = 0)

#define ATTR_XSET(c, a)							\
	do {								\
		if (ATTR_ISSET((c), (a)))				\
			abort();					\
		ATTR_SET((c), (a));					\
	} while (0)

#define ATTR_XUNSET(c, a)						\
	do {								\
		if (!ATTR_HASALL((c), (a)))				\
			abort();					\
		ATTR_UNSET((c), (a));					\
	} while (0)

/* if multiple of 2, use bitwise ops to simplify math */
#define PSC_ALIGN(sz, incr)	((incr) * (((sz) + ((incr) - 1)) / (incr)))

/* arthimetic on a generic pointer */
#define PSC_AGP(p, off)		((void *)((char *)(p) + (off)))

/* forced rvalue designators */
#define _PFL_RVSTART		(
#define _PFL_RVEND		)

#define PSC_MAKETRUE(expr)	_PFL_RVSTART { expr; 1; } _PFL_RVEND
#define PSC_MAKEFALSE(expr)	_PFL_RVSTART { expr; 0; } _PFL_RVEND

#define MAXVALMASK(nb)		((UINT64_C(1) << (nb)) - 1)

#endif /* _PFL_CDEFS_H_ */
