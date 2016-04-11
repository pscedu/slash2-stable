/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2006-2016, Pittsburgh Supercomputing Center
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

#ifndef _PFL_FAULT_H_
#define _PFL_FAULT_H_

#include "pfl/hashtbl.h"
#include "pfl/lock.h"

#define PFL_FAULT_NAME_MAX	32

struct pfl_fault {
	psc_spinlock_t		pflt_lock;
	char			pflt_name[PFL_FAULT_NAME_MAX];
	int			pflt_flags;		/* see below */
	int			pflt_hits;		/* #times triggered */
	int			pflt_unhits;		/* #times skipped */
	int			pflt_skips;		/* fault skipped */
	int			pflt_interval;		/* fault every # times */
	long			pflt_delay;		/* seconds to sleep inside fault */
	int			pflt_count;		/* #times to respond with programmed behavior */
	int			pflt_begin;		/* #times to skip programmed behavior */
	int			pflt_chance;		/* likelihood of event to occur from 0-100 */
	int			pflt_retval;		/* interject return code */
};

/* fault flags */
#define PFLTF_ACTIVE		(1 << 0)		/* fault point is activated */

#define	pfl_fault_lock(pflt)	spinlock(&(pflt)->pflt_lock)
#define	pfl_fault_unlock(pflt)	freelock(&(pflt)->pflt_lock)

#define pfl_fault_register(name)	_pfl_fault_get((name), 1)
#define pfl_fault_get(name)		_pfl_fault_get((name), 1)
#define pfl_fault_peek(name)		_pfl_fault_get((name), 0)

int	_pfl_fault_here(struct pfl_fault *, int *, int);
void	 pfl_fault_destroy(int);
struct pfl_fault *
	_pfl_fault_get(const char *, int);

#define pfl_fault_here_rc(name, rcp, rc)				\
	({								\
		static struct pfl_fault *_fault;			\
		int _rc = 0;						\
									\
		if (_fault == NULL)					\
			_fault = pfl_fault_get(name);			\
		if (_fault->pflt_flags & PFLTF_ACTIVE)			\
			_rc = _pfl_fault_here(_fault, (rcp), (rc));	\
		_rc;							\
	})

#define pfl_fault_here(f, rcp)	pfl_fault_here_rc((f), (rcp), 0)

extern struct psc_dynarray	pfl_faults;
extern psc_spinlock_t		pfl_faults_lock;

#endif /* _PFL_FAULT_H_ */
