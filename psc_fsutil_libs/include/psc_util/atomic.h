/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_ATOMIC_H_
#define _PFL_ATOMIC_H_

#include <stdint.h>
#include <stdarg.h>

#include "psc_util/log.h"

#define psc_atomic16_init			_pfl_gen_atomic16_init
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

#define psc_atomic32_init			_pfl_gen_atomic32_init
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

#define psc_atomic64_init			_pfl_gen_atomic64_init
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

struct psc_atomic16 { volatile int16_t value16; } __packed;
struct psc_atomic32 { volatile int32_t value32; } __packed;
struct psc_atomic64 { volatile int64_t value64; } __packed;

#define psc_atomic16_t struct psc_atomic16
#define psc_atomic32_t struct psc_atomic32
#define psc_atomic64_t struct psc_atomic64

#ifdef __ia64
# include "pfl/compat/ia64/atomic.h"
#elif defined(__amd64)
# include "pfl/compat/amd64/atomic.h"
#else
# include "pfl/compat/i386/atomic.h"
#endif

#include "pfl/compat/generic/atomic.h"

#define PSC_ATOMIC16_SIZE	sizeof(psc_atomic16_t)
#define PSC_ATOMIC32_SIZE	sizeof(psc_atomic32_t)
#define PSC_ATOMIC64_SIZE	sizeof(psc_atomic64_t)

static __inline void
_pfl_atomic_init(void *v, size_t siz)
{
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_init(v);
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_init(v);
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_init(v);
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
}

