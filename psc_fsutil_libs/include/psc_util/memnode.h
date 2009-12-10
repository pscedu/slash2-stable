/* $Id$ */

#ifndef _PFL_MEMNODE_H_
#define _PFL_MEMNODE_H_

#include <pthread.h>

#ifdef HAVE_NUMA
#include <numa.h>
#endif

#ifdef HAVE_NUMA
# define psc_memnode_getid()	numa_preferred()
#else
# define psc_memnode_getid()	0
#endif

struct psc_memnode {
	psc_spinlock_t		pmn_lock;
	struct psc_dynarray	pmn_keys;
};

struct psc_memnode *
	 psc_memnode_get(void);
void	*psc_memnode_getkey(struct psc_memnode *, int);
void	 psc_memnode_setkey(struct psc_memnode *, int, void *);
void	 psc_memnode_init(void);

#endif /* _PFL_MEMNODE_H_ */
