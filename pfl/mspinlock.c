/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <pthread.h>
#include <stdint.h>

#include "pfl/cdefs.h"
#include "pfl/vbitmap.h"
#include "psc_util/lock.h"
#include "psc_util/mspinlock.h"

struct psc_vbitmap	 _psc_mspin_unthridmap = VBITMAP_INIT_AUTO;
psc_spinlock_t		 _psc_mspin_unthridmap_lock = SPINLOCK_INIT;
