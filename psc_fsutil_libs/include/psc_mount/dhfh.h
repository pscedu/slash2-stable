/* $Id$ */
#ifndef __DHFH_H
#define __DHFH_H 1

#include "psc_types.h"
#include "psc_ds/tree.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"


/* Register operation parameters.
 */ 
#define FD_REG_NEW   0
#define FD_REG_EXIST 1

struct fhent {
	u64			 fh_id;
	u64                      fh_gen;
	u32			 fh_state;
	atomic_t		 fh_refcnt;
	psc_spinlock_t		 fh_lock;
	void                    *fh_private;
	SPLAY_ENTRY(fhent)	 fh_entry;
};

/* Valid fhent states, the first two help with initialization race
 *  conditions.
 */
enum fhent_states {
	FHENT_INIT   = (1<<0),
	FHENT_READY  = (1<<1),
	FHENT_READ   = (1<<2),
	FHENT_WRITE  = (1<<3),
	FHENT_OPEN   = (1<<4),
	FHENT_LOOKUP = (1<<5),
	FHENT_DIR    = (1<<6)
};

#define	fh_lookup(fh) _fh_lookup((fh), 0)
#define	fh_remove(fh) _fh_lookup((fh), 1)

struct fhent * _fh_lookup(u64, int);
struct fhent * fh_register(u64, int,
			void (*)(struct fhent *, int, void **), void *[]);
int	       fh_reap(void);

/* XXX convert these so that we have one fh cache for 
 *  dirs and files.
 */
struct dhent *	_dh_lookup(u64, int);
u64		dh_register(int);
void		dh_release(struct dhent *);
void		dh_destroy(struct dhent *);
int		dh_reap(void);

#endif
