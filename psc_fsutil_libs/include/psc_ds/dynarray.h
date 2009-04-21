/* $Id$ */

/*
 * Routines for managing dynamically-sized arrays.
 */

#ifndef _PFL_DYNARRAY_H_
#define _PFL_DYNARRAY_H_

struct dynarray {
	int		  da_pos;
	int		  da_nalloc;
	void		**da_items;
};

#define DYNARRAY_INIT { 0, 0, NULL }

int	 dynarray_add(struct dynarray *, void *);
int	 dynarray_add_ifdne(struct dynarray *, void *);
int	 dynarray_ensurelen(struct dynarray *, int);
int	 dynarray_exists(const struct dynarray *, const void *);
void	 dynarray_free(struct dynarray *);
int	 dynarray_freeslack(struct dynarray *);
void	*dynarray_get(const struct dynarray *);
void	*dynarray_getpos(const struct dynarray *, int);
void	 dynarray_init(struct dynarray *);
int	 dynarray_len(const struct dynarray *);
void	 dynarray_remove(struct dynarray *, const void *);
void	 dynarray_reset(struct dynarray *);

#endif /* _PFL_DYNARRAY_H_ */
