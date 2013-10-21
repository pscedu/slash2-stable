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

#define PFL_NMEMKEYS		0

struct psc_memnode {
	psc_spinlock_t		pmn_lock;
	struct psc_dynarray	pmn_keys;
};

struct psc_memnode *
	 psc_memnode_get(void);
void	*psc_memnode_getkey(struct psc_memnode *, int);
void	*psc_memnode_getobj(int, void *(*)(void *), void *);
void	 psc_memnode_init(void);
void	 psc_memnode_setkey(struct psc_memnode *, int, void *);

#endif /* _PFL_MEMNODE_H_ */
