/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
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
