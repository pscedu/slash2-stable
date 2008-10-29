/* $Id$ */

#ifndef __ARCH_I386_ATOMIC__
#define __ARCH_I386_ATOMIC__

#include <sys/types.h>
#include <asm/bitops.h>
#include <asm/system.h>

#include <stdint.h>

#include "psc_types.h"

#ifdef __ia64

typedef struct { volatile __s32 counter; } atomic_t;
typedef struct { volatile __s64 counter; } psc_atomic64_t;

#define ATOMIC_INIT(i)				{ (i) }
#define PSC_ATOMIC64_INIT(i)			{ (i) }

#define atomic_read(v)				((v)->counter)
#define psc_atomic64_read(v)			((v)->counter)

#define atomic_set(v, i)			(((v)->counter) = (i))
#define psc_atomic64_set(v, i)			(((v)->counter) = (i))

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
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->counter)	\
		: ia64_atomic_add(__ia64_aar_i, (v));			\
})

#define psc_atomic64_add_return(i, v)					\
({									\
	long __ia64_aar_i = (i);					\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->counter)	\
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
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->counter)	\
		: ia64_atomic_sub(__ia64_asr_i, (v));			\
})

#define psc_atomic64_sub_return(i, v)					\
({									\
	long __ia64_asr_i = (i);					\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->counter)	\
		: ia64_atomic64_sub(__ia64_asr_i, (v));			\
})

#define atomic_dec_return(v)			atomic_sub_return(1, (v))
#define atomic_inc_return(v)			atomic_add_return(1, (v))
#define psc_atomic64_dec_return(v)		psc_atomic64_sub_return(1, (v))
#define psc_atomic64_inc_return(v)		psc_atomic64_add_return(1, (v))

#define atomic_dec_and_test(v)			(atomic_sub_return(1, (v)) == 0)

#define atomic_add(i, v)			atomic_add_return((i), (v))
#define atomic_sub(i, v)			atomic_sub_return((i), (v))
#define atomic_inc(v)				atomic_add(1, (v))
#define atomic_dec(v)				atomic_sub(1, (v))

#define psc_atomic64_add(i, v)			psc_atomic64_add_return((i), (v))
#define psc_atomic64_sub(i, v)			psc_atomic64_sub_return((i), (v))
#define psc_atomic64_inc(v)			psc_atomic64_add(1 ,(v))
#define psc_atomic64_dec(v)			psc_atomic64_sub(1 ,(v))

#define atomic_cmpxchg(v, old, new)		((int)cmpxchg(&((v)->counter), (old), (new)))
#define atomic_xchg(v, new)			(xchg(&((v)->counter), (new)))

#else

#ifdef LOCK_PREFIX
#undef LOCK_PREFIX
#endif
#define LOCK_PREFIX "lock ; "

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct { volatile int counter; } atomic_t;
typedef struct { volatile int64_t counter; } psc_atomic64_t;

#define ATOMIC_INIT(i)			{ (i) }
#define PSC_ATOMIC64_INIT(i)		{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#define atomic_read(v)			((v)->counter)
#define psc_atomic64_read(v)		((v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
#define atomic_set(v, i)		(((v)->counter) = (i))
#define psc_atomic64_set(v, i)		(((v)->counter) = (i))

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
		:"=m" (v->counter)
		:"ir" (i), "m" (v->counter));
}

/**
 * psc_atomic64_add - add integer to atomic variable
 * @i: int64_t value to add
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically adds @i to @v.
 */
static __inline void
psc_atomic64_add(int64_t i, psc_atomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addq %1,%0"
		:"=m" (v->counter)
		:"ir" (i), "m" (v->counter));
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
		:"=m" (v->counter)
		:"ir" (i), "m" (v->counter));
}

/**
 * psc_atomic64_sub - subtract the atomic variable
 * @i: int64_t value to subtract
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically subtracts @i from @v.
 */
static __inline void
psc_atomic64_sub(int64_t i, psc_atomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subq %1,%0"
		:"=m" (v->counter)
		:"ir" (i), "m" (v->counter));
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
		:"=m" (v->counter), "=qm" (c)
		:"ir" (i), "m" (v->counter) : "memory");
	return c;
}

