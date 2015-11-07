/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#define _PFL_ASM(code, ...)		__asm__ __volatile__("lock; " code, ## __VA_ARGS__)
#define _PFL_NL_ASM(code, ...)		__asm__ __volatile__(code, ## __VA_ARGS__)

#define PSC_ATOMIC16_INIT(i)		{ i }
#define PSC_ATOMIC32_INIT(i)		{ i }
#define PSC_ATOMIC64_INIT(i)		{ i }

#define _PFL_GETA16(v)			((v)->value16)
#define _PFL_GETA32(v)			((v)->value32)
#define _PFL_GETA64(v)			((v)->value64)

#undef psc_atomic16_init
static __inline void
psc_atomic16_init(__unusedx psc_atomic16_t *v)
{
}

#undef psc_atomic32_init
static __inline void
psc_atomic32_init(__unusedx psc_atomic32_t *v)
{
}

#undef psc_atomic64_init
static __inline void
psc_atomic64_init(__unusedx psc_atomic64_t *v)
{
}

#undef psc_atomic16_read
static __inline int16_t
psc_atomic16_read(psc_atomic16_t *v)
{
	return (_PFL_GETA16(v));
}

#undef psc_atomic32_read
static __inline int32_t
psc_atomic32_read(psc_atomic32_t *v)
{
	return (_PFL_GETA32(v));
}

#undef psc_atomic64_read
static __inline int64_t
psc_atomic64_read(const psc_atomic64_t *v)
{
	return (_PFL_GETA64(v));
}

#undef psc_atomic16_set
static __inline void
psc_atomic16_set(psc_atomic16_t *v, int16_t i)
{
	_PFL_GETA16(v) = i;
}

#undef psc_atomic32_set
static __inline void
psc_atomic32_set(psc_atomic32_t *v, int32_t i)
{
	_PFL_GETA32(v) = i;
}

#undef psc_atomic64_set
static __inline void
psc_atomic64_set(psc_atomic64_t *v, int64_t i)
{
	_PFL_GETA64(v) = i;
}

#undef psc_atomic16_add
static __inline void
psc_atomic16_add(psc_atomic16_t *v, int16_t i)
{
	_PFL_ASM("addw %1,%0" : "=m" _PFL_GETA16(v) : "ir" (i), "m" _PFL_GETA16(v));
}

#undef psc_atomic32_add
static __inline void
psc_atomic32_add(psc_atomic32_t *v, int32_t i)
{
	_PFL_ASM("addl %1,%0" : "=m" _PFL_GETA32(v) : "ir" (i), "m" _PFL_GETA32(v));
}

