/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_THREAD_H_
#define _PFL_THREAD_H_

#include <sys/types.h>

#include <pthread.h>

#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/waitq.h"

#define PSC_THRNAME_MAX		32				/* must be 8-byte aligned */

struct psc_thread {
	struct psclist_head	  pscthr_lentry;		/* list management */
	psc_spinlock_t		  pscthr_lock;			/* for mutex */
	pthread_t		  pscthr_pthread;		/* pthread_self() */
	pid_t			  pscthr_thrid;			/* gettid(2) */
	int			  pscthr_uniqid;		/* transiency bookkeeping */

	void			(*pscthr_startf)(struct psc_thread *);	/* thread main */
	void			(*pscthr_dtor)(void *);		/* custom destructor */

	int			  pscthr_flags;			/* operational flags */
	int			  pscthr_type;			/* app-specific type */
	char			  pscthr_name[PSC_THRNAME_MAX];/* human readable name */
	int			 *pscthr_loglevels;		/* logging granularity */
	void			 *pscthr_private;		/* app-specific data */
	struct psc_waitq	  pscthr_waitq;			/* for init, at least */

	/* only used for thread initialization */
	int			  pscthr_memnid;		/* ID of memnode */
	size_t			  pscthr_privsiz;		/* size of app data */
};

/* internal operation flags */
#define PTF_PAUSED		(1 << 0)			/* thread is frozen */
#define PTF_RUN			(1 << 1)			/* thread should operate normally */
#define PTF_READY		(1 << 2)			/* thread can start (used during init) */
#define PTF_DEAD		(1 << 3)			/* thread will terminate now */

#define PSCTHR_LOCK(thr)	spinlock(&(thr)->pscthr_lock)
#define PSCTHR_ULOCK(thr)	freelock(&(thr)->pscthr_lock)
#define PSCTHR_RLOCK(thr)	reqlock(&(thr)->pscthr_lock)

#define PSCTHR_MKCAST(label, name, type)				\
static inline struct name *						\
label(struct psc_thread *pt)						\
{									\
	psc_assert(pt->pscthr_type == (type));				\
	return ((struct name *)pt->pscthr_private);			\
}

/*
 * pscthr_init - Initialize an application thread.
 */
#define pscthr_init(thrtype, flags, startf, dtor, privsiz, namefmt,	\
	    ...)							\
	_pscthr_init((thrtype), (flags), (startf), (dtor), (privsiz),	\
	    -1, (namefmt), ## __VA_ARGS__)

#define pscthr_gettid()		pscthr_get()->pscthr_thrid

const char *
	pscthr_getname(void);
int	pscthr_getuniqid(void);
int	pscthr_run(void);
void	pscthr_setdead(struct psc_thread *, int);
void	pscthr_setloglevel(int, int);
void	pscthr_setpause(struct psc_thread *, int);
void	pscthr_setready(struct psc_thread *);
void	pscthr_setrun(struct psc_thread *, int);

struct psc_thread *pscthr_get(void);
struct psc_thread *pscthr_get_canfail(void);
struct psc_thread *
	_pscthr_init(int, int, void (*)(struct psc_thread *), void (*)(void *),
	    size_t, int, const char *, ...);

extern struct psc_lockedlist	psc_threads;

#endif /* _PFL_THREAD_H_ */
