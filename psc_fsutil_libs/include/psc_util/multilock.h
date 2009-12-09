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

struct psc_multilock;
struct vbitmap;

struct psc_multilock_cond {
	pthread_mutex_t			 mlc_mutex;
	struct dynarray			 mlc_multilocks;/* where cond is registered */
	struct psc_multilock		*mlc_winner;	/* which multilock awoke first */
	const void			*mlc_data;	/* pointer to user data */
	int				 mlc_flags;
	char				 mlc_name[48];	/* should 8-byte boundary */
};

#define PMLCF_WAKEALL			(1 << 0)	/* wake all multilocks, not just one */

#define MLCOND_INIT(data, name, flags) \
	{ PTHREAD_MUTEX_INITIALIZER, DYNARRAY_INIT, NULL, (data), (flags), (name) }

struct psc_multilock {
	/*
	 * Multilocks have mutexes not because threads may
	 * "share" them, explicitly, but because threads may
	 * modify some fields implicitly (e.g. on wakeups and
	 * condition removals).
	 */
	pthread_mutex_t			 ml_mutex;
	pthread_t			 ml_owner;	/* who woke us */
	pthread_cond_t			 ml_cond;	/* master condition */
	struct psc_multilock_cond	*ml_waker;	/* which mlcond woke us */
	struct dynarray			 ml_conds;	/* registered conditions */
	struct vbitmap			*ml_mask;	/* which conds can wake us */
	int				 ml_flags;
	char				 ml_name[32];	/* should be 8-byte boundary */
};

#define PMLF_CRITSECT			(1 << 0)	/* inside critical section */

int	psc_multilock_addcond(struct psc_multilock *, struct psc_multilock_cond *, int);
void	psc_multilock_enter_critsect(struct psc_multilock *);
void	psc_multilock_free(struct psc_multilock *);
int	psc_multilock_hascond(struct psc_multilock *, struct psc_multilock_cond *);
void	psc_multilock_init(struct psc_multilock *, const char *, ...);
void	psc_multilock_leave_critsect(struct psc_multilock *);
void	psc_multilock_mask_cond(struct psc_multilock *, const struct psc_multilock_cond *, int);
void	psc_multilock_reset(struct psc_multilock *);
int	psc_multilock_wait(struct psc_multilock *, void *, int);
void	psc_multilock_prconds(struct psc_multilock *);

#define psc_multilock_wait_us(ml, p, tm)	psc_multilock_wait((ml), (p), (tm))
#define psc_multilock_wait_ms(ml, p, tm)	psc_multilock_wait((ml), (p), (tm) * 1000)
#define psc_multilock_wait_s(ml, p, tm)		psc_multilock_wait((ml), (p), (tm) * 1000 * 1000)

void	psc_multilock_cond_destroy(struct psc_multilock_cond *);
void	psc_multilock_cond_init(struct psc_multilock_cond *, const void *, int, const char *, ...);
size_t	psc_multilock_cond_nwaitors(struct psc_multilock_cond *);
void	psc_multilock_cond_wait(struct psc_multilock_cond *, pthread_mutex_t *);
void	psc_multilock_cond_wakeup(struct psc_multilock_cond *);

#endif /* _PFL_MULTILOCK_H_ */
