/* $Id$ */

#ifndef _PFL_WAITQ_H_
#define _PFL_WAITQ_H_

#include <time.h>

#include "psc_ds/list.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

#if HAVE_LIBPTHREAD

# include <pthread.h>

struct psc_waitq {
	pthread_mutex_t		wq_mut;
	pthread_cond_t		wq_cond;
	atomic_t		wq_nwaitors;
	struct timespec		wq_waitv;
};

#define PSC_WAITQ_INIT	{ PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP, \
			  PTHREAD_COND_INITIALIZER, ATOMIC_INIT(0), { 0, 0 } }

#else /* HAVE_LIBPTHREAD */

struct psc_waitq {
	atomic_t		wq_nwaitors;
};

#define PSC_WAITQ_INIT	{ ATOMIC_INIT(0) }

#endif

typedef struct psc_waitq psc_waitq_t;

/**
 * psc_waitq_wait - wait until resource managed by wq_cond is available.
 * @wq: wait queue.
 * @lk: optional lock to prevent race condition in waiting.
 */
#define psc_waitq_wait(wq, lk)		psc_waitq_waitrel((wq), (lk), NULL)

#define psc_waitq_waitrel_s(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), (n), 0L)
#define psc_waitq_waitrel_us(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), 0L, (n) * 1000L)
#define psc_waitq_waitrel_ms(wq, lk, n)	 _psc_waitq_waitrelv((wq), (lk), 0L, (n) * 1000L * 1000L)
#define psc_waitq_waitrel_tv(wq, lk, tv) _psc_waitq_waitrelv((wq), (lk), (tv)->tv_sec, (tv)->tv_usec * 1000L)

/**
 * psc_waitq_nwaitors - determine number of threads waiting on a waitq.
 * @wq: wait queue.
 */
#define psc_waitq_nwaitors(wq)		atomic_read(&(wq)->wq_nwaitors)

#define psc_waitq_timedwait(wq, lk, ts)	psc_waitq_waitabs((wq), (lk), (ts))

void	psc_waitq_init(struct psc_waitq *);
void	psc_waitq_wakeone(struct psc_waitq *);
void	psc_waitq_wakeall(struct psc_waitq *);
int	psc_waitq_waitrel(struct psc_waitq *, psc_spinlock_t *,
	    const struct timespec *);
int	_psc_waitq_waitrelv(struct psc_waitq *, psc_spinlock_t *,
	    long, long);
int	psc_waitq_waitabs(struct psc_waitq *, psc_spinlock_t *,
	    const struct timespec *);

#endif /* _PFL_WAITQ_H_ */
