/* $Id$ */

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
	u32			 fh_state;
	atomic_t		 fh_refcnt;
	psc_spinlock_t		 fh_lock;
	union {
		u64		 fhu_magic;
		void		*fhu_private;
	}			 fh_data;
	SPLAY_ENTRY(fhent)	 fh_entry;
#define fh_pri   fh_data.fhu_private
#define fh_magic fh_data.fhu_magic
};

/* Valid fhent states, the first two help with initialization race
 *  conditions.
 */
enum fhent_states {
	FHENT_INIT  = (1<<0),
	FHENT_READY = (1<<1),
	FHENT_READ  = (1<<2),
	FHENT_WRITE = (1<<3)
};

struct dhent {
	u64			 dh_id;
	int			 dh_dfd;
	psc_spinlock_t		 dh_lock;
	int			 dh_refcnt;
	SPLAY_ENTRY(dhent)	 dh_entry;
};

#define	fh_lookup(fh) _fh_lookup((fh), 0)
#define	fh_remove(fh) _fh_lookup((fh), 1)

struct fhent *	_fh_lookup(u64, int);
void		fh_register(u64, int,
			void (*)(struct fhent *, int, void **), void *[]);
int		fh_reap(void);

#define	dh_lookup(fh) _dh_lookup((fh), 0)
#define	dh_remove(fh) _dh_lookup((fh), 1)

struct dhent *	_dh_lookup(u64, int);
u64		dh_register(int);
void		dh_release(struct dhent *);
void		dh_destroy(struct dhent *);
int		dh_reap(void);
