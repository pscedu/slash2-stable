/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2014-2015, Pittsburgh Supercomputing Center (PSC).
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
int	 pfl_heap_nitems(struct pfl_heap *);
void	*pfl_heap_peek(struct pfl_heap *);
void	*pfl_heap_peekidx(struct pfl_heap *, int);
void	 pfl_heap_remove(struct pfl_heap *, void *);
void	 pfl_heap_reseat(struct pfl_heap *, void *);
void	*pfl_heap_shift(struct pfl_heap *);

#endif /* _PFL_HEAP_H_ */
