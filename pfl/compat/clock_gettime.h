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

#ifndef _PFL_COMPAT_CLOCK_GETTIME_H_
#define _PFL_COMPAT_CLOCK_GETTIME_H_

#include <time.h>

typedef enum {
	CLOCK_MONOTONIC,
	CLOCK_REALTIME
} clockid_t;

int clock_gettime(clockid_t, struct timespec *);

#endif /* _PFL_COMPAT_CLOCK_GETTIME_H_ */
