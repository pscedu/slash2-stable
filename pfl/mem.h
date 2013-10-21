/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_MEM_H_
#define _PFL_MEM_H_

#ifdef HAVE_NUMA

#include <numa.h>

struct psc_nodemask {
	struct bitmask	*pnm_mask;
};

#  define psc_numa_get_run_node_mask(m)					\
	do {								\
		(m)->pnm_mask = numa_get_run_node_mask();		\
	} while (0)

#  define psc_numa_tonodemask_memory(p, len, m)				\
	numa_tonodemask_memory((p), (len), (m)->pnm_mask)

#else

struct psc_nodemask {
};

#  define psc_numa_get_run_node_mask(m)					\
	do {								\
		/* hack to avoid unused warnings */			\
		(void)(m);						\
	} while (0)

#  define psc_numa_tonodemask_memory(p, len, m)				\
	do {								\
		(void)(p);						\
		(void)(len);						\
		(void)(m);						\
	} while (0)

#endif

#define psc_mprotect(p, len, prot)					\
	do {								\
		if (mprotect((p), (len), (prot)) == -1)			\
			psc_fatal("mprotect %p", (p));			\
	} while (0)

#define psc_munmap(p, len)						\
	do {								\
		if (munmap((p), (len)) == -1)				\
			psc_fatal("munmap %p", (p));			\
	} while (0)

void migrate_numa(void);

#endif /* _PFL_MEM_H_ */
