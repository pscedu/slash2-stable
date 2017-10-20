/* $Id$ */

/*	$OpenBSD: time.h,v 1.25 2007/05/09 17:42:19 deraadt Exp $	*/
/*	$NetBSD: time.h,v 1.18 1996/04/23 10:29:33 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)time.h	8.2 (Berkeley) 7/10/94
 */

#ifndef _PFL_TIME_H_
#define _PFL_TIME_H_

#include <sys/time.h>

#include <stdint.h>

#ifndef HAVE_CLOCK_GETTIME
# include "pfl/compat/clock_gettime.h"
#endif

#ifndef timespecclear
#define timespecclear(tsp)		((tsp)->tv_sec = (tsp)->tv_nsec = 0)
#endif

#ifndef timespecisset
#define timespecisset(tsp)		((tsp)->tv_sec || (tsp)->tv_nsec)
#endif

#ifndef timespeccmp
#define timespeccmp(tsp, usp, cmp)					\
	(((tsp)->tv_sec == (usp)->tv_sec) ?				\
	 ((tsp)->tv_nsec cmp (usp)->tv_nsec) :				\
	 ((tsp)->tv_sec cmp (usp)->tv_sec))
#endif

#ifndef timespecadd
#define timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (0)
#endif

#ifndef timespecsub
#define timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)
#endif

#define PFL_GETTIMEVAL(tv)						\
	do {								\
		if (gettimeofday((tv), NULL) == -1)			\
			psc_fatal("gettimeofday");			\
	} while (0)

#define _PFL_GETTIMESPEC(wh, ts)					\
	do {								\
		if (clock_gettime((wh), (ts)) == -1)			\
			psc_fatal("clock_gettime");			\
	} while (0)

#ifdef CLOCK_REALTIME_PRECISE
#  define PFL_CLOCK_REALTIME CLOCK_REALTIME_PRECISE
#else
#  define PFL_CLOCK_REALTIME CLOCK_REALTIME
#endif

#define PFL_GETTIMESPEC(ts)	_PFL_GETTIMESPEC(PFL_CLOCK_REALTIME, (ts))

#define PFL_GETTIMESPEC_MONO(ts) _PFL_GETTIMESPEC(CLOCK_MONOTONIC, (ts))

#ifndef HAVE_FUTIMENS
struct timespec;

int futimens(int, const struct timespec *);
#endif

#define PFL_CTIME_BUFSIZ		26

struct pfl_timespec {
	uint64_t			tv_sec;		/* XXX __time_t is long */
	uint64_t			tv_nsec;
};

#define PFLPRI_PTIMESPEC		"%"PRId64":%09"PRId64
#define PFLPRI_PTIMESPEC_ARGS(ts)	(ts)->tv_sec, (ts)->tv_nsec

#define PFL_GETPTIMESPEC(ts)						\
	do {								\
		struct timespec _ts;					\
									\
		PFL_GETTIMESPEC(&_ts);					\
		(ts)->tv_sec = _ts.tv_sec;				\
		(ts)->tv_nsec = _ts.tv_nsec;				\
	} while (0)

#endif /* _PFL_TIME_H_ */
