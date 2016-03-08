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

#define _PFL_ATOMIC_OPLOOPRC(prefix, type, v, oldv, newv, op, retcode)		\
	do {									\
		type oldv, newv;						\
										\
		do {								\
			oldv = prefix ## _read(v);				\
			newv = op;						\
		} while (prefix ## _cmpxchg((v), oldv, newv) != oldv);		\
		retcode;							\
	} while (0)

#define _PFL_ATOMIC_OPLOOP(prefix, type, v, oldv, newv, op, rv)			\
	_PFL_ATOMIC_OPLOOPRC(prefix, type, v, oldv, newv, op, return (rv))

#define _PFL_ATOMIC_OPLOOPV(prefix, type, v, oldv, newv, op)			\
	_PFL_ATOMIC_OPLOOPRC(prefix, type, v, oldv, newv, op, return)

static __inline int16_t
_pfl_gen_atomic16_clearmask_getold(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv & ~i, oldv);
}

static __inline int32_t
_pfl_gen_atomic32_clearmask_getold(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv & ~i, oldv);
}

static __inline int64_t
_pfl_gen_atomic64_clearmask_getold(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv & ~i, oldv);
}

static __inline int16_t
_pfl_gen_atomic16_setmask_getold(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv | i, oldv);
}

static __inline int32_t
_pfl_gen_atomic32_setmask_getold(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv | i, oldv);
}

static __inline int64_t
_pfl_gen_atomic64_setmask_getold(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv | i, oldv);
}

static __inline void
_pfl_gen_atomic16_add(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic16, int16_t, v, oldv, newv, oldv + i);
}

static __inline void
_pfl_gen_atomic32_add(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic32, int32_t, v, oldv, newv, oldv + i);
}

static __inline void
_pfl_gen_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic64, int64_t, v, oldv, newv, oldv + i);
}

static __inline void
_pfl_gen_atomic16_sub(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic16, int16_t, v, oldv, newv, oldv - i);
}

static __inline void
_pfl_gen_atomic32_sub(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic32, int32_t, v, oldv, newv, oldv - i);
}

static __inline void
_pfl_gen_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic64, int64_t, v, oldv, newv, oldv - i);
}

static __inline int
_pfl_gen_atomic16_sub_and_test0(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv - i, newv == 0);
}

static __inline int
_pfl_gen_atomic32_sub_and_test0(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv - i, newv == 0);
}

static __inline int
_pfl_gen_atomic64_sub_and_test0(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv - i, newv == 0);
}

static __inline int
_pfl_gen_atomic16_add_and_test0(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv + i, newv == 0);
}

static __inline int
_pfl_gen_atomic32_add_and_test0(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv + i, newv == 0);
}

static __inline int
_pfl_gen_atomic64_add_and_test0(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv + i, newv == 0);
}

static __inline void
_pfl_gen_atomic16_inc(psc_atomic16_t *v)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic16, int16_t, v, oldv, newv, oldv + 1);
}

static __inline void
_pfl_gen_atomic32_inc(psc_atomic32_t *v)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic32, int32_t, v, oldv, newv, oldv + 1);
}

static __inline void
_pfl_gen_atomic64_inc(psc_atomic64_t *v)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic64, int64_t, v, oldv, newv, oldv + 1);
}

static __inline void
_pfl_gen_atomic16_dec(psc_atomic16_t *v)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic16, int16_t, v, oldv, newv, oldv - 1);
}

static __inline void
_pfl_gen_atomic32_dec(psc_atomic32_t *v)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic32, int32_t, v, oldv, newv, oldv - 1);
}

static __inline void
_pfl_gen_atomic64_dec(psc_atomic64_t *v)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic64, int64_t, v, oldv, newv, oldv - 1);
}

static __inline int
_pfl_gen_atomic16_inc_and_test0(psc_atomic16_t *v)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv + 1, newv == 0);
}

static __inline int
_pfl_gen_atomic32_inc_and_test0(psc_atomic32_t *v)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv + 1, newv == 0);
}

