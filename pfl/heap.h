/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2014, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Routines for priority queue heaps.
 */

#ifndef _PFL_HEAP_H_
#define _PFL_HEAP_H_

struct pfl_heap {
	void	**ph_base;
	int	(*ph_cmpf)(const void *, const void *);
	int	  ph_entoff;
	int	  ph_nitems;
	int	  ph_nalloc;
};

struct pfl_heap_entry {
	int	  phe_idx;
} __packed;

#define HEAP_INIT(type, memb, cmpf)					\
	{ NULL, (cmpf), offsetof(type, memb), 0, 0 }

void	 pfl_heap_add(struct pfl_heap *, void *);
void	 pfl_heap_remove(struct pfl_heap *, void *);
void	*pfl_heap_shift(struct pfl_heap *);
void	 pfl_heap_reseat(struct pfl_heap *, void *);

#endif /* _PFL_HEAP_H_ */
