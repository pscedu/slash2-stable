/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_ATOMIC_H_
#define _PFL_ATOMIC_H_

#include <stdint.h>
#include <stdarg.h>

#include "pfl/log.h"

#define psc_atomic16_init(v)
#define psc_atomic16_add			_pfl_gen_atomic16_add
#define psc_atomic16_sub			_pfl_gen_atomic16_sub
#define psc_atomic16_sub_and_test0		_pfl_gen_atomic16_sub_and_test0
#define psc_atomic16_add_and_test0		_pfl_gen_atomic16_add_and_test0
#define psc_atomic16_inc			_pfl_gen_atomic16_inc
#define psc_atomic16_dec			_pfl_gen_atomic16_dec
#define psc_atomic16_inc_and_test0		_pfl_gen_atomic16_inc_and_test0
#define psc_atomic16_dec_and_test0		_pfl_gen_atomic16_dec_and_test0
#define psc_atomic16_add_and_test_neg		_pfl_gen_atomic16_add_and_test_neg
#define psc_atomic16_sub_and_test_neg		_pfl_gen_atomic16_sub_and_test_neg
#define psc_atomic16_add_getnew			_pfl_gen_atomic16_add_getnew
#define psc_atomic16_clearmask			_pfl_gen_atomic16_clearmask
#define psc_atomic16_setmask			_pfl_gen_atomic16_setmask
#define psc_atomic16_clearmask_getnew		_pfl_gen_atomic16_clearmask_getnew
#define psc_atomic16_setmask_getnew		_pfl_gen_atomic16_setmask_getnew
#define psc_atomic16_clearmask_getold		_pfl_gen_atomic16_clearmask_getold
#define psc_atomic16_setmask_getold		_pfl_gen_atomic16_setmask_getold
#define psc_atomic16_xchg			_pfl_gen_atomic16_xchg

#define psc_atomic32_init(v)
#define psc_atomic32_add			_pfl_gen_atomic32_add
#define psc_atomic32_sub			_pfl_gen_atomic32_sub
#define psc_atomic32_sub_and_test0		_pfl_gen_atomic32_sub_and_test0
#define psc_atomic32_add_and_test0		_pfl_gen_atomic32_add_and_test0
#define psc_atomic32_inc			_pfl_gen_atomic32_inc
#define psc_atomic32_dec			_pfl_gen_atomic32_dec
#define psc_atomic32_inc_and_test0		_pfl_gen_atomic32_inc_and_test0
#define psc_atomic32_dec_and_test0		_pfl_gen_atomic32_dec_and_test0
#define psc_atomic32_add_and_test_neg		_pfl_gen_atomic32_add_and_test_neg
#define psc_atomic32_sub_and_test_neg		_pfl_gen_atomic32_sub_and_test_neg
#define psc_atomic32_add_getnew			_pfl_gen_atomic32_add_getnew
#define psc_atomic32_clearmask			_pfl_gen_atomic32_clearmask
#define psc_atomic32_setmask			_pfl_gen_atomic32_setmask
#define psc_atomic32_clearmask_getnew		_pfl_gen_atomic32_clearmask_getnew
#define psc_atomic32_setmask_getnew		_pfl_gen_atomic32_setmask_getnew
#define psc_atomic32_clearmask_getold		_pfl_gen_atomic32_clearmask_getold
#define psc_atomic32_setmask_getold		_pfl_gen_atomic32_setmask_getold
#define psc_atomic32_xchg			_pfl_gen_atomic32_xchg
#define PSC_ATOMIC32_XCHG			psc_atomic32_xchg

#define psc_atomic64_init(v)
#define psc_atomic64_add			_pfl_gen_atomic64_add
#define psc_atomic64_sub			_pfl_gen_atomic64_sub
#define psc_atomic64_sub_and_test0		_pfl_gen_atomic64_sub_and_test0
#define psc_atomic64_add_and_test0		_pfl_gen_atomic64_add_and_test0
#define psc_atomic64_inc			_pfl_gen_atomic64_inc
#define psc_atomic64_dec			_pfl_gen_atomic64_dec
#define psc_atomic64_inc_and_test0		_pfl_gen_atomic64_inc_and_test0
#define psc_atomic64_dec_and_test0		_pfl_gen_atomic64_dec_and_test0
#define psc_atomic64_add_and_test_neg		_pfl_gen_atomic64_add_and_test_neg
#define psc_atomic64_sub_and_test_neg		_pfl_gen_atomic64_sub_and_test_neg
#define psc_atomic64_add_getnew			_pfl_gen_atomic64_add_getnew
#define psc_atomic64_clearmask			_pfl_gen_atomic64_clearmask
#define psc_atomic64_setmask			_pfl_gen_atomic64_setmask
#define psc_atomic64_clearmask_getnew		_pfl_gen_atomic64_clearmask_getnew
#define psc_atomic64_setmask_getnew		_pfl_gen_atomic64_setmask_getnew
#define psc_atomic64_clearmask_getold		_pfl_gen_atomic64_clearmask_getold
#define psc_atomic64_setmask_getold		_pfl_gen_atomic64_setmask_getold
#define psc_atomic64_xchg			_pfl_gen_atomic64_xchg