static __inline void
_pfl_atomic_read(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		*va_arg(ap, int16_t *) = psc_atomic16_read(v);
		break;
	case PSC_ATOMIC32_SIZE:
		*va_arg(ap, int32_t *) = psc_atomic32_read(v);
		break;
	case PSC_ATOMIC64_SIZE:
		*va_arg(ap, int64_t *) = psc_atomic64_read(v);
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

static __inline void
_pfl_atomic_set(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_set(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_set(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_set(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

static __inline void
_pfl_atomic_add(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_add(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_add(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_add(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

static __inline void
_pfl_atomic_sub(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_sub(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_sub(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_sub(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

static __inline int
_pfl_atomic_sub_and_test0(void *v, size_t siz, ...)
{
	va_list ap;
	int rc;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		rc = psc_atomic16_sub_and_test0(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		rc = psc_atomic32_sub_and_test0(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		rc = psc_atomic64_sub_and_test0(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
	return (rc);
}

static __inline int
_pfl_atomic_add_and_test0(void *v, size_t siz, ...)
{
	va_list ap;
	int rc;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		rc = psc_atomic16_add_and_test0(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		rc = psc_atomic32_add_and_test0(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		rc = psc_atomic64_add_and_test0(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
	return (rc);
}

static __inline void
_pfl_atomic_inc(void *v, size_t siz)
{
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_inc(v);
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_inc(v);
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_inc(v);
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
}

static __inline void
_pfl_atomic_dec(void *v, size_t siz)
{
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_dec(v);
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_dec(v);
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_dec(v);
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
}

static __inline int
_pfl_atomic_inc_and_test0(void *v, size_t siz)
{
	int rc;

	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		rc = psc_atomic16_inc_and_test0(v);
		break;
	case PSC_ATOMIC32_SIZE:
		rc = psc_atomic32_inc_and_test0(v);
		break;
	case PSC_ATOMIC64_SIZE:
		rc = psc_atomic64_inc_and_test0(v);
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	return (rc);
}

static __inline int
_pfl_atomic_dec_and_test0(void *v, size_t siz)
{
	int rc;

	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		rc = psc_atomic16_dec_and_test0(v);
		break;
	case PSC_ATOMIC32_SIZE:
		rc = psc_atomic32_dec_and_test0(v);
		break;
	case PSC_ATOMIC64_SIZE:
		rc = psc_atomic64_dec_and_test0(v);
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	return (rc);
}

static __inline int
_pfl_atomic_add_and_test_neg(void *v, size_t siz, ...)
{
	va_list ap;
	int rc;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		rc = psc_atomic16_add_and_test_neg(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		rc = psc_atomic32_add_and_test_neg(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		rc = psc_atomic64_add_and_test_neg(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
	return (rc);
}

static __inline int
_pfl_atomic_sub_and_test_neg(void *v, size_t siz, ...)
{
	va_list ap;
	int rc;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		rc = psc_atomic16_sub_and_test_neg(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		rc = psc_atomic32_sub_and_test_neg(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		rc = psc_atomic64_sub_and_test_neg(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
	return (rc);
}

static __inline void
_pfl_atomic_clearmask(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_clearmask(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_clearmask(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_clearmask(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

static __inline void
_pfl_atomic_setmask(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE:
		psc_atomic16_setmask(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC32_SIZE:
		psc_atomic32_setmask(v, va_arg(ap, int32_t));
		break;
	case PSC_ATOMIC64_SIZE:
		psc_atomic64_setmask(v, va_arg(ap, int64_t));
		break;
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

static __inline void
_pfl_atomic_cmpxchg(void *v, size_t siz, ...)
{
	va_list ap;

	va_start(ap, siz);
	switch (siz) {
	case PSC_ATOMIC16_SIZE: {
		int16_t cmpv;

		cmpv = va_arg(ap, int32_t);
		psc_atomic16_cmpxchg(v, cmpv, va_arg(ap, int32_t));
		break;
	    }
	case PSC_ATOMIC32_SIZE: {
		int32_t cmpv;

		cmpv = va_arg(ap, int32_t);
		psc_atomic32_cmpxchg(v, cmpv, va_arg(ap, int32_t));
		break;
	    }
	case PSC_ATOMIC64_SIZE: {
		int64_t cmpv;

		cmpv = va_arg(ap, int64_t);
		psc_atomic64_cmpxchg(v, cmpv, va_arg(ap, int64_t));
		break;
	    }
	default:
		psc_fatalx("%zd: invalid integer width", siz);
	}
	va_end(ap);
}

#define psc_atomic_init(v)			_pfl_atomic_init((v), sizeof(*(v)))
#define psc_atomic_read(v, i)			_pfl_atomic_read((v), sizeof(*(v)), (i))
#define psc_atomic_set(v, i)			_pfl_atomic_set((v), sizeof(*(v)), (i))
#define psc_atomic_add(v, i)			_pfl_atomic_add((v), sizeof(*(v)), (i))
#define psc_atomic_sub(v, i)			_pfl_atomic_sub((v), sizeof(*(v)), (i))
#define psc_atomic_sub_and_test0(v, i)		_pfl_atomic_sub_and_test0((v), sizeof(*(v)), (i))
#define psc_atomic_add_and_test0(v, i)		_pfl_atomic_add_and_test0((v), sizeof(*(v)), (i))
#define psc_atomic_inc(v)			_pfl_atomic_inc((v), sizeof(*(v)))
#define psc_atomic_dec(v)			_pfl_atomic_dec((v), sizeof(*(v)))
#define psc_atomic_inc_and_test0(v)		_pfl_atomic_inc_and_test0((v), sizeof(*(v)))
#define psc_atomic_dec_and_test0(v)		_pfl_atomic_dec_and_test0((v), sizeof(*(v)))
#define psc_atomic_add_and_test_neg(v, i)	_pfl_atomic_add_and_test_neg((v), sizeof(*(v)), (i))
#define psc_atomic_sub_and_test_neg(v, i)	_pfl_atomic_sub_and_test_neg((v), sizeof(*(v)), (i))
#define psc_atomic_clearmask(v, i)		_pfl_atomic_clearmask((v), sizeof(*(v)), (i))
#define psc_atomic_setmask(v, i)		_pfl_atomic_setmask((v), sizeof(*(v)), (i))
#define psc_atomic_cmpxchg(v, cmpv, newv)	_pfl_atomic_cmpxchg((v), sizeof(*(v)), (cmpv), (newv))

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
psc_atomic16_testmask_set(psc_atomic16_t *v, int64_t mask, int64_t newval)
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
psc_atomic32_testmask_set(psc_atomic32_t *v, int64_t mask, int64_t newval)
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
