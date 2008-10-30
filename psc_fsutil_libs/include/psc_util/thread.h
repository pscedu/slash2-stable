/* $Id$ */

#ifndef __PFL_THREAD_H__
#define __PFL_THREAD_H__

#include <pthread.h>
#include <stdarg.h>

#include "psc_types.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/hash.h"
#include "psc_util/lock.h"

struct psc_ctlmsg_stats;

#define PSC_THRNAME_MAX	24 /* must be 8-byte aligned */

struct psc_thread {
	int		   pscthr_run;
	int		   pscthr_flags;
	void		*(*pscthr_start)(void *);	/* thread main */
	pthread_t	   pscthr_pthread;
	u64		   pscthr_hashid;		/* lookup ID */
	size_t		   pscthr_id;			/* XXX go away */
	int		   pscthr_type;			/* app-specific type */
	char		   pscthr_name[PSC_THRNAME_MAX];
	int		  *pscthr_loglevels;
	struct hash_entry  pscthr_hentry;		/* entry in threadtable */
	psc_spinlock_t	   pscthr_lock;
	void		  *pscthr_private;		/* app-specific data */
};

#define PTF_PAUSED	(1 << 0)

void	pscthr_setpause(struct psc_thread *, int);
void	pscthr_sigusr1(int);
void	pscthr_sigusr2(int);
void	pscthr_destroy(struct psc_thread *);
void	pscthr_init(struct psc_thread *, int, void *(*)(void *),
		void *, const char *, ...);

extern struct dynarray	pscThreads;
extern psc_spinlock_t	pscThreadsLock;

#endif /* __PFL_THREAD_H__ */
