/* $Id$ */

static __inline int16_t psc_atomic16_read(psc_atomic16_t *);
static __inline int32_t psc_atomic32_read(psc_atomic32_t *);
static __inline int64_t psc_atomic64_read(psc_atomic64_t *);

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
