/* $Id$ */

#ifndef __PFL_THREAD_H__
#define __PFL_THREAD_H__

#include <pthread.h>
#include <stdarg.h>

#include "psc_ds/dynarray.h"
#include "psc_ds/list.h"
#include "psc_util/lock.h"

#define PSC_THRNAME_MAX	16	/* must be 8-byte aligned */

struct psc_thread {
	struct psclist_head	   pscthr_lentry;
	psc_spinlock_t		   pscthr_lock;
	pthread_t		   pscthr_pthread;		/* pthread_self() */
	pid_t			   pscthr_thrid;		/* gettid(2) */

	void			*(*pscthr_start)(void *);	/* thread main */
	void			 (*pscthr_dtor)(void *);	/* custom destructor */

	int			   pscthr_flags;		/* operational flags */
	int			   pscthr_type;			/* app-specific type */
	char			   pscthr_name[PSC_THRNAME_MAX];
	int			  *pscthr_loglevels;		/* logging granularity */
	void			  *pscthr_private;		/* app-specific data */
	void			  *pscthr_memnode;		/* which memnode we're on */
};

#define PTF_PAUSED	(1 << 0)	/* thread is frozen */
#define PTF_FREE	(1 << 1)	/* thr mem should be free(3)'d on exit */
#define PTF_RUN		(1 << 2)	/* thread should operate */

#define PSCTHR_MKCAST(label, name, type)		\
static inline struct name *				\
label(struct psc_thread *pt)				\
{							\
	psc_assert(pt->pscthr_type == (type));		\
	return ((struct name *)pt->pscthr_private);	\
}

#define pscthr_init(thr, type, startf, priv, namefmt, ...)	\
	_pscthr_init((thr), (type), (startf), (priv), 0, NULL,	\
	    (namefmt), ## __VA_ARGS__)

void	pscthr_setpause(struct psc_thread *, int);
void	pscthr_sigusr1(int);
void	pscthr_sigusr2(int);
void	_pscthr_init(struct psc_thread *, int, void *(*)(void *), void *,
	    int, void (*)(void *), const char *, ...);
struct psc_thread *pscthr_get(void);
struct psc_thread *pscthr_get_canfail(void);

extern struct psc_lockedlist psc_threads;

#endif /* __PFL_THREAD_H__ */
