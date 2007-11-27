/* $Id: dynarray.h 2197 2007-11-08 21:41:22Z yanovich $ */

/*
 * Routines for managing dynamically-sized arrays.
 */

#ifndef _DYNARRAY_H_
#define _DYNARRAY_H_

struct dynarray {
	int		  da_pos;
	int		  da_nalloc;
	const void	**da_items;
};

#define DYNARRAY_INIT { 0, 0, NULL}

void	 dynarray_init(struct dynarray *);
int	 dynarray_add(struct dynarray *, const void *);
void	*dynarray_get(const struct dynarray *);
void	 dynarray_free(struct dynarray *);
void	 dynarray_reset(struct dynarray *);
int	 dynarray_len(const struct dynarray *);
void	 dynarray_remove(struct dynarray *, const void *);
int	 dynarray_ensure(struct dynarray *, int);
int	 dynarray_freeslack(struct dynarray *);

#endif /* _DYNARRAY_H_ */
