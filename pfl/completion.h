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

#ifndef _PFL_COMPLETION_H_
#define _PFL_COMPLETION_H_

#include "psc_util/lock.h"
#include "psc_util/waitq.h"

struct psc_compl {
	struct psc_waitq	pc_wq;
	psc_spinlock_t		pc_lock;
	int			pc_done;
	int			pc_rc;		/* optional "barrier" value */
};

#define PSC_COMPL_INIT		{ PSC_WAITQ_INIT, SPINLOCK_INIT, 0, 1 }

#define psc_compl_ready(pc, rc)	_psc_compl_ready((pc), (rc), 0)
#define psc_compl_one(pc, rc)	_psc_compl_ready((pc), (rc), 1)

#define psc_compl_wait(pc)	 psc_compl_waitrel_s((pc), 0)

void	 psc_compl_destroy(struct psc_compl *);
void	 psc_compl_init(struct psc_compl *);
void	_psc_compl_ready(struct psc_compl *, int, int);
int	 psc_compl_waitrel_s(struct psc_compl *, int);

#endif /* _PFL_COMPLETION_H_ */
