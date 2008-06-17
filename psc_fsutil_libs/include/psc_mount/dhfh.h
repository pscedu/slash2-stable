/* $Id$ */

#include "psc_types.h"
#include "psc_ds/tree.h"
#include "psc_util/lock.h"

#define FD_REG_NEW   0
#define FD_REG_EXIST 1
#define FD_REG_INIT  0xffffffff
#define FD_REG_READY 0x00000000

struct fhent {
	u64			 fh;
	u32			 fd;
	atomic_t		 refcnt;
	psc_spinlock_t		 lock;
	union {
		u64		 magic;
		void		*private;
	}			 fh_data;
	SPLAY_ENTRY(fhent)	 entry;
#define fh_pri   fh_data.private
#define fh_magic fh_data.magic
};

struct dhent {
	int			 dfd;
	u64			 fh;
	psc_spinlock_t		 lock;
	int			 refcnt;
	SPLAY_ENTRY(dhent)	 entry;
};

#define	fh_lookup(fh) _fh_lookup((fh), 0)
#define	fh_remove(fh) _fh_lookup((fh), 1)

struct fhent *	_fh_lookup(u64, int);
u64		fh_register(u64,
			void (*)(struct fhent *, int, void **), void *[]);
int		fh_reap(void);

#define	dh_lookup(fh) _dh_lookup((fh), 0)
#define	dh_remove(fh) _dh_lookup((fh), 1)

struct dhent *	_dh_lookup(u64, int);
u64		dh_register(int);
void		dh_release(struct dhent *);
void		dh_destroy(struct dhent *);
int		dh_reap(void);
