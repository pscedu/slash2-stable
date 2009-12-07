/* $Id$ */

#ifndef _PFL_COMPAT_IA64_ATOMIC_H_
#define _PFL_COMPAT_IA64_ATOMIC_H_

#include "psc_util/log.h"

#define __bad_increment_for_ia64_fetch_and_add_guts	{ psc_fatalx("__bad_increment_for_ia64_fetch_and_add"); 0; }
#define __bad_increment_for_ia64_fetch_and_add()	(__bad_increment_for_ia64_fetch_and_add_guts)
#define __bad_size_for_ia64_fetch_and_add()		psc_fatalx("__bad_size_for_ia64_fetch_and_add")
#define ia64_cmpxchg_called_with_bad_pointer_guts	{ psc_fatalx("ia64_cmpxchg_called_with_bad_pointer"); 0; }
#define ia64_cmpxchg_called_with_bad_pointer()		(ia64_cmpxchg_called_with_bad_pointer_guts)
#define ia64_xchg_called_with_bad_pointer()		psc_fatalx("ia64_xchg_called_with_bad_pointer")

typedef struct { volatile __s32 value; } atomic_t;
typedef struct { volatile __s16 value; } psc_atomic16_t;
typedef struct { volatile __s32 value; } psc_atomic32_t;
typedef struct { volatile __s64 value; } psc_atomic64_t;

#define ATOMIC_INIT(i)				{ (i) }
#define PSC_ATOMIC16_INIT(i)			{ (i) }
#define PSC_ATOMIC32_INIT(i)			{ (i) }
#define PSC_ATOMIC64_INIT(i)			{ (i) }

#define psc_atomic16_access(v)			((v)->value)
#define psc_atomic32_access(v)			((v)->value)
#define psc_atomic64_access(v)			((v)->value)

#define atomic_read(v)				((const int)(v)->value)
#define psc_atomic16_read(v)			((const uint16_t)psc_atomic16_access(v))
#define psc_atomic32_read(v)			((const uint32_t)psc_atomic32_access(v))
#define psc_atomic64_read(v)			((const uint64_t)psc_atomic32_access(v))

#define atomic_set(v, i)			(((v)->value) = (i))
#define psc_atomic16_set(v, i)			(psc_atomic16_access(v) = (i))
#define psc_atomic32_set(v, i)			(psc_atomic32_access(v) = (i))
#define psc_atomic64_set(v, i)			(psc_atomic64_access(v) = (i))

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
ia64_atomic16_add(__s16 i, psc_atomic16_t *v)
{
	__s16 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic16_read(v);
		new = old + i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(psc_atomic16_t)) != old);
	return new;
}

static __inline int
ia64_atomic32_add(__s32 i, psc_atomic32_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic32_read(v);
		new = old + i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(psc_atomic32_t)) != old);
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
ia64_atomic16_sub(__s16 i, psc_atomic16_t *v)
{
	__s16 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic16_read(v);
		new = old - i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(psc_atomic16_t)) != old);
	return new;
}

static __inline int
ia64_atomic32_sub(__s32 i, psc_atomic32_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic32_read(v);
		new = old - i;
	} while (ia64_cmpxchg(acq, v, old, new, sizeof(psc_atomic32_t)) != old);
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

#define psc_atomic16_add_return(v, i) ia64_atomic16_add((i), (v))

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
		: ia64_atomic32_add(__ia64_aar_i, (v));			\
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

#define psc_atomic16_sub_return(v, i)	ia64_atomic16_sub((i), (v))

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
		: ia64_atomic32_sub(__ia64_asr_i, (v));			\
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

#define atomic_dec_and_test(v)			(atomic_sub_return(1, (v)) == 0)
#define psc_atomic16_dec_test_zero(v)		(psc_atomic16_sub_return((v), 1) == 0)
#define psc_atomic32_dec_test_zero(v)		(psc_atomic32_sub_return((v), 1) == 0)
#define psc_atomic64_dec_test_zero(v)		(psc_atomic64_sub_return((v), 1) == 0)

#define atomic_add(i, v)			atomic_add_return((i), (v))
#define atomic_sub(i, v)			atomic_sub_return((i), (v))
#define atomic_inc(v)				atomic_add(1, (v))
#define atomic_dec(v)				atomic_sub(1, (v))

#define psc_atomic16_add(v, i)			psc_atomic16_add_return((v), (i))
#define psc_atomic16_sub(v, i)			psc_atomic16_sub_return((v), (i))
#define psc_atomic16_inc(v)			psc_atomic16_add((v), 1)
#define psc_atomic16_dec(v)			psc_atomic16_sub((v), 1)

#define psc_atomic32_add(v, i)			psc_atomic32_add_return((v), (i))
#define psc_atomic32_sub(v, i)			psc_atomic32_sub_return((v), (i))
#define psc_atomic32_inc(v)			psc_atomic32_add((v), 1)
#define psc_atomic32_dec(v)			psc_atomic32_sub((v), 1)

#define psc_atomic64_add(v, i)			psc_atomic64_add_return((v), (i))
#define psc_atomic64_sub(v, i)			psc_atomic64_sub_return((v), (i))
#define psc_atomic64_inc(v)			psc_atomic64_add((v), 1)
#define psc_atomic64_dec(v)			psc_atomic64_sub((v), 1)

static __inline void
psc_atomic32_set_mask(psc_atomic32_t *v, int32_t mask)
{
	int32_t oldval, newval;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldval = psc_atomic32_read(v);
		newval = oldval | mask;
	} while (ia64_cmpxchg(acq, v, oldval,
	    newval, sizeof(newval)) != oldval);
}

static __inline void
psc_atomic32_clear_mask(psc_atomic32_t *v, int32_t mask)
{
	int32_t oldval, newval;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldval = psc_atomic32_read(v);
		newval = oldval & ~mask;
	} while (ia64_cmpxchg(acq, v, oldval,
	    newval, sizeof(newval)) != oldval);
}

static __inline void
psc_atomic64_set_mask(psc_atomic64_t *v, int64_t mask)
{
	int64_t oldval, newval;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldval = psc_atomic64_read(v);
		newval = oldval | mask;
	} while (ia64_cmpxchg(acq, v, oldval,
	    newval, sizeof(newval)) != oldval);
}

static __inline void
psc_atomic64_clear_mask(psc_atomic64_t *v, int64_t mask)
{
	int64_t oldval, newval;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldval = psc_atomic64_read(v);
		newval = oldval & ~mask;
	} while (ia64_cmpxchg(acq, v, oldval,
	    newval, sizeof(newval)) != oldval);
}

static __inline void
atomic_set_mask(int32_t mask, atomic_t *v)
{
	int32_t oldval, newval;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldval = atomic_read(v);
		newval = oldval | mask;
	} while (ia64_cmpxchg(acq, v, oldval,
	    newval, sizeof(newval)) != oldval);
}

static __inline void
atomic_clear_mask(int32_t mask, atomic_t *v)
{
	int32_t oldval, newval;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldval = atomic_read(v);
		newval = oldval & ~mask;
	} while (ia64_cmpxchg(acq, v, oldval,
	    newval, sizeof(newval)) != oldval);
}

#endif /* _PFL_COMPAT_IA64_ATOMIC_H_ */
