/* $Id$ */

#ifndef _PFL_ATOMIC_H_
#define _PFL_ATOMIC_H_

#include <sys/types.h>
#include <asm/bitops.h>
#include <asm/system.h>

#include <stdint.h>

#ifdef __ia64

#include "psc_util/log.h"

#define __bad_increment_for_ia64_fetch_and_add() ({ psc_fatalx("__bad_increment_for_ia64_fetch_and_add"); 0; })
#define __bad_size_for_ia64_fetch_and_add()	 psc_fatalx("__bad_size_for_ia64_fetch_and_add")
#define ia64_cmpxchg_called_with_bad_pointer()	 ({ psc_fatalx("ia64_cmpxchg_called_with_bad_pointer"); 0; })
#define ia64_xchg_called_with_bad_pointer()	 psc_fatalx("ia64_xchg_called_with_bad_pointer")

typedef struct { volatile __s32 value; } atomic_t;
typedef struct { volatile __s16 value; } psc_atomic16_t;
typedef struct { volatile __s32 value; } psc_atomic32_t;
typedef struct { volatile __s64 value; } psc_atomic64_t;

#define ATOMIC_INIT(i)				{ (i) }
#define PSC_ATOMIC16_INIT(i)			{ (i) }
#define PSC_ATOMIC32_INIT(i)			{ (i) }
#define PSC_ATOMIC64_INIT(i)			{ (i) }

#define atomic_read(v)				((v)->value)
#define psc_atomic16_read(v)			((v)->value)
#define psc_atomic32_read(v)			((v)->value)
#define psc_atomic64_read(v)			((v)->value)

#define atomic_set(v, i)			(((v)->value) = (i))
#define psc_atomic16_set(v, i)			(((v)->value) = (i))
#define psc_atomic32_set(v, i)			(((v)->value) = (i))
#define psc_atomic64_set(v, i)			(((v)->value) = (i))

static __inline int
ia64_atomic_add(int i, atomic_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic_read(v);
		new = old + i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(atomic_t)) != old);
	return new;
}

static __inline int
ia64_atomic64_add(__s64 i, psc_atomic64_t *v)
{
	__s64 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic64_read(v);
		new = old + i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(psc_atomic64_t)) != old);
	return new;
}

static __inline int
ia64_atomic_sub(int i, atomic_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic_read(v);
		new = old - i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(atomic_t)) != old);
	return new;
}

static __inline int
ia64_atomic64_sub(__s64 i, psc_atomic64_t *v)
{
	__s64 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic64_read(v);
		new = old - i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(psc_atomic64_t)) != old);
	return new;
}

#define atomic_add_return(i, v)						\
({									\
	int __ia64_aar_i = (i);						\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->value)		\
		: ia64_atomic_add(__ia64_aar_i, (v));			\
})

#define psc_atomic32_add_return(v, i)					\
({									\
	int __ia64_aar_i = (i);						\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->value)		\
		: ia64_atomic_add(__ia64_aar_i, (v));			\
})

#define psc_atomic64_add_return(v, i)					\
({									\
	long __ia64_aar_i = (i);					\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->value)		\
		: ia64_atomic64_add(__ia64_aar_i, (v));			\
})

#define atomic_sub_return(i, v)						\
({									\
	int __ia64_asr_i = (i);						\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->value)	\
		: ia64_atomic_sub(__ia64_asr_i, (v));			\
})

#define psc_atomic32_sub_return(v, i)					\
({									\
	int __ia64_asr_i = (i);						\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->value)	\
		: ia64_atomic_sub(__ia64_asr_i, (v));			\
})

#define psc_atomic64_sub_return(v, i)					\
({									\
	long __ia64_asr_i = (i);					\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->value)	\
		: ia64_atomic64_sub(__ia64_asr_i, (v));			\
})

#define psc_atomic64_dec_return(v)		psc_atomic64_sub_return((v), 1)
#define psc_atomic64_inc_return(v)		psc_atomic64_add_return((v), 1)

#define atomic_dec_and_test(v)			(atomic_sub_return(1, (v)) == 0)
#define psc_atomic32_dec_test_zero(v)		(psc_atomic32_sub_return((v), 1) == 0)
#define psc_atomic64_dec_test_zero(v)		(psc_atomic64_sub_return((v), 1) == 0)

#define atomic_add(i, v)			atomic_add_return((i), (v))
#define atomic_sub(i, v)			atomic_sub_return((i), (v))
#define atomic_inc(v)				atomic_add(1, (v))
#define atomic_dec(v)				atomic_sub(1, (v))

