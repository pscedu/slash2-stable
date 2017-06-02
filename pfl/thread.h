/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_THREAD_H_
#define _PFL_THREAD_H_

#include <sys/types.h>

#include <pthread.h>

#include "pfl/list.h"
#include "pfl/lockedlist.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/waitq.h"

#define PSC_THRNAME_MAX		  32				/* must be 8-byte aligned */

extern psc_spinlock_t		  pthread_lock;

struct psc_thread {
	struct psclist_head	  pscthr_lentry;		/* list management */
	pthread_t		  pscthr_pthread;		/* pthread_self() */
	pid_t			  pscthr_thrid;			/* gettid(2) */
	int			  pscthr_uniqid;		/* transiency bookkeeping */
	int			  pscthr_flags;			/* operational flags */
	int			  pscthr_type;			/* app-specific type */
	char			 *pscthr_waitq;			/* debugging only */
	char			  pscthr_name[PSC_THRNAME_MAX]; /* human readable name */
	int			 *pscthr_loglevels;		/* logging granularity */
	struct pfl_callerinfo	 *pscthr_callerinfo;
	void			 *pscthr_private;		/* app-specific data */
};

struct psc_thread_init {
	struct psc_thread	 *pti_thread;
	void			(*pti_startf)(struct psc_thread *);	/* thread main */
	int			  pti_memnid;			/* ID of memnode */
};

/* internal operation flags */
#define PTF_PAUSED		(1 << 0)			/* thread is frozen */
#define PTF_RUN			(1 << 1)			/* thread should operate normally */
#define PTF_READY		(1 << 2)			/* thread can start (used during init) */
#define PTF_DEAD		(1 << 3)			/* thread will terminate now */
#define PTF_INIT		(1 << 4)			/* thread being inited now */
#define PTF_RPC_SVC_THREAD	(1 << 5)			/* thread is an RPC servicer */

#define PSCTHR_MKCAST(label, name, type)				\
static inline struct name *						\
label(struct psc_thread *pt)						\
{									\
	psc_assert(pt->pscthr_type == (type));				\
	return (pt->pscthr_private);					\
}

/*
 * pscthr_init - Initialize an application thread.
 */
#define pscthr_init(thrtype, startf, privsiz, namefmt, ...)		\
	_pscthr_init((thrtype), (startf), (privsiz), -1,		\
	    (namefmt), ## __VA_ARGS__)

#define pscthr_gettid()		pscthr_get()->pscthr_thrid

enum {
	PFL_THRT_CTL,			/* control processor */
	PFL_THRT_CTLAC,			/* control acceptor */
	PFL_THRT_FS,			/* fs worker */
	PFL_THRT_FSMGR,			/* pscfs manager */
	PFL_THRT_NBRPC,			/* non-blocking RPC reply handler */
	PFL_THRT_OPSTIMER,		/* opstats updater */
	PFL_THRT_USKLNDPL,		/* userland socket lustre net dev poll thr */
	PFL_THRT_WORKER,		/* generic worker */
	_PFL_NTHRT,
};

void	pscthrs_init(void);

int	pscthr_getuniqid(void);
void	pscthr_destroy(struct psc_thread *);
void	pscthr_killall(void);
int	pscthr_run(struct psc_thread *);
void	pscthr_setdead(struct psc_thread *, int);
void	pscthr_setpause(struct psc_thread *, int);
void	pscthr_setready(struct psc_thread *);
void	pscthr_setrun(struct psc_thread *, int);

struct psc_thread *pscthr_get(void);
struct psc_thread *pscthr_get_canfail(void);
struct psc_thread *
	_pscthr_init(int, void (*)(struct psc_thread *),
	    size_t, int, const char *, ...);

extern struct psc_lockedlist	psc_threads;

#endif /* _PFL_THREAD_H_ */
