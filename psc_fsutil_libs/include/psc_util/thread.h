/* $Id: psc_util/thread.h 2211 2007-11-13 08:13:47Z yanovich $ */

#ifndef _ZESTTHREAD_H_
#define _ZESTTHREAD_H_

#include "psc_util/atomic.h"
#include "psc_ds/listcache.c"
#include "psc_types.h"
#include "psc_ds/psc_ds/dynarray.h"

#define ZTHR_NAME_MAX	24 /* must be 8-byte aligned */

struct psc_thread {
	/* thread_cond_waitv implementation variables. */
	struct psc_thread  **pscthr_waitnext;
	struct psc_thread ***pscthr_waitprev;
	int                      pscthr_run;
	pid_t			 pscthr_pid;
	int			 pscthr_psig;
	void			*(*pscthr_start)(void *);	/* thread's main routine */
	pthread_t		 pscthr_pthread;
	u64			 pscthr_hashid;		/* pthread_self() lookup */
	size_t			 pscthr_id;
	int			 pscthr_type;
	int			 pscthr_rc;
	char			 pscthr_name[ZTHR_NAME_MAX];
	int			 pscthr_log_levels[ZNSUBSYS];
	psc_spinlock_t		 pscthr_lock;
	void                    *pscthr_private;
};

void	pscthr_init(struct psc_thread *, int, void *(*)(void *), int);

#define ZINIT_LOCK(l)	spinlock(l)
#define ZINIT_ULOCK(l)	freelock(l)

#define PSC_THREAD_INIT_BARRIER(l)		\
	do {					\
		ZINIT_LOCK(l);			\
		ZINIT_ULOCK(l);			\
	} while (0)

#endif /* _ZESTTHREAD_H_ */