#define psc_atomic32_add(v, i)			psc_atomic32_add_return((v), (i))
#define psc_atomic32_sub(v, i)			psc_atomic32_sub_return((v), (i))
#define psc_atomic32_inc(v)			psc_atomic32_add((v), 1)
#define psc_atomic32_dec(v)			psc_atomic32_sub((v), 1)

#define psc_atomic64_add(v, i)			psc_atomic64_add_return((v), (i))
#define psc_atomic64_sub(v, i)			psc_atomic64_sub_return((v), (i))
#define psc_atomic64_inc(v)			psc_atomic64_add((v), 1)
#define psc_atomic64_dec(v)			psc_atomic64_sub((v), 1)

#define psc_atomic_genaf(name, readf, atype, vtype, op, ...)		\
static __inline vtype							\
name(atype *v, ## __VA_ARGS__)						\
{									\
	vtype oldval, newval;						\
	CMPXCHG_BUGCHECK_DECL						\
									\
	do {								\
		CMPXCHG_BUGCHECK(v);					\
		oldval = readf(v);					\
		newval = op;						\
	} while (ia64_cmpxchg(acq, v, oldval,				\
	    newval, sizeof(vtype)) != oldval);				\
	return (newval);						\
}

psc_atomic_genaf(atomic_clear_mask, atomic_read, atomic_t, uint32_t, oldval & ~mask, uint32_t mask)
psc_atomic_genaf(atomic_set_mask, atomic_read, atomic_t, uint32_t, oldval | mask, uint32_t mask)
psc_atomic_genaf(psc_atomic32_clear_mask, psc_atomic32_read, psc_atomic32_t, uint32_t, oldval & ~mask, uint32_t mask)
psc_atomic_genaf(psc_atomic32_set_mask, psc_atomic32_read, psc_atomic32_t, uint32_t, oldval | mask, uint32_t mask)
psc_atomic_genaf(psc_atomic64_clear_mask, psc_atomic64_read, psc_atomic64_t, uint64_t, oldval & ~mask, uint64_t mask)
psc_atomic_genaf(psc_atomic64_set_mask, psc_atomic64_read, psc_atomic64_t, uint64_t, oldval | mask, uint64_t mask)

#else

#ifdef LOCK_PREFIX
#undef LOCK_PREFIX
#endif
#define LOCK_PREFIX "lock; "

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct { volatile int value; } atomic_t;
typedef struct { volatile int16_t value; } psc_atomic16_t;
typedef struct { volatile int32_t value; } psc_atomic32_t;
typedef struct { volatile int64_t value; } psc_atomic64_t;

#define ATOMIC_INIT(i)			{ (i) }
#define PSC_ATOMIC16_INIT(i)		{ (i) }
#define PSC_ATOMIC32_INIT(i)		{ (i) }
#define PSC_ATOMIC64_INIT(i)		{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic_read(v)			((v)->value)
#define psc_atomic16_read(v)		((v)->value)
#define psc_atomic32_read(v)		((v)->value)
#define psc_atomic64_read(v)		((v)->value)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
#define atomic_set(v, i)		(((v)->value) = (i))
#define psc_atomic16_set(v, i)		(((v)->value) = (i))
#define psc_atomic32_set(v, i)		(((v)->value) = (i))
#define psc_atomic64_set(v, i)		(((v)->value) = (i))

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v.
 */
static __inline void
atomic_add(int i, atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addl %1,%0"
		:"=m" (v->value)
		:"ir" (i), "m" (v->value));
}

static __inline void
psc_atomic32_add(psc_atomic32_t *v, int32_t i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addl %1,%0"
		:"=m" (v->value)
		:"ir" (i), "m" (v->value));
}

/**
 * psc_atomic64_add - add integer to atomic variable
 * @i: int64_t value to add
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically adds @i to @v.
 */
static __inline void
psc_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addq %1,%0"
		:"=m" (v->value)
		:"ir" (i), "m" (v->value));
}

/**
 * atomic_sub - subtract the atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v.
 */
static __inline void
atomic_sub(int i, atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subl %1,%0"
		:"=m" (v->value)
		:"ir" (i), "m" (v->value));
}

static __inline void
psc_atomic32_sub(psc_atomic32_t *v, int32_t i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subl %1,%0"
		:"=m" (v->value)
		:"ir" (i), "m" (v->value));
}

/**
 * psc_atomic64_sub - subtract the atomic variable
 * @i: int64_t value to subtract
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically subtracts @i from @v.
 */
static __inline void
psc_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subq %1,%0"
		:"=m" (v->value)
		:"ir" (i), "m" (v->value));
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __inline int
atomic_sub_and_test(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subl %2,%0; sete %1"
		:"=m" (v->value), "=qm" (c)
		:"ir" (i), "m" (v->value) : "memory");
	return c;
}

