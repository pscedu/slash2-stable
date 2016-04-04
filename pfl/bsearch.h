/* $Id$ */
/* %ISC_LICENSE% */

#ifndef _PFL_BSEARCH_H_
#define _PFL_BSEARCH_H_

int	bsearch_floor(const void *, const void *, size_t, size_t,
	    int (*)(const void *, const void *));
int	bsearch_ceil(const void *, const void *, size_t, size_t,
	    int (*)(const void *, const void *));

#endif /* _PFL_BSEARCH_H_ */