/**
 * psc_atomic64_sub_and_test - subtract value from variable and test result
 * @i: int64_t value to subtract
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __inline int
psc_atomic64_sub_and_test(int64_t i, psc_atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subq %2,%0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"ir" (i), "m" (v->counter) : "memory");
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
		:"=m" (v->counter)
		:"m" (v->counter));
}

/**
 * psc_atomic64_inc - increment atomic variable
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically increments @v by 1.
 */
static __inline void
psc_atomic64_inc(psc_atomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incq %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.
 */
static __inline void
atomic_dec(atomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decl %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

/**
 * psc_atomic64_dec - decrement atomic variable
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically decrements @v by 1.
 */
static __inline void
psc_atomic64_dec(psc_atomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decq %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */
static __inline int
atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decl %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return c != 0;
}

/**
 * psc_atomic64_dec_and_test - decrement and test
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */
static __inline int
psc_atomic64_dec_and_test(psc_atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decq %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return c != 0;
}

/**
 * atomic_inc_and_test - increment and test
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
static __inline int
atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incl %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return c != 0;
}

/**
 * psc_atomic64_inc_and_test - increment and test
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
static __inline int
psc_atomic64_inc_and_test(psc_atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incq %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return c != 0;
}

/**
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 *
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */
static __inline int
atomic_add_negative(int i, atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addl %2,%0; sets %1"
		:"=m" (v->counter), "=qm" (c)
		:"ir" (i), "m" (v->counter) : "memory");
	return c;
}

/**
 * psc_atomic64_add_negative - add and test if negative
 * @v: pointer of type psc_atomic64_t
 * @i: int64_t value to add
 *
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */
static __inline int
psc_atomic64_add_negative(int64_t i, psc_atomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addq %2,%0; sets %1"
		:"=m" (v->counter), "=qm" (c)
		:"ir" (i), "m" (v->counter) : "memory");
	return c;
}

/**
 * atomic_add_return - add and return
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static __inline int
atomic_add_return(int i, atomic_t *v)
{
	int __i = i;

	__asm__ __volatile__(
		LOCK_PREFIX "xaddl %0, %1"
		:"+r" (i), "+m" (v->counter)
		: : "memory");
	return i + __i;
}

/**
 * psc_atomic64_add_return - add and return
 * @i: int64_t value to add
 * @v: pointer of type psc_atomic64_t
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static __inline int64_t
psc_atomic64_add_return(int64_t i, psc_atomic64_t *v)
{
	int64_t __i = i;

	__asm__ __volatile__(
		LOCK_PREFIX "xaddq %0, %1;"
		:"=r"(i)
		:"m"(v->counter), "0"(i));
	return i + __i;
}

#define atomic_sub_return(i, v)		atomic_add_return(-(i), (v))
#define psc_atomic64_sub_return(i, v)	psc_atomic64_add_return(-(i), (v))

#define atomic_cmpxchg(v, old, new)	((int)cmpxchg(&((v)->counter), (old), (new)))
#define atomic_xchg(v, new)		(xchg(&((v)->counter), (new)))

/**
 * atomic_add_unless - add unless the number is a given value
 * @v: pointer of type atomic_t
 * @a: the amount to add to v...
 * @u: ...unless v is equal to u.
 *
 * Atomically adds @a to @v, so long as it was not @u.
 * Returns non-zero if @v was not @u, and zero otherwise.
 */
#define atomic_add_unless(v, a, u)				\
({								\
	int c, old;						\
								\
	c = atomic_read(v);					\
	for (;;) {						\
		if (unlikely(c == (u)))				\
			break;					\
		old = atomic_cmpxchg((v), c, c + (a));		\
		if (likely(old == c))				\
			break;					\
		c = old;					\
	}							\
	c != (u);						\
})
#define atomic_inc_not_zero(v)	atomic_add_unless((v), 1, 0)

#define atomic_inc_return(v)	atomic_add_return(1, (v))
#define atomic_dec_return(v)	atomic_sub_return(1, (v))

#define atomic_clear_mask(mask, addr)				\
	__asm__ __volatile__(					\
		LOCK_PREFIX "andl %0,%1"			\
		: : "r" (~(mask)),				\
		    "m" (*(addr)) : "memory")

#define atomic_set_mask(mask, addr)				\
	__asm__ __volatile__(					\
		LOCK_PREFIX "orl %0,%1"				\
		: : "r" (mask),					\
		    "m" (*(addr)) : "memory")

#endif /* __ia64 */
#endif /* __PFL_ATOMIC_H__ */
