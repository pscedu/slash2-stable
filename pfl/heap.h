/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2014-2015, Pittsburgh Supercomputing Center (PSC).
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

#define pfl_heap_init(hp, type, memb, cmpf)				\
	_pfl_heap_init((hp), offsetof(type, memb), cmpf)

void	 pfl_heap_add(struct pfl_heap *, void *);
void	_pfl_heap_init(struct pfl_heap *, int, int (*)(const void *, const void *));
int	 pfl_heap_nitems(struct pfl_heap *);
void	*pfl_heap_peek(struct pfl_heap *);
void	*pfl_heap_peekidx(struct pfl_heap *, int);
void	 pfl_heap_remove(struct pfl_heap *, void *);
void	 pfl_heap_reseat(struct pfl_heap *, void *);
void	*pfl_heap_shift(struct pfl_heap *);

#endif /* _PFL_HEAP_H_ */
