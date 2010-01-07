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

static __inline int16_t psc_atomic16_read(const psc_atomic16_t *);
static __inline int32_t psc_atomic32_read(const psc_atomic32_t *);
static __inline int64_t psc_atomic64_read(const psc_atomic64_t *);

static __inline int16_t psc_atomic16_cmpxchg(psc_atomic16_t *, int16_t, int16_t);
static __inline int32_t psc_atomic32_cmpxchg(psc_atomic32_t *, int32_t, int32_t);
static __inline int64_t psc_atomic64_cmpxchg(psc_atomic64_t *, int64_t, int64_t);

#define _PFL_ATOMIC_OPLOOP(prefix, type, v, oldv, newv, op, rv)			\
	do {									\
		type oldv, newv;						\
										\
		do {								\
			oldv = prefix ## _read(v);				\
			newv = op;						\
		} while (prefix ## _cmpxchg((v), oldv, newv) != oldv);		\
		return (rv);							\
	} while (0)

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
