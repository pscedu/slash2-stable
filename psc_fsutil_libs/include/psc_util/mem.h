/* $Id$ */

#ifndef _PFL_MEM_H_
#define _PFL_MEM_H_

#ifdef HAVE_CPUSET

#include <sys/types.h>

#include <numa.h>

struct psc_nodemask {
	nodemask_t	pnm_mask;
};

#define	psc_numa_get_run_node_mask(m)				\
	do {							\
		(m)->pnm_mask = numa_get_run_node_mask();	\
	} while (0)

#define	psc_numa_tonodemask_memory(p, siz, m)			\
	numa_tonodemask_memory((p), (siz), &(m)->pnm_mask)

#else

struct psc_nodemask {
};

#define	psc_numa_get_run_node_mask(m)			\
	do {						\
		/* hack to avoid unused warnings */	\
		(void)(m);				\
	} while (0)

#define	psc_numa_tonodemask_memory(p, siz, m)		\
	do {						\
		(void)(p);				\
		(void)(siz);				\
		(void)(m);				\
	} while (0)

#endif

#endif /* _PFL_MEM_H_ */
