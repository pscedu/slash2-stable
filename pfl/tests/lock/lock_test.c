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

#define TEST_LOCK_INCLUDE	"psc_util/lock.h"
#define TEST_LOCK_TYPE		psc_spinlock_t
#define TEST_LOCK_INITIALIZER	SPINLOCK_INIT
#define TEST_LOCK_ACQUIRE(lk)	spinlock(lk)
#define TEST_LOCK_RELEASE(lk)	freelock(lk)

#include "lock_template.c"
