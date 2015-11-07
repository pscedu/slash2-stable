/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_FAULT_H_
#define _PFL_FAULT_H_

#include "pfl/hashtbl.h"
#include "pfl/lock.h"

#define PSC_FAULT_NAME_MAX	32

struct psc_fault {
	psc_spinlock_t		pflt_lock;
	char			pflt_name[PSC_FAULT_NAME_MAX];
	int			pflt_flags;		/* see below */
	int			pflt_hits;		/* #times triggered */
	int			pflt_unhits;		/* #times skipped */
	long			pflt_delay;		/* usec to sleep inside fault */
	int			pflt_count;		/* #times to respond with programmed behavior */
	int			pflt_begin;		/* #times to skip programmed behavior */
	int			pflt_chance;		/* likelihood of event to occur from 0-100 */
	int			pflt_retval;		/* interject return code */
};

/* fault flags */
#define PFLTF_ACTIVE		(1 << 0)		/* fault point is activated */

#define	psc_fault_lock(pflt)	spinlock(&(pflt)->pflt_lock)
#define	psc_fault_unlock(pflt)	freelock(&(pflt)->pflt_lock)

int	_psc_fault_here(struct psc_fault *, int *, int);
struct psc_fault *
	_psc_fault_register(const char *);

#define psc_fault_here_rc(f, rcp, rc)					\
	_PFL_RVSTART {							\
		int _rc = 0;						\
									\
		if (psc_faults[f].pflt_flags & PFLTF_ACTIVE)		\
			_rc = _psc_fault_here(&psc_faults[f], (rcp),	\
			    (rc));					\
		_rc;							\
	} _PFL_RVEND

#define psc_fault_here(f, rcp)	psc_fault_here_rc((f), (rcp), 0)

#define psc_fault_register(f)	_psc_fault_register(#f)

extern struct psc_fault		 psc_faults[];

#endif /* _PFL_FAULT_H_ */