static __inline int
psc_atomic32_sub_test_zero(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subl %2,%0; sete %1"
		:"=m" (v->value), "=qm" (c)
		:"ir" (i), "m" (v->value) : "memory");
	return c;
}

/**
 * psc_atomic64_sub_test_zero - subtract value from variable and test result
 * @i: int64_t value to subtract
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __inline int
psc_atomic64_sub_test_zero(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subq %2,%0; sete %1"
		:"=m" (v->value), "=qm" (c)
		:"ir" (i), "m" (v->value) : "memory");
	return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1.
 */
static __inline void
atomic_inc(atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incl %0"
		:"=m" (v->value)
		:"m" (v->value));
}

static __inline void
psc_atomic32_inc(psc_atomic32_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incl %0"
		:"=m" (v->value)
		:"m" (v->value));
}

/**
 * psc_atomic64_inc - Atomically increment by one.
 * @v: atomic value.
 */
static __inline void
psc_atomic64_inc(psc_atomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incq %0"
		: "=m" (v->value)
		: "m" (v->value));
}

static __inline void
atomic_dec(atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decl %0"
		:"=m" (v->value)
		:"m" (v->value));
}

/**
 * psc_atomic32_dec - Atomically decrement by one.
 * @v: atomic value.
 */
static __inline void
psc_atomic32_dec(psc_atomic32_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decl %0"
		: "=m" (v->value)
		: "m" (v->value));
}

/**
 * psc_atomic64_dec - Atomically decrement by one.
 * @v: atomic value.
 */
static __inline void
psc_atomic64_dec(psc_atomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decq %0"
		: "=m" (v->value)
		: "m" (v->value));
}

static __inline int
atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decl %0; sete %1"
		:"=m" (v->value), "=qm" (c)
		:"m" (v->value) : "memory");
	return c != 0;
}

/**
 * psc_atomic32_dec_test_zero - Atomically decrement and check
 *	if new value is zero.
 * @v: atomic value.
 */
static __inline int
psc_atomic32_dec_test_zero(psc_atomic32_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decl %0; sete %1"
		: "=m" (v->value), "=qm" (c)
		: "m" (v->value) : "memory");
	return (c != 0);
}

/**
 * psc_atomic64_dec_test_zero - Atomically decrement and check
 *	if new value is zero.
 * @v: atomic value.
 */
static __inline int
psc_atomic64_dec_test_zero(psc_atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decq %0; sete %1"
		: "=m" (v->value), "=qm" (c)
		: "m" (v->value) : "memory");
	return (c != 0);
}

static __inline int
atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incl %0; sete %1"
		:"=m" (v->value), "=qm" (c)
		:"m" (v->value) : "memory");
	return c != 0;
}

/**
 * psc_atomic32_inc_test_zero - Atomically increment and check
 *	if new value is zero.
 * @v: atomic value.
 */
static __inline int
psc_atomic32_inc_test_zero(psc_atomic32_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incl %0; sete %1"
		: "=m" (v->value), "=qm" (c)
		: "m" (v->value) : "memory");
	return (c != 0);
}

/**
 * psc_atomic64_inc_test_zero - Atomically increment and check
 *	if new value is zero.
 * @v: atomic value.
 */
static __inline int
psc_atomic64_inc_test_zero(psc_atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incq %0; sete %1"
		: "=m" (v->value), "=qm" (c)
		: "m" (v->value) : "memory");
	return (c != 0);
}

static __inline int
atomic_add_negative(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addl %2,%0; sets %1"
		:"=m" (v->value), "=qm" (c)
		:"ir" (i), "m" (v->value) : "memory");
	return c;
}

/**
 * psc_atomic32_add_test_neg - Atomically add and test if result is
 *	negative.
 * @v: atomic value to add to.
 * @i: value to add.
 */
static __inline int
psc_atomic32_add_test_neg(psc_atomic32_t *v, int i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addl %2,%0; sets %1"
		: "=m" (v->value), "=qm" (c)
		: "ir" (i), "m" (v->value) : "memory");
	return (c);
}

/**
 * psc_atomic64_add_test_neg - Atomically add and test if result is
 *	negative.
 * @v: atomic value to add to.
 * @i: value to add.
 */
static __inline int
psc_atomic64_add_test_neg(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addq %2,%0; sets %1"
		:"=m" (v->value), "=qm" (c)
		:"ir" (i), "m" (v->value) : "memory");
	return (c);
}