struct psc_atomic16 { volatile int16_t value16 __aligned(4); } __packed;
struct psc_atomic64 { volatile int64_t value64 __aligned(8); } __packed;

#include "pfl/_atomic32.h"

#define psc_atomic16_t struct psc_atomic16
#define psc_atomic64_t struct psc_atomic64

#ifdef __ia64
# include "pfl/compat/ia64/atomic.h"
#elif defined(__amd64)
# include "pfl/compat/amd64/atomic.h"
#else
# include "pfl/compat/i386/atomic.h"
#endif

#include "pfl/compat/generic/atomic.h"

#define psc_atomic16_inc_getnew(v)		psc_atomic16_add_getnew((v), 1)
#define psc_atomic32_inc_getnew(v)		psc_atomic32_add_getnew((v), 1)
#define psc_atomic64_inc_getnew(v)		psc_atomic64_add_getnew((v), 1)

#define psc_atomic16_dec_getnew(v)		psc_atomic16_sub_getnew((v), 1)
#define psc_atomic32_dec_getnew(v)		psc_atomic32_sub_getnew((v), 1)
#define psc_atomic64_dec_getnew(v)		psc_atomic64_sub_getnew((v), 1)

#define psc_atomic16_sub_getnew(v, i)		psc_atomic16_add_getnew((v), -(i))	/* u32 cast */
#define psc_atomic32_sub_getnew(v, i)		psc_atomic32_add_getnew((v), -(i))
#define psc_atomic64_sub_getnew(v, i)		psc_atomic64_add_getnew((v), -(i))

static __inline int
psc_atomic16_testmask_set(psc_atomic16_t *v, int16_t mask, int16_t newval)
{
	int16_t oldval;

	oldval = psc_atomic16_read(v);
	if (oldval & mask)
		return (0);
	if (psc_atomic16_cmpxchg(v, oldval, newval) != oldval)
		return (0);
	return (1);
}

static __inline int
psc_atomic32_testmask_set(psc_atomic32_t *v, int32_t mask, int32_t newval)
{
	int32_t oldval;

	oldval = psc_atomic32_read(v);
	if (oldval & mask)
		return (0);
	if (psc_atomic32_cmpxchg(v, oldval, newval) != oldval)
		return (0);
	return (1);
}

static __inline int
psc_atomic64_testmask_set(psc_atomic64_t *v, int64_t mask, int64_t newval)
{
	int64_t oldval;

	oldval = psc_atomic64_read(v);
	if (oldval & mask)
		return (0);
	if (psc_atomic64_cmpxchg(v, oldval, newval) != oldval)
		return (0);
	return (1);
}

static __inline void
psc_atomic32_setmax(psc_atomic32_t *v, int32_t val)
{
	int32_t oldval;

	do {
		oldval = psc_atomic32_read(v);
		if (val <= oldval)
			break;
	} while (psc_atomic32_cmpxchg(v, oldval, val) != oldval);
}

/* default width */
typedef psc_atomic32_t psc_atomic_t;

/* atomic_t compat */
typedef psc_atomic32_t atomic_t;

#define ATOMIC_INIT(i)			PSC_ATOMIC32_INIT(i)

#define atomic_read(v)			psc_atomic32_read(v)
#define atomic_set(v, i)		psc_atomic32_set(v, i)
#define atomic_add(i, v)		psc_atomic32_add(v, i)
#define atomic_sub(i, v)		psc_atomic32_sub(v, i)
#define atomic_sub_and_test(v, i)	psc_atomic32_sub_and_test0(v, i)
#define atomic_add_and_test(v, i)	psc_atomic32_add_and_test0(v, i)
#define atomic_inc(v)			psc_atomic32_inc(v)
#define atomic_dec(v)			psc_atomic32_dec(v)
#define atomic_inc_return(v)		psc_atomic32_inc_getnew(v)
#define atomic_dec_return(v)		psc_atomic32_dec_getnew(v)
#define atomic_inc_and_test(v)		psc_atomic32_inc_and_test0(v)
#define atomic_dec_and_test(v)		psc_atomic32_dec_and_test0(v)
#define atomic_add_and_test_neg(v, i)	psc_atomic32_add_and_test_neg(v, i)
#define atomic_sub_and_test_neg(v, i)	psc_atomic32_sub_and_test_neg(v, i)
#define atomic_add_getnew(v, i)		psc_atomic32_add_getnew(v, i)
#define atomic_sub_getnew(v, i)		psc_atomic32_sub_getnew(v, i)
#define atomic_clearmask(v, i)		psc_atomic32_clearmask(v, i)
#define atomic_setmask(v, i)		psc_atomic32_setmask(v, i)
#define atomic_clearmask_getnew(v, i)	psc_atomic32_clearmask_getnew(v, i)
#define atomic_setmask_getnew(v, i)	psc_atomic32_setmask_getnew(v, i)
#define atomic_clearmask_getold(v, i)	psc_atomic32_clearmask_getold(v, i)
#define atomic_setmask_getold(v, i)	psc_atomic32_setmask_getold(v, i)
#define atomic_xchg(v, i)		psc_atomic32_xchg(v, i)
#define atomic_cmpxchg(v, newv, cmpv)	psc_atomic32_cmpxchg(v, newv, cmpv)

#endif /* _PFL_ATOMIC_H_ */
