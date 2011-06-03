/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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

uint32_t psc_random32(void);
uint32_t psc_random32u(uint32_t);
uint64_t psc_random64(void);

#endif /* _PFL_RANDOM_H_ */
