/* $Id$ */

#ifndef _PFL_COMPAT_IA64_ATOMIC_H_
#define _PFL_COMPAT_IA64_ATOMIC_H_

#include <asm/types.h>
#include <asm/intrinsics.h>

#include "pfl/log.h"

#define __bad_increment_for_ia64_fetch_and_add_guts	{ psc_fatalx("__bad_increment_for_ia64_fetch_and_add"); 0; }
#define __bad_increment_for_ia64_fetch_and_add()	(__bad_increment_for_ia64_fetch_and_add_guts)
#define __bad_size_for_ia64_fetch_and_add()		psc_fatalx("__bad_size_for_ia64_fetch_and_add")
#define ia64_cmpxchg_called_with_bad_pointer_guts	{ psc_fatalx("ia64_cmpxchg_called_with_bad_pointer"); 0; }
#define ia64_cmpxchg_called_with_bad_pointer()		(ia64_cmpxchg_called_with_bad_pointer_guts)
#define ia64_xchg_called_with_bad_pointer()		psc_fatalx("ia64_xchg_called_with_bad_pointer")

#define PSC_ATOMIC16_INIT(i)			{ i }
#define PSC_ATOMIC32_INIT(i)			{ i }
#define PSC_ATOMIC64_INIT(i)			{ i }

#define psc_atomic16_access(v)			((v)->value16)
#define psc_atomic32_access(v)			((v)->value32)
#define psc_atomic64_access(v)			((v)->value64)

#define psc_atomic16_set(v, i)			(psc_atomic16_access(v) = (i))
#define psc_atomic32_set(v, i)			(psc_atomic32_access(v) = (i))
#define psc_atomic64_set(v, i)			(psc_atomic64_access(v) = (i))

static __inline int16_t
psc_atomic16_read(psc_atomic16_t *v)
{
	return (psc_atomic16_access(v));
}

static __inline int32_t
psc_atomic32_read(psc_atomic32_t *v)
{
	return (psc_atomic32_access(v));
}

static __inline int64_t
psc_atomic64_read(psc_atomic64_t *v)
{
	return (psc_atomic64_access(v));
}

#undef psc_atomic16_add
static __inline int16_t
psc_atomic16_add(psc_atomic16_t *v, int16_t i)
{
	int16_t old, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic16_read(v);
		newv = old + i;
	} while (ia64_cmpxchg(acq, v, old, newv, sizeof(psc_atomic16_t)) != old);
	return newv;
}

#undef psc_atomic32_add
static __inline int32_t
psc_atomic32_add(psc_atomic32_t *v, int32_t i)
{
	int32_t old, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic32_read(v);
		newv = old + i;
	} while (ia64_cmpxchg(acq, v, old, newv, sizeof(psc_atomic32_t)) != old);
	return newv;
}

#undef psc_atomic64_add
static __inline int64_t
psc_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	int64_t old, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic64_read(v);
		newv = old + i;
	} while (ia64_cmpxchg(acq, v, old, newv, sizeof(psc_atomic64_t)) != old);
	return newv;
}

#undef psc_atomic16_sub
static __inline int16_t
psc_atomic16_sub(psc_atomic16_t *v, int16_t i)
{
	int16_t old, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic16_read(v);
		newv = old - i;
	} while (ia64_cmpxchg(acq, v, old, newv, sizeof(psc_atomic16_t)) != old);
	return newv;
}

#undef psc_atomic32_sub
static __inline int32_t
psc_atomic32_sub(psc_atomic32_t *v, int32_t i)
{
	int32_t old, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic32_read(v);
		newv = old - i;
	} while (ia64_cmpxchg(acq, v, old, newv, sizeof(psc_atomic32_t)) != old);
	return newv;
}

#undef psc_atomic64_sub
static __inline int64_t
psc_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	int64_t old, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = psc_atomic64_read(v);
		newv = old - i;
	} while (ia64_cmpxchg(acq, v, old, newv, sizeof(psc_atomic64_t)) != old);
	return newv;
}

#define psc_atomic16_add_return(v, i) psc_atomic16_add((v), (i))

#define psc_atomic32_add_return(v, i)					\
_PFL_RVSTART {								\
	int __ia64_aar_i = (i);						\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->value)		\
		: psc_atomic32_add((v), __ia64_aar_i);			\
} _PFL_RVEND

#define psc_atomic64_add_return(v, i)					\
_PFL_RVSTART {								\
	long __ia64_aar_i = (i);					\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_aar_i ==  1) || (__ia64_aar_i ==   4)		\
	     || (__ia64_aar_i ==  8) || (__ia64_aar_i ==  16)		\
	     || (__ia64_aar_i == -1) || (__ia64_aar_i ==  -4)		\
	     || (__ia64_aar_i == -8) || (__ia64_aar_i == -16)))		\
		? ia64_fetch_and_add(__ia64_aar_i, &(v)->value)		\
		: ia64_atomic64_add(__ia64_aar_i, (v));			\
} _PFL_RVEND