#undef psc_atomic64_add
static __inline void
psc_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("addq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

#undef psc_atomic16_sub
static __inline void
psc_atomic16_sub(psc_atomic16_t *v, int16_t i)
{
	_PFL_ASM("subw %1,%0" : "=m" _PFL_GETA16(v) : "ir" (i), "m" _PFL_GETA16(v));
}

#undef psc_atomic32_sub
static __inline void
psc_atomic32_sub(psc_atomic32_t *v, int32_t i)
{
	_PFL_ASM("subl %1,%0" : "=m" _PFL_GETA32(v) : "ir" (i), "m" _PFL_GETA32(v));
}

#undef psc_atomic64_sub
static __inline void
psc_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("subq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

#undef psc_atomic16_sub_and_test0
static __inline int
psc_atomic16_sub_and_test0(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("subw %2, %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_sub_and_test0
static __inline int
psc_atomic32_sub_and_test0(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("subl %2, %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
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

#undef psc_atomic16_add_and_test0
static __inline int
psc_atomic16_add_and_test0(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("addw %2, %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_add_and_test0
static __inline int
psc_atomic32_add_and_test0(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("addl %2, %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
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

#undef psc_atomic16_inc
static __inline void
psc_atomic16_inc(psc_atomic16_t *v)
{
	_PFL_ASM("incw %0" : "=m" _PFL_GETA16(v) : "m" _PFL_GETA16(v));
}

#undef psc_atomic32_inc
static __inline void
psc_atomic32_inc(psc_atomic32_t *v)
{
	_PFL_ASM("incl %0" : "=m" _PFL_GETA32(v) : "m" _PFL_GETA32(v));
}

#undef psc_atomic64_inc
static __inline void
psc_atomic64_inc(psc_atomic64_t *v)
{
	_PFL_ASM("incq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}

#undef psc_atomic16_dec
static __inline void
psc_atomic16_dec(psc_atomic16_t *v)
{
	_PFL_ASM("decw %0" : "=m" _PFL_GETA16(v) : "m" _PFL_GETA16(v));
}

#undef psc_atomic32_dec
static __inline void
psc_atomic32_dec(psc_atomic32_t *v)
{
	_PFL_ASM("decl %0" : "=m" _PFL_GETA32(v) : "m" _PFL_GETA32(v));
}

#undef psc_atomic64_dec
static __inline void
psc_atomic64_dec(psc_atomic64_t *v)
{
	_PFL_ASM("decq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}

#undef psc_atomic16_inc_and_test0
static __inline int
psc_atomic16_inc_and_test0(psc_atomic16_t *v)
{
	unsigned char c;

	_PFL_ASM("incw %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_inc_and_test0
static __inline int
psc_atomic32_inc_and_test0(psc_atomic32_t *v)
{
	unsigned char c;

	_PFL_ASM("incl %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "m" _PFL_GETA32(v) : "memory");
	return (c);
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

#undef psc_atomic16_dec_and_test0
static __inline int
psc_atomic16_dec_and_test0(psc_atomic16_t *v)
{
	unsigned char c;

	_PFL_ASM("decw %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_dec_and_test0
static __inline int
psc_atomic32_dec_and_test0(psc_atomic32_t *v)
{
	unsigned char c;

	_PFL_ASM("decl %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "m" _PFL_GETA32(v) : "memory");
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

#undef psc_atomic16_add_and_test_neg
static __inline int
psc_atomic16_add_and_test_neg(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("addw %2, %0; sets %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_add_and_test_neg
static __inline int
psc_atomic32_add_and_test_neg(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("addl %2, %0; sets %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
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

#undef psc_atomic16_sub_and_test_neg
static __inline int
psc_atomic16_sub_and_test_neg(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("subw %2, %0; sets %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_sub_and_test_neg
static __inline int
psc_atomic32_sub_and_test_neg(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("subl %2, %0; sets %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
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

#undef psc_atomic16_add_getnew
static __inline int16_t
psc_atomic16_add_getnew(psc_atomic16_t *v, int16_t i)
{
	int16_t adj = i;

	_PFL_ASM("xaddw %0, %1" : "+r" (i),
	    "+m" _PFL_GETA16(v) : : "memory");
	return (i + adj);
}

#undef psc_atomic32_add_getnew
static __inline int32_t
psc_atomic32_add_getnew(psc_atomic32_t *v, int32_t i)
{
	int32_t adj = i;

	_PFL_ASM("xaddl %0, %1" : "+r" (i),
	    "+m" _PFL_GETA32(v) : : "memory");
	return (i + adj);
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

#undef psc_atomic16_clearmask
static __inline void
psc_atomic16_clearmask(psc_atomic16_t *v, int16_t mask)
{
	mask = ~mask;
	_PFL_ASM("andw %0, %1" : : "r" (mask),
	    "m" _PFL_GETA16(v) : "memory");
}

#undef psc_atomic32_clearmask
static __inline void
psc_atomic32_clearmask(psc_atomic32_t *v, int32_t mask)
{
	mask = ~mask;
	_PFL_ASM("andl %0, %1" : : "r" (mask),
	    "m" _PFL_GETA32(v) : "memory");
}

#undef psc_atomic64_clearmask
static __inline void
psc_atomic64_clearmask(psc_atomic64_t *v, int64_t mask)
{
	mask = ~mask;
	_PFL_ASM("andq %0, %1" : : "r" (mask),
	    "m" _PFL_GETA64(v) : "memory");
}

#undef psc_atomic16_setmask
static __inline void
psc_atomic16_setmask(psc_atomic16_t *v, int16_t mask)
{
	_PFL_ASM("orw %0, %1" : : "r" (mask),
	    "m" _PFL_GETA16(v) : "memory");
}

#undef psc_atomic32_setmask
static __inline void
psc_atomic32_setmask(psc_atomic32_t *v, int32_t mask)
{
	_PFL_ASM("orl %0, %1" : : "r" (mask),
	    "m" _PFL_GETA32(v) : "memory");
}

#undef psc_atomic64_setmask
static __inline void
psc_atomic64_setmask(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ASM("orq %0, %1" : : "r" (mask),
	    "m" _PFL_GETA64(v) : "memory");
}

#undef psc_atomic16_clearmask_getnew
static __inline int16_t
psc_atomic16_clearmask_getnew(psc_atomic16_t *v, int16_t mask)
{
	int16_t oldv = mask;

	mask = ~mask;
	_PFL_ASM("andw %0, %1;" : "=r" (mask)
	    : "m" _PFL_GETA16(v), "0" (oldv));
	return (oldv & ~mask);
}

#undef psc_atomic32_clearmask_getnew
static __inline int32_t
psc_atomic32_clearmask_getnew(psc_atomic32_t *v, int32_t mask)
{
	int32_t oldv = mask;

	mask = ~mask;
	_PFL_ASM("andl %0, %1;" : "=r" (mask)
	    : "m" _PFL_GETA32(v), "0" (oldv));
	return (oldv & ~mask);
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

#undef psc_atomic16_setmask_getnew
static __inline int16_t
psc_atomic16_setmask_getnew(psc_atomic16_t *v, int16_t i)
{
	int16_t oldv = i;

	_PFL_ASM("orw %0, %1;" : "=r" (i)
	    : "m" _PFL_GETA16(v), "0" (oldv));
	return (oldv | i);
}

#undef psc_atomic32_setmask_getnew
static __inline int32_t
psc_atomic32_setmask_getnew(psc_atomic32_t *v, int32_t i)
{
	int32_t oldv = i;

	_PFL_ASM("orl %0, %1;" : "=r" (i)
	    : "m" _PFL_GETA32(v), "0" (oldv));
	return (oldv | i);
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

#undef psc_atomic16_xchg
static __inline int16_t
psc_atomic16_xchg(psc_atomic16_t *v, int16_t i)
{
	_PFL_NL_ASM("xchgw %0, %1" : "=r" (i)
	    : "m" _PFL_GETA16(v), "0" (i) : "memory");
	return (i);
}

#undef psc_atomic32_xchg
static __inline int32_t
psc_atomic32_xchg(psc_atomic32_t *v, int32_t i)
{
	_PFL_NL_ASM("xchgl %0, %1" : "=r" (i)
	    : "m" _PFL_GETA32(v), "0" (i) : "memory");
	return (i);
}

#undef PSC_ATOMIC32_XCHG
#define PSC_ATOMIC32_XCHG(v, i)						\
	_PFL_RVSTART {							\
		int32_t _aval = (i);					\
									\
		_PFL_NL_ASM("xchgl %0, %1" : "=r" (_aval)		\
		    : "m" _PFL_GETA32(v), "0" (_aval) : "memory");	\
		_aval;							\
	} _PFL_RVEND

#undef psc_atomic64_xchg
static __inline int64_t
psc_atomic64_xchg(psc_atomic64_t *v, int64_t i)
{
	_PFL_NL_ASM("xchgq %0, %1" : "=r" (i)
	    : "m" _PFL_GETA64(v), "0" (i) : "memory");
	return (i);
}

static __inline int16_t
psc_atomic16_cmpxchg(psc_atomic16_t *v, int16_t cmpv, int16_t newv)
{
	int16_t oldv;

	_PFL_ASM("cmpxchgw %1, %2" : "=a" (oldv) : "r" (newv),
	    "m" _PFL_GETA16(v), "0" (cmpv) : "memory");
	return (oldv);
}

static __inline int32_t
psc_atomic32_cmpxchg(psc_atomic32_t *v, int32_t cmpv, int32_t newv)
{
	int32_t oldv;

	_PFL_ASM("cmpxchgl %k1,%2" : "=a" (oldv) : "r" (newv),
	    "m" _PFL_GETA32(v), "0" (cmpv) : "memory");
	return (oldv);
}

static __inline int64_t
psc_atomic64_cmpxchg(psc_atomic64_t *v, int64_t cmpv, int64_t newv)
{
	int64_t oldv;

	_PFL_ASM("cmpxchgq %1, %2" : "=a" (oldv) : "r" (newv),
	    "m" _PFL_GETA64(v), "0" (cmpv) : "memory");
	return (oldv);
}
