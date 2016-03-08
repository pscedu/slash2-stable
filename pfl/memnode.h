/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_MEMNODE_H_
#define _PFL_MEMNODE_H_

#ifdef HAVE_NUMA
#include <numa.h>
#endif

#include "pfl/lock.h"
#include "pfl/dynarray.h"

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
