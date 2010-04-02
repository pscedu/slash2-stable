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

#ifndef _PFL_PTHRUTIL_H_
#define _PFL_PTHRUTIL_H_

#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER
# include "pfl/compat/pthread_barrier.h"
#endif

#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
# define PSC_PTHREAD_RWLOCK_INITIALIZER PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
#else
# define PSC_PTHREAD_RWLOCK_INITIALIZER PTHREAD_RWLOCK_INITIALIZER
#endif

#ifdef PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
# define PSC_PTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_ERRORCHECK_INITIALIZER
#elif defined(PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP)
# define PSC_PTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_ERRORCHECK_INITIALIZER_NP
#else
# define PSC_PTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

void	psc_pthread_mutex_ensure_locked(pthread_mutex_t *);
void	psc_pthread_mutex_init(pthread_mutex_t *);
void	psc_pthread_mutex_lock(pthread_mutex_t *);
int	psc_pthread_mutex_reqlock(pthread_mutex_t *);
int	psc_pthread_mutex_trylock(pthread_mutex_t *);
void	psc_pthread_mutex_unlock(pthread_mutex_t *);
void	psc_pthread_mutex_ureqlock(pthread_mutex_t *, int);

#endif /* _PFL_PTHRUTIL_H_ */
