/* $Id$ */

/*
 * Multiple-lock routines: for waiting on any number of
 * conditions to become available.
 */

#ifndef _PFL_MULTILOCK_H_
#define _PFL_MULTILOCK_H_

#include <sys/types.h>

#include <pthread.h>

#include "psc_ds/dynarray.h"

struct multilock;
struct vbitmap;

struct multilock_cond {
	pthread_mutex_t		 mlc_mutex;
	struct dynarray		 mlc_multilocks;	/* where cond is registered */
	struct multilock	*mlc_winner;		/* which multilock awoke first */
	const void		*mlc_data;		/* pointer to user data */
	int			 mlc_flags;
	char			 mlc_name[24];
};

#define PMLCF_WAKEALL		(1 << 0)		/* wake all multilocks, not just one */

#define MLCOND_INIT(data, name, flags) \
	{ PTHREAD_MUTEX_INITIALIZER, DYNARRAY_INIT, NULL, (data), (flags), (name) }

struct multilock {
	/*
	 * Multilocks have mutexes not because threads may
	 * "share" them, explicitly, but because threads may
	 * modify some fields implicitly (e.g. on wakeups and
	 * condition removals).
	 */
	pthread_mutex_t		  ml_mutex;
	pthread_t		  ml_owner;		/* who woke us */
	pthread_cond_t		  ml_cond;		/* master condition */
	struct multilock_cond	 *ml_waker;		/* which mlcond woke us */
	struct dynarray		  ml_conds;		/* registered conditions */
	struct vbitmap		 *ml_mask;		/* which conds can wake us */
	int			  ml_flags;
	char			  ml_name[24];
};

#define PMLF_CRITSECT		(1 << 0)		/* inside critical section */

int	multilock_addcond(struct multilock *, struct multilock_cond *, int);
void	multilock_enter_critsect(struct multilock *);
void	multilock_init(struct multilock *, const char *, ...);
void	multilock_leave_critsect(struct multilock *);
void	multilock_mask_cond(struct multilock *, const struct multilock_cond *, int);
void	multilock_reset(struct multilock *);
int	multilock_wait(struct multilock *, void *, int);
void	multilock_free(struct multilock *);

void	multilock_cond_init(struct multilock_cond *, const void *, int, const char *, ...);
size_t	multilock_cond_nwaitors(struct multilock_cond *);
void	multilock_cond_wakeup(struct multilock_cond *);
void	multilock_cond_destroy(struct multilock_cond *);

#endif /* _PFL_MULTILOCK_H_ */
