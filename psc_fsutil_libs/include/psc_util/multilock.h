/* $Id$ */

/*
 * Multiwait: for waiting on any of a number of conditions to become
 * available.
 */

#ifndef _PFL_MULTIWAIT_H_
#define _PFL_MULTIWAIT_H_

#include <sys/types.h>

#include <pthread.h>

#include "psc_ds/dynarray.h"

struct psc_multiwait;
struct psc_vbitmap;

struct psc_multiwaitcond {
	pthread_mutex_t			 mwc_mutex;
	pthread_cond_t			 mwc_cond;	/* for single waiters */
	struct psc_dynarray		 mwc_multiwaits;/* where cond is registered */
	struct psc_multiwait		*mwc_winner;	/* which multiwait awoke first */
	const void			*mwc_data;	/* pointer to user data */
	int				 mwc_flags;
	char				 mwc_name[48];	/* should be on 8-byte boundary */
};

#define PMWCF_WAKEALL			(1 << 0)	/* wake all multiwaits, not just one */

#define MWCOND_INIT(data, name, flags)					\
	{ PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,		\
	    DYNARRAY_INIT, NULL, (data), (flags), (name) }

struct psc_multiwait {
	/*
	 * Multiwaits have mutexes not because threads may "share" them,
	 * explicitly, but because threads may modify some fields
	 * implicitly (e.g. on wakeups and condition removals).
	 */
	pthread_mutex_t			 mw_mutex;
	pthread_cond_t			 mw_cond;	/* master condition */
	struct psc_multiwaitcond	*mw_waker;	/* which mwcond woke us */
	struct psc_dynarray		 mw_conds;	/* registered conditions */
	struct psc_vbitmap		*mw_condmask;	/* which conds can wake us */
	int				 mw_flags;
	char				 mw_name[32];	/* should be 8-byte boundary */
};

#define PMWF_CRITSECT			(1 << 0)	/* inside critical section */

/**
 * psc_multiwait_addcond - add a condition to a multiwait.
 * @mw: a multiwait.
 * @mwc: the condition to add.
 */
#define psc_multiwait_addcond(mw, c)		_psc_multiwait_addcond((mw), (c), 1)
#define psc_multiwait_addcond_masked(mw, c, m)	_psc_multiwait_addcond((mw), (c), (m))

/**
 * psc_multiwait_* - wait for any of a number of conditions in a multiwait
 *	to become available.
 * @mw: the multiwait whose registered conditions will be waited upon.
 * @data: pointer to user data filled in from the multiwaitcond.
 * @usec: # microseconds till timeout.
 */
#define psc_multiwaitms(mw, p, ms)		psc_multiwaitus((mw), (p), (ms) * 1000)
#define psc_multiwaits(mw, p, s)		psc_multiwaitus((mw), (p), (s) * 1000 * 1000)
#define psc_multiwait(mw, p)			psc_multiwaitus((mw), (p), 0)

int	_psc_multiwait_addcond(struct psc_multiwait *, struct psc_multiwaitcond *, int);
void	 psc_multiwait_entercritsect(struct psc_multiwait *);
void	 psc_multiwait_free(struct psc_multiwait *);
int	 psc_multiwait_hascond(struct psc_multiwait *, struct psc_multiwaitcond *);
void	 psc_multiwait_init(struct psc_multiwait *, const char *, ...);
void	 psc_multiwait_leavecritsect(struct psc_multiwait *);
void	 psc_multiwait_setcondwakeable(struct psc_multiwait *, const struct psc_multiwaitcond *, int);
void	 psc_multiwait_reset(struct psc_multiwait *);
void	 psc_multiwait_prconds(struct psc_multiwait *);
int	 psc_multiwaitus(struct psc_multiwait *, void *, int);

void	 psc_multiwaitcond_destroy(struct psc_multiwaitcond *);
void	 psc_multiwaitcond_init(struct psc_multiwaitcond *, const void *, int, const char *, ...);
size_t	 psc_multiwaitcond_nwaiters(struct psc_multiwaitcond *);
void	 psc_multiwaitcond_wait(struct psc_multiwaitcond *, pthread_mutex_t *);
void	 psc_multiwaitcond_wakeup(struct psc_multiwaitcond *);

#endif /* _PFL_MULTIWAIT_H_ */