#define psc_atomic16_sub_return(v, i)	psc_atomic16_sub((v), (i))

#define psc_atomic32_sub_return(v, i)					\
_PFL_RVSTART {								\
	int __ia64_asr_i = (i);						\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->value)	\
		: ia64_atomic32_sub(__ia64_asr_i, (v));			\
} _PFL_RVEND

#define psc_atomic64_sub_return(v, i)					\
_PFL_RVSTART {								\
	long __ia64_asr_i = (i);					\
									\
	(__builtin_constant_p(i)					\
	 && (   (__ia64_asr_i ==   1) || (__ia64_asr_i ==   4)		\
	     || (__ia64_asr_i ==   8) || (__ia64_asr_i ==  16)		\
	     || (__ia64_asr_i ==  -1) || (__ia64_asr_i ==  -4)		\
	     || (__ia64_asr_i ==  -8) || (__ia64_asr_i == -16)))	\
		? ia64_fetch_and_add(-__ia64_asr_i, &(v)->value)	\
		: ia64_atomic64_sub(__ia64_asr_i, (v));			\
} _PFL_RVEND

#define psc_atomic16_dec_test_zero(v)		(psc_atomic16_sub_return((v), 1) == 0)
#define psc_atomic32_dec_test_zero(v)		(psc_atomic32_sub_return((v), 1) == 0)
#define psc_atomic64_dec_test_zero(v)		(psc_atomic64_sub_return((v), 1) == 0)

#undef psc_atomic16_inc
#define psc_atomic16_inc(v)			psc_atomic16_add((v), 1)
#undef psc_atomic16_dec
#define psc_atomic16_dec(v)			psc_atomic16_sub((v), 1)

#undef psc_atomic32_inc
#define psc_atomic32_inc(v)			psc_atomic32_add((v), 1)
#undef psc_atomic32_dec
#define psc_atomic32_dec(v)			psc_atomic32_sub((v), 1)

#undef psc_atomic64_inc
#define psc_atomic64_inc(v)			psc_atomic64_add((v), 1)
#undef psc_atomic64_dec
#define psc_atomic64_dec(v)			psc_atomic64_sub((v), 1)

#undef psc_atomic32_setmask
static __inline void
psc_atomic32_setmask(psc_atomic32_t *v, int32_t mask)
{
	int32_t oldv, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldv = psc_atomic32_read(v);
		newv = oldv | mask;
	} while (ia64_cmpxchg(acq, v, oldv,
	    newv, sizeof(newv)) != oldv);
}

#undef psc_atomic64_setmask
static __inline void
psc_atomic64_setmask(psc_atomic64_t *v, int64_t mask)
{
	int64_t oldv, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldv = psc_atomic64_read(v);
		newv = oldv | mask;
	} while (ia64_cmpxchg(acq, v, oldv,
	    newv, sizeof(newv)) != oldv);
}

#undef psc_atomic32_clearmask
static __inline void
psc_atomic32_clearmask(psc_atomic32_t *v, int32_t mask)
{
	int32_t oldv, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldv = psc_atomic32_read(v);
		newv = oldv & ~mask;
	} while (ia64_cmpxchg(acq, v, oldv,
	    newv, sizeof(newv)) != oldv);
}

#undef psc_atomic64_clearmask
static __inline void
psc_atomic64_clearmask(psc_atomic64_t *v, int64_t mask)
{
	int64_t oldv, newv;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		oldv = psc_atomic64_read(v);
		newv = oldv & ~mask;
	} while (ia64_cmpxchg(acq, v, oldv,
	    newv, sizeof(newv)) != oldv);
}

static __inline int16_t
psc_atomic16_cmpxchg(psc_atomic16_t *v, int16_t cmpv, int16_t newv)
{
	CMPXCHG_BUGCHECK_DECL

	CMPXCHG_BUGCHECK(v);
	return (ia64_cmpxchg(acq, v, cmpv, newv, sizeof(newv)));
}

static __inline int32_t
psc_atomic32_cmpxchg(psc_atomic32_t *v, int32_t cmpv, int32_t newv)
{
	CMPXCHG_BUGCHECK_DECL

	CMPXCHG_BUGCHECK(v);
	return (ia64_cmpxchg(acq, v, cmpv, newv, sizeof(newv)));
}

static __inline int64_t
psc_atomic64_cmpxchg(psc_atomic64_t *v, int64_t cmpv, int64_t newv)
{
	CMPXCHG_BUGCHECK_DECL

	CMPXCHG_BUGCHECK(v);
	return (ia64_cmpxchg(acq, v, cmpv, newv, sizeof(newv)));
}

#endif /* _PFL_COMPAT_IA64_ATOMIC_H_ */
