/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_FAULT_H_
#define _PFL_FAULT_H_

#include "pfl/hashtbl.h"
#include "psc_util/lock.h"

#define PSC_FAULT_NAME_MAX	32

struct psc_fault {
	psc_spinlock_t		pflt_lock;
	struct psc_hashent	pflt_hentry;
	char			pflt_name[PSC_FAULT_NAME_MAX];
	int			pflt_flags;		/* see below */
	int			pflt_hits;		/* #times triggered so far */
	int			pflt_unhits;		/* #times un-triggered so far */
	long			pflt_delay;		/* usec */
	int			pflt_count;		/* #times to respond with programmed behavior */
	int			pflt_begin;		/* #times to skip programmed behavior */
	int			pflt_chance;		/* liklihood of event to occur from 0-100 */
	int			pflt_retval;		/* forced return code */
};

/* fault flags */
#define PFLTF_ACTIVE	(1 << 0)		/* fault point is activated */

#define	psc_fault_lock(pflt)	spinlock(&(pflt)->pflt_lock)
#define	psc_fault_unlock(pflt)	freelock(&(pflt)->pflt_lock)

void	psc_faults_init(void);

struct psc_fault *
	psc_fault_lookup(const char *);
int	psc_fault_add(const char *);
void	psc_fault_here(const char *, int *);
int	psc_fault_register(const char *, int, int, int, int, int);
int	psc_fault_remove(const char *);

extern struct psc_hashtbl	 psc_fault_table;
extern const char		*psc_fault_names[];

#endif /* _PFL_FAULT_H_ */
