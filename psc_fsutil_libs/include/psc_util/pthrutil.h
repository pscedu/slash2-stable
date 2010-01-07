/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_PTHRUTIL_H_
#define _PFL_PTHRUTIL_H_

#include <pthread.h>

void	psc_pthread_mutex_init(pthread_mutex_t *);
void	psc_pthread_mutex_lock(pthread_mutex_t *);
void	psc_pthread_mutex_unlock(pthread_mutex_t *);
int	psc_pthread_mutex_reqlock(pthread_mutex_t *);
void	psc_pthread_mutex_ureqlock(pthread_mutex_t *, int);

#define psc_pthread_mutex_ensure_locked(mutex)				\
	psc_assert(pthread_mutex_trylock(mutex) == EDEADLK)

#endif /* _PFL_PTHRUTIL_H_ */
