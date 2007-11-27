/* $Id: pscWaitq.h 1741 2007-09-26 16:21:31Z yanovich $ */

#ifndef HAVE_PSC_WAITQ_INC
#define HAVE_PSC_WAITQ_INC

#include <time.h>

#include "psc_ds/lock.h"
#include "psc_ds/list.h"

#if HAVE_LIBPTHREAD

# include <pthread.h>

struct psc_wait_queue {
	pthread_mutex_t		zwaitq_mut;
	pthread_cond_t		zwaitq_cond;
	pthread_condattr_t	zwaitq_cond_attr;
};

#else /* HAVE_LIBPTHREAD */

struct psc_wait_queue {
	psc_spinlock_t		zwaitq_mut;
	struct psclist_head	zwaitq_sleepers;
};

#endif /* HAVE_LIBPTHREAD */

typedef struct psc_wait_queue psc_waitq_t;

void psc_waitq_init(psc_waitq_t *q);
void psc_waitq_wait(psc_waitq_t *q, psc_spinlock_t *k);
int  psc_waitq_timedwait(psc_waitq_t *q, psc_spinlock_t *k,
			 const struct timespec *abstime);
void psc_waitq_wakeup(psc_waitq_t *q);
void psc_waitq_wakeall(psc_waitq_t *q);

/* Compatibility for LNET code. */
typedef psc_waitq_t		wait_queue_head_t;
#define init_waitqueue_head(q)	psc_waitq_init(q)
#define wake_up(q)		psc_waitq_wakeup(q)

#endif
