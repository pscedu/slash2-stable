/* $Id$ */

/*
 * Routines for managing dynamically-sized arrays.
 */

#ifndef _PFL_DYNARRAY_H_
#define _PFL_DYNARRAY_H_

struct psc_dynarray {
	int		  pda_pos;
	int		  pda_nalloc;
	void		**pda_items;
#define da_pos		pda_pos
#define da_nalloc	pda_nalloc
#define da_items	pda_items
};

#define dynarray	psc_dynarray

#define DYNARRAY_INIT	{ 0, 0, NULL }

/**
 * DYNARRAY_FOREACH - Iterate across items of a dynarray.
 * @p: item iterator.
 * @n: integer iterator variable.
 * @pda: dynamic array.
 * Notes: do not invoke dynarray_add/remove() in the body of this loop.
 */
#define DYNARRAY_FOREACH(p, n, pda)					\
	for ((n) = 0; (n) < dynarray_len(pda) &&			\
	    (((p) = dynarray_getpos((pda), (n))) || 1); (n)++)

/**
 * dynarray_len - Obtain the number of elements stored in a dynamic array.
 * @pda: dynamic array to access.
 */
#define dynarray_len(pda)	(pda)->pda_pos

/**
 * dynarray_get - Access a dynamic array's item list.
 * @pda: dynamic array to access.
 */
#define dynarray_get(pda)	((void *)(pda)->pda_items)

/**
 * dynarray_sort - Reorder elements of a dynarray.
 * @pda: dynamic array whose elements to sort.
 * @sortf: which sorting algorithm to use.
 * @cmpf: criteria for reordering items.
 * Returns -1 on failure or zero on success.
 */
#define psc_dynarray_sort(pda, sortf, cmpf)				\
	(sortf)((pda)->pda_items, (pda)->pda_pos,			\
	    sizeof(*(pda)->pda_items), (cmpf))

int	 dynarray_add(struct psc_dynarray *, void *);
int	 dynarray_add_ifdne(struct psc_dynarray *, void *);
int	 dynarray_ensurelen(struct psc_dynarray *, int);
int	 dynarray_exists(const struct psc_dynarray *, const void *);
void	 dynarray_free(struct psc_dynarray *);
int	 dynarray_freeslack(struct psc_dynarray *);
void	*dynarray_getpos(const struct psc_dynarray *, int);
void	 dynarray_init(struct psc_dynarray *);
void	 dynarray_remove(struct psc_dynarray *, const void *);
void	 dynarray_reset(struct psc_dynarray *);

#endif /* _PFL_DYNARRAY_H_ */
