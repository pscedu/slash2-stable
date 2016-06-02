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

#ifndef _PFL_COMPLETION_H_
#define _PFL_COMPLETION_H_

#include "pfl/lock.h"
#include "pfl/pthrutil.h"
#include "pfl/waitq.h"

struct psc_compl {
	struct psc_waitq	pc_wq;
	struct psc_spinlock	pc_lock;
	int			pc_counter;
	int			pc_done;
	int			pc_rc;		/* optional "barrier" value */
};

#define PSC_COMPL_INIT(name)	{ PSC_WAITQ_INIT(name), SPINLOCK_INIT, 0, 0, 1 }

#define psc_compl_ready(pc, rc)	_psc_compl_ready((pc), (rc), 0)
#define psc_compl_one(pc, rc)	_psc_compl_ready((pc), (rc), 1)

#define psc_compl_wait(pc)	 psc_compl_waitrel((pc), 0, NULL, 0, 0)
#define psc_compl_waitrel_s(pc, spinlock, s)				\
				 psc_compl_waitrel((pc),		\
				     PFL_LOCKPRIMT_SPIN, (spinlock),	\
				     (s), 0)

void	 psc_compl_destroy(struct psc_compl *);
void	 psc_compl_init(struct psc_compl *);
void	_psc_compl_ready(struct psc_compl *, int, int);
int	 psc_compl_waitrel(struct psc_compl *, enum pfl_lockprim, void *, long, long);

#endif /* _PFL_COMPLETION_H_ */