static __inline int
atomic_add_return(int i, atomic_t *v)
{
	int __i = i;

	__asm__ __volatile__(
		LOCK_PREFIX "xaddl %0, %1"
		: "+r" (i), "+m" (v->value)
		: : "memory");
	return (i + __i);
}

/**
 * psc_atomic32_add_return - Atomically add @i to @v and return @i + @v.
 * @v: atomic value
 * @i: int32_t value to add
 */
static __inline int
psc_atomic32_add_return(psc_atomic32_t *v, int32_t i)
{
	int __i = i;

	__asm__ __volatile__(
		LOCK_PREFIX "xaddl %0, %1"
		: "+r" (i), "+m" (v->value)
		: : "memory");
	return (i + __i);
}

/**
 * psc_atomic64_add_return - Atomically add @i to @v and return @i + @v.
 * @v: atomic value
 * @i: int64_t value to add
 */
static __inline int64_t
psc_atomic64_add_return(psc_atomic64_t *v, int64_t i)
{
	int64_t __i = i;

	__asm__ __volatile__(
		LOCK_PREFIX "xaddq %0, %1;"
		: "=r" (i)
		: "m" (v->value), "0" (i));
	return (i + __i);
}

#define atomic_sub_return(i, v)		atomic_add_return(-(i), (v))
#define psc_atomic16_sub_return(v, i)	psc_atomic16_add_return((v), -(i))
#define psc_atomic32_sub_return(v, i)	psc_atomic32_add_return((v), -(i))
#define psc_atomic64_sub_return(v, i)	psc_atomic64_add_return((v), -(i))

static __inline void
atomic_clear_mask(int32_t mask, atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "andl %0,%1"
		: : "r" (~(mask)),
		    "m" (v->value) : "memory");
}

static __inline void
psc_atomic32_clear_mask(psc_atomic32_t *v, int32_t mask)
{
	__asm__ __volatile__(
		LOCK_PREFIX "andl %0,%1"
		: : "r" (~(mask)),
		    "m" (v->value) : "memory");
}

static __inline void
atomic_set_mask(int32_t mask, atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "orl %0,%1"
		: : "r" (mask),
		    "m" (v->value) : "memory");
}

static __inline void
psc_atomic32_set_mask(psc_atomic32_t *v, int32_t mask)
{
	__asm__ __volatile__(
		LOCK_PREFIX "orl %0,%1"
		: : "r" (mask),
		    "m" (v->value) : "memory");
}

static __inline int64_t
psc_atomic64_set_mask(psc_atomic64_t *v, int64_t mask)
{
	int64_t newmask = mask;

	__asm__ __volatile__(
		LOCK_PREFIX "orq %0, %1;"
		:"=r"(mask)
		:"m"(v->value), "0"(newmask));
	return (newmask | mask);
}

#endif /* __ia64 */

#define atomic_dec_return(v)			atomic_sub_return(1, (v))
#define atomic_inc_return(v)			atomic_add_return(1, (v))

#define psc_atomic32_dec_return(v)		psc_atomic32_sub_return((v), 1)
#define psc_atomic32_inc_return(v)		psc_atomic32_add_return((v), 1)

#define atomic_cmpxchg(v, old, new)		((int)cmpxchg(&((v)->value), (old), (new)))
#define atomic_xchg(v, new)			(xchg(&((v)->value), (new)))
#define psc_atomic16_cmpxchg(v, old, new)	((int16_t)cmpxchg(&((v)->value), (old), (new)))
#define psc_atomic16_xchg(v, new)		(xchg(&((v)->value), (new)))
#define psc_atomic32_cmpxchg(v, old, new)	((int32_t)cmpxchg(&((v)->value), (old), (new)))
#define psc_atomic32_xchg(v, new)		(xchg(&((v)->value), (new)))
#define psc_atomic64_cmpxchg(v, old, new)	((int64_t)cmpxchg(&((v)->value), (old), (new)))
#define psc_atomic64_xchg(v, new)		(xchg(&((v)->value), (new)))

static inline int16_t
psc_atomic16_setmask_ret(psc_atomic16_t *v, int16_t mask)
{
	int16_t oldval, newval;

	do {
		oldval = psc_atomic16_read(v);
		newval = oldval | mask;
	} while (psc_atomic16_cmpxchg(v, oldval, newval) != oldval);
	return (oldval);
}

static inline int16_t
psc_atomic16_testmaskset(psc_atomic16_t *v, int16_t mask, int16_t newval)
{
	int16_t oldval;

	oldval = psc_atomic16_read(v);
	if (oldval & mask)
		return (0);
	if (psc_atomic16_cmpxchg(v, oldval, newval) != oldval)
		return (0);
	return (1);
}

#endif /* _PFL_ATOMIC_H_ */
