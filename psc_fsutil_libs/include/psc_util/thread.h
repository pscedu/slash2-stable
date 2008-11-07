/* $Id$ */

#ifndef __PFL_THREAD_H__
#define __PFL_THREAD_H__

#include <pthread.h>
#include <stdarg.h>

#include "psc_ds/dynarray.h"
#include "psc_util/lock.h"

struct psc_ctlmsg_stats;

#define PSC_THRNAME_MAX	16	/* must be 8-byte aligned */

struct psc_thread {
	int		   pscthr_run;			/* XXX merge with PTF_PAUSED */
	int		   pscthr_flags;
	void		*(*pscthr_start)(void *);	/* thread main */
	pthread_t	   pscthr_pthread;
	int		   pscthr_type;			/* app-specific type */
	char		   pscthr_name[PSC_THRNAME_MAX];
	int		  *pscthr_loglevels;
	psc_spinlock_t	   pscthr_lock;
	void		  *pscthr_private;		/* app-specific data */
	pid_t		   pscthr_thrid;		/* gettid(2) */
	void		 (*pscthr_dtor)(void *);
};

#define PTF_PAUSED	(1 << 0)
#define PTF_FREE	(1 << 1)

#define pscthr_init(thr, type, startf, priv, namefmt, ...)	\
	_pscthr_init((thr), (type), (startf), (priv), 0, NULL,	\
	    (namefmt), ## __VA_ARGS__)

void	pscthr_setpause(struct psc_thread *, int);
void	pscthr_sigusr1(int);
void	pscthr_sigusr2(int);
void	_pscthr_init(struct psc_thread *, int, void *(*)(void *), void *,
	    int, void (*)(void *), const char *, ...);
struct psc_thread *	pscthr_get(void);
struct psc_thread *	pscthr_get_canfail(void);

extern struct dynarray	pscThreads;
extern psc_spinlock_t	pscThreadsLock;

#endif /* __PFL_THREAD_H__ */
