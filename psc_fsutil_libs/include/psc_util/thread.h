/* $Id$ */

#ifndef _PFL_THREAD_H_
#define _PFL_THREAD_H_

#include <pthread.h>
#include <stdarg.h>

#include "psc_types.h"
#include "psc_ds/dynarray.h"
#include "psc_util/lock.h"
#include "psc_util/slash_appthread.h"

#define PSC_THRNAME_MAX	24 /* must be 8-byte aligned */

struct psc_thread {
	int		   pscthr_run;
	void		*(*pscthr_start)(void *);	/* thread main */
	pthread_t	   pscthr_pthread;
	u64		   pscthr_hashid;		/* lookup ID */
	size_t		   pscthr_id;
	int		   pscthr_type;			/* app-specific type */
	char		   pscthr_name[PSC_THRNAME_MAX];
	int		  *pscthr_loglevels;
	psc_spinlock_t	   pscthr_lock;
	void		  *pscthr_private;		/* app-specific data */
};

void	pscthr_init(struct psc_thread *, int, void *(*)(void *), const char *, ...);

extern struct dynarray pscThreads;

#endif /* _PFL_THREAD_H_ */
