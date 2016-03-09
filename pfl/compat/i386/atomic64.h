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

#include "pfl/lock.h"

#ifdef HAVE_I386_CMPXCHG8B
# define PFL_ATOMIC64_LOCK(v)
# define PFL_ATOMIC64_UNLOCK(v)

# define PSC_ATOMIC64_INIT(i)		{ i }
#else
# define PFL_ATOMIC64_LOCK(v)		spinlock(&(v)->lock64)
# define PFL_ATOMIC64_UNLOCK(v)		freelock(&(v)->lock64)

struct psc_i386_atomic64 {
	volatile int64_t		value64;
	psc_spinlock_t			lock64;
} __packed;

# undef psc_atomic64_t
# define psc_atomic64_t			struct psc_i386_atomic64
# define PSC_ATOMIC64_INIT(i)		{ (i), SPINLOCK_INIT }

# undef psc_atomic64_init
static __inline void
psc_atomic64_init(psc_atomic64_t *v)
{
	INIT_SPINLOCK(&v->lock64);
}
#endif

#define _PFL_GETA64(v)			((v)->value64)

#undef psc_atomic64_read
static __inline int64_t
psc_atomic64_read(psc_atomic64_t *v)
{
	uint64_t val;

	PFL_ATOMIC64_LOCK(v);
	val = _PFL_GETA64(v);
	PFL_ATOMIC64_UNLOCK(v);
	return (val);
}

#undef psc_atomic64_set
static __inline void
psc_atomic64_set(psc_atomic64_t *v, int64_t i)
{
	PFL_ATOMIC64_LOCK(v);
	_PFL_GETA64(v) = i;
	PFL_ATOMIC64_UNLOCK(v);
}

#if 0
#undef psc_atomic64_add
static __inline void
psc_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("addq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

#undef psc_atomic64_sub
static __inline void
psc_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("subq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

#undef psc_atomic64_sub_and_test0
static __inline int
psc_atomic64_sub_and_test0(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("subq %2, %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic64_add_and_test0
static __inline int
psc_atomic64_add_and_test0(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("addq %2, %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic64_inc
static __inline void
psc_atomic64_inc(psc_atomic64_t *v)
{
	_PFL_ASM("incq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}

#undef psc_atomic64_dec
static __inline void
psc_atomic64_dec(psc_atomic64_t *v)
{
	_PFL_ASM("decq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}

#undef psc_atomic64_inc_and_test0
static __inline int
psc_atomic64_inc_and_test0(psc_atomic64_t *v)
{
	unsigned char c;

	_PFL_ASM("incq %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic64_dec_and_test0
static __inline int
psc_atomic64_dec_and_test0(psc_atomic64_t *v)
{
	unsigned char c;

	_PFL_ASM("decq %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic64_add_and_test_neg
static __inline int
psc_atomic64_add_and_test_neg(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("addq %2, %0; sets %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic64_sub_and_test_neg
static __inline int
psc_atomic64_sub_and_test_neg(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("subq %2, %0; sets %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic64_add_getnew
static __inline int64_t
psc_atomic64_add_getnew(psc_atomic64_t *v, int64_t i)
{
	int64_t adj = i;

	_PFL_ASM("xaddq %0, %1" : "+r" (i),
	    "+m" _PFL_GETA64(v) : : "memory");
	return (i + adj);
}

#undef psc_atomic64_clearmask
static __inline void
psc_atomic64_clearmask(psc_atomic64_t *v, int64_t mask)
{
	mask = ~mask;
	_PFL_ASM("andq %0, %1" : : "r" (mask),
	    "m" _PFL_GETA64(v) : "memory");
}

#undef psc_atomic64_setmask
static __inline void
psc_atomic64_setmask(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ASM("orq %0, %1" : : "r" (mask),
	    "m" _PFL_GETA64(v) : "memory");
}

#undef psc_atomic64_clearmask_getnew
static __inline int64_t
psc_atomic64_clearmask_getnew(psc_atomic64_t *v, int64_t mask)
{
	int64_t oldv = mask;

	mask = ~mask;
	_PFL_ASM("andq %0, %1;" : "=r" (mask)
	    : "m" _PFL_GETA64(v), "0" (oldv));
	return (oldv & ~mask);
}

#undef psc_atomic64_setmask_getnew
static __inline int64_t
psc_atomic64_setmask_getnew(psc_atomic64_t *v, int64_t i)
{
	int64_t oldv = i;

	_PFL_ASM("orq %0, %1;" : "=r" (i)
	    : "m" _PFL_GETA64(v), "0" (oldv));
	return (oldv | i);
}

#undef psc_atomic64_xchg
static __inline int64_t
psc_atomic64_xchg(psc_atomic64_t *v, int64_t i)
{
	_PFL_NL_ASM("xchgq %0, %1" : "=r" (i)
	    : "m" _PFL_GETA64(v), "0" (i) : "memory");
	return (i);
}
#endif

static __inline int64_t
psc_atomic64_cmpxchg(psc_atomic64_t *v, int64_t cmpv, int64_t newv)
{
	int64_t oldv;

#if HAVE_I386_CMPXCHG8B
	_PFL_ASM("cmpxchg8b %3" : "=A" (oldv) : "b" ((int32_t)newv),
	    "c" ((uint64_t)newv >> 32), "m" _PFL_GETA64(v),
	    "0" (cmpv) : "memory");
#else
	PFL_ATOMIC64_LOCK(v);
	oldv = _PFL_GETA64(v);
	if (oldv == cmpv)
		_PFL_GETA64(v) = newv;
	PFL_ATOMIC64_UNLOCK(v);
#endif
	return (oldv);
}
