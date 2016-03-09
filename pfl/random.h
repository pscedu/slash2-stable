/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_RANDOM_H_
#define _PFL_RANDOM_H_

#include <stdint.h>

struct rnd_iterator {
	int	ri_n;
	int	ri_rnd_idx;
	int	ri_iter;
};

#define _RESET_RND_ITER(ri)						\
	(ri)->ri_iter = 0,						\
	(ri)->ri_rnd_idx = psc_random32u((ri)->ri_n)

#define FOREACH_RND(ri, n)						\
	for ((ri)->ri_n = (n), _RESET_RND_ITER(ri);			\
	    (ri)->ri_iter < (ri)->ri_n;					\
	    (ri)->ri_iter++, (ri)->ri_rnd_idx + 1 >= (ri)->ri_n ?	\
	      ((ri)->ri_rnd_idx = 0) : (ri)->ri_rnd_idx++)

#define RESET_RND_ITER(ri)	_RESET_RND_ITER(ri)

void pfl_random_getbytes(void *, size_t);

uint32_t psc_random32(void);
uint32_t psc_random32u(uint32_t);
uint64_t psc_random64(void);

#endif /* _PFL_RANDOM_H_ */