static __inline int
_pfl_gen_atomic64_inc_and_test0(psc_atomic64_t *v)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv + 1, newv == 0);
}

static __inline int
_pfl_gen_atomic16_dec_and_test0(psc_atomic16_t *v)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv - 1, newv == 0);
}

static __inline int
_pfl_gen_atomic32_dec_and_test0(psc_atomic32_t *v)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv - 1, newv == 0);
}

static __inline int
_pfl_gen_atomic64_dec_and_test0(psc_atomic64_t *v)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv - 1, newv == 0);
}

static __inline int
_pfl_gen_atomic16_add_and_test_neg(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv + i, newv < 0);
}

static __inline int
_pfl_gen_atomic32_add_and_test_neg(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv + i, newv < 0);
}

static __inline int
_pfl_gen_atomic64_add_and_test_neg(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv + i, newv < 0);
}

static __inline int
_pfl_gen_atomic16_sub_and_test_neg(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv - i, newv < 0);
}

static __inline int
_pfl_gen_atomic32_sub_and_test_neg(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv - i, newv < 0);
}

static __inline int
_pfl_gen_atomic64_sub_and_test_neg(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv - i, newv < 0);
}

static __inline int16_t
_pfl_gen_atomic16_add_getnew(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv + i, newv);
}

static __inline int32_t
_pfl_gen_atomic32_add_getnew(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv + i, newv);
}

static __inline int64_t
_pfl_gen_atomic64_add_getnew(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv + i, newv);
}

static __inline void
_pfl_gen_atomic16_clearmask(psc_atomic16_t *v, int16_t mask)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic16, int16_t, v, oldv, newv, oldv & ~mask);
}

static __inline void
_pfl_gen_atomic32_clearmask(psc_atomic32_t *v, int32_t mask)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic32, int32_t, v, oldv, newv, oldv & ~mask);
}

static __inline void
_pfl_gen_atomic64_clearmask(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic64, int64_t, v, oldv, newv, oldv & ~mask);
}

static __inline void
_pfl_gen_atomic16_setmask(psc_atomic16_t *v, int16_t mask)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic16, int16_t, v, oldv, newv, oldv | mask);
}

static __inline void
_pfl_gen_atomic32_setmask(psc_atomic32_t *v, int32_t mask)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic32, int32_t, v, oldv, newv, oldv | mask);
}

static __inline void
_pfl_gen_atomic64_setmask(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ATOMIC_OPLOOPV(psc_atomic64, int64_t, v, oldv, newv, oldv | mask);
}

static __inline int16_t
_pfl_gen_atomic16_clearmask_getnew(psc_atomic16_t *v, int16_t mask)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv & ~mask, newv);
}

static __inline int32_t
_pfl_gen_atomic32_clearmask_getnew(psc_atomic32_t *v, int32_t mask)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv & ~mask, newv);
}

static __inline int64_t
_pfl_gen_atomic64_clearmask_getnew(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv & ~mask, newv);
}

static __inline int16_t
_pfl_gen_atomic16_setmask_getnew(psc_atomic16_t *v, int16_t mask)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, oldv | mask, newv);
}

static __inline int32_t
_pfl_gen_atomic32_setmask_getnew(psc_atomic32_t *v, int32_t mask)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, oldv | mask, newv);
}

static __inline int64_t
_pfl_gen_atomic64_setmask_getnew(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, oldv | mask, newv);
}

static __inline int16_t
_pfl_gen_atomic16_xchg(psc_atomic16_t *v, int16_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic16, int16_t, v, oldv, newv, i, oldv);
}

static __inline int32_t
_pfl_gen_atomic32_xchg(psc_atomic32_t *v, int32_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic32, int32_t, v, oldv, newv, i, oldv);
}

static __inline int64_t
_pfl_gen_atomic64_xchg(psc_atomic64_t *v, int64_t i)
{
	_PFL_ATOMIC_OPLOOP(psc_atomic64, int64_t, v, oldv, newv, i, oldv);
}
