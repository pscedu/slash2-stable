/* $Id$ */

#ifndef _PFL_COMPAT_AMD64_ATOMIC_H_
#define _PFL_COMPAT_AMD64_ATOMIC_H_

#define _PFL_ASM(code, ...)		__asm__ __volatile__("lock; " code, ## __VA_ARGS__)
#define _PFL_NL_ASM(code, ...)		__asm__ __volatile__(code, ## __VA_ARGS__)

typedef struct { volatile int16_t value16; } __packed psc_atomic16_t;
typedef struct { volatile int32_t value32; } __packed psc_atomic32_t;
typedef struct { volatile int64_t value64; } __packed psc_atomic64_t;

#define PSC_ATOMIC16_INIT(i)		{ i }
#define PSC_ATOMIC32_INIT(i)		{ i }
#define PSC_ATOMIC64_INIT(i)		{ i }

#define _PFL_GETA16(v)			(((psc_atomic16_t *)(v))->value16)
#define _PFL_GETAC32(v)			(((psc_atomic32_t *)(v))->value32)
#define _PFL_GETAC64(v)			(((psc_atomic64_t *)(v))->value64)

#define psc_atomic16_read(v)		((const)(_PFL_GETA16(v)))
#define psc_atomic32_read(v)		((const)(_PFL_GETA32(v)))
#define psc_atomic64_read(v)		((const)(_PFL_GETA64(v)))

#define psc_atomic16_read(v, i)		((_PFL_GETA16(v)) = (i))
#define psc_atomic32_read(v, i)		((_PFL_GETA32(v)) = (i))
#define psc_atomic64_read(v, i)		((_PFL_GETA64(v)) = (i))

static __inline void
psc_atomic16_add(psc_atomic16_t *v, int16_t i)
{
	_PFL_ASM("addw %1,%0" : "=m" _PFL_GETA16(v) : "ir" (i), "m" _PFL_GETA16(v));
}

static __inline void
psc_atomic32_add(psc_atomic32_t *v, int32_t i)
{
	_PFL_ASM("addl %1,%0" : "=m" _PFL_GETA32(v) : "ir" (i), "m" _PFL_GETA32(v));
}

static __inline void
psc_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("addq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

static __inline void
psc_atomic16_sub(psc_atomic16_t *v, int16_t i)
{
	_PFL_ASM("subw %1,%0" : "=m" _PFL_GETA16(v) : "ir" (i), "m" _PFL_GETA16(v));
}

static __inline void
psc_atomic32_sub(psc_atomic32_t *v, int32_t i)
{
	_PFL_ASM("subl %1,%0" : "=m" _PFL_GETA32(v) : "ir" (i), "m" _PFL_GETA32(v));
}

static __inline void
psc_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("subq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

static __inline int
psc_atomic16_sub_and_test0(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("subw %2, %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

static __inline int
psc_atomic32_sub_and_test0(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("subl %2, %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
}

static __inline int
psc_atomic64_sub_and_test0(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("subq %2, %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

static __inline int
psc_atomic16_add_and_test0(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("addw %2, %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

static __inline int
psc_atomic32_add_and_test0(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("addl %2, %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
}

static __inline int
psc_atomic64_add_and_test0(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("addq %2, %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

static __inline void
psc_atomic16_inc(psc_atomic16_t *v)
{
	_PFL_ASM("incw %0" : "=m" _PFL_GETA16(v) : "m" _PFL_GETA16(v));
}

static __inline void
psc_atomic32_inc(psc_atomic32_t *v)
{
	_PFL_ASM("incl %0" : "=m" _PFL_GETA32(v) : "m" _PFL_GETA32(v));
}

static __inline void
psc_atomic64_inc(psc_atomic64_t *v)
{
	_PFL_ASM("incq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}

static __inline void
psc_atomic16_dec(psc_atomic16_t *v)
{
	_PFL_ASM("decw %0" : "=m" _PFL_GETA16(v) : "m" _PFL_GETA16(v));
}

static __inline void
psc_atomic32_dec(psc_atomic32_t *v)
{
	_PFL_ASM("decl %0" : "=m" _PFL_GETA32(v) : "m" _PFL_GETA32(v));
}

static __inline void
psc_atomic64_dec(psc_atomic64_t *v)
{
	_PFL_ASM("decq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}

static __inline int
psc_atomic16_inc_and_test0(psc_atomic16_t *v)
{
	unsigned char c;

	_PFL_ASM("incw %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "m" _PFL_GETA16(v) : "memory");
	return (c);
}

static __inline int
psc_atomic32_inc_and_test0(psc_atomic32_t *v)
{
	unsigned char c;

	_PFL_ASM("incl %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "m" _PFL_GETA32(v) : "memory");
	return (c);
}

static __inline int
psc_atomic64_inc_and_test0(psc_atomic64_t *v)
{
	unsigned char c;

	_PFL_ASM("incq %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "m" _PFL_GETA64(v) : "memory");
	return (c);
}

static __inline int
psc_atomic16_dec_and_test0(psc_atomic16_t *v)
{
	unsigned char c;

	_PFL_ASM("decw %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "m" _PFL_GETA16(v) : "memory");
	return (c);
}

static __inline int
psc_atomic32_dec_and_test0(psc_atomic32_t *v)
{
	unsigned char c;

	_PFL_ASM("decl %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "m" _PFL_GETA32(v) : "memory");
	return (c);
}

static __inline int
psc_atomic64_dec_and_test0(psc_atomic64_t *v)
{
	unsigned char c;

	_PFL_ASM("decq %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "m" _PFL_GETA64(v) : "memory");
	return (c);
}




static __inline int
_PFL_ATOMIC_ADD_AND_TEST_NEG(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg;
	unsigned char c;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	switch (siz) {
	case 2:
		_PFL_ASM("addw %2, %0; sets %1" : "=m" _PFL_GETA16(v),
		    "=qm" (c) : "ir" (arg.v16), "m" _PFL_GETA16(v) : "memory");
		break;
	case 4:
		_PFL_ASM("addl %2, %0; sets %1" : "=m" _PFL_GETA32(v),
		    "=qm" (c) : "ir" (arg.v32), "m" _PFL_GETA32(v) : "memory");
		break;
	case 8:
		_PFL_ASM("addq %2, %0; sets %1" : "=m" _PFL_GETA64(v),
		    "=qm" (c) : "ir" (arg.v64), "m" _PFL_GETA64(v) : "memory");
		break;
	}
	return (c);
}

static __inline int
_PFL_ATOMIC_SUB_AND_TEST_NEG(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg;
	unsigned char c;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	switch (siz) {
	case 2:
		_PFL_ASM("subw %2, %0; sets %1" : "=m" _PFL_GETA16(v),
		    "=qm" (c) : "ir" (arg.v16), "m" _PFL_GETA16(v) : "memory");
		break;
	case 4:
		_PFL_ASM("subl %2, %0; sets %1" : "=m" _PFL_GETA32(v),
		    "=qm" (c) : "ir" (arg.v32), "m" _PFL_GETA32(v) : "memory");
		break;
	case 8:
		_PFL_ASM("subq %2, %0; sets %1" : "=m" _PFL_GETA64(v),
		    "=qm" (c) : "ir" (arg.v64), "m" _PFL_GETA64(v) : "memory");
		break;
	}
	return (c);
}

static __inline int64_t
_PFL_ATOMIC_ADD_GETNEW(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg, adj;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	adj = arg;

	switch (siz) {
	case 2:
		_PFL_ASM("xaddw %0, %1" : "+r" (arg.v16),
		    "+m" _PFL_GETA16(v) : : "memory");
		return (adj.v16 + arg.v16);
	case 4:
		_PFL_ASM("xaddl %0, %1" : "+r" (arg.v32),
		    "+m" _PFL_GETA32(v) : : "memory");
		return (adj.v32 + arg.v32);
	case 8:
		_PFL_ASM("xaddq %0, %1" : "+r" (arg.v64),
		    "+m" _PFL_GETA64(v) : : "memory");
		return (adj.v64 + arg.v64);
	default:
		psc_fatalx("impossible");
	}
}

static __inline int64_t
_PFL_ATOMIC_SUB_GETNEW(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg, adj;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	adj = arg;

	switch (siz) {
	case 2:
		_PFL_ASM("xsubw %0, %1" : "+r" (arg.v16),
		    "+m" _PFL_GETA16(v) : : "memory");
		return (arg.v16 - adj.v16);
	case 4:
		_PFL_ASM("xsubl %0, %1" : "+r" (arg.v32),
		    "+m" _PFL_GETA32(v) : : "memory");
		return (arg.v32 - adj.v32);
	case 8:
		_PFL_ASM("xsubq %0, %1" : "+r" (arg.v64),
		    "+m" _PFL_GETA64(v) : : "memory");
		return (arg.v64 - adj.v64);
	default:
		psc_fatalx("impossible");
	}
}

static __inline void
_PFL_ATOMIC_CLEARMASK(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg, adj;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	adj = arg;

	switch (siz) {
	case 2:
		_PFL_ASM("andw %0, %1" : : "r" (~arg.v16), "m" _PFL_GETA16(v) : "memory");
		break;
	case 4:
		_PFL_ASM("andl %0, %1" : : "r" (~arg.v32), "m" _PFL_GETA32(v) : "memory");
		break;
	case 8:
		_PFL_ASM("andq %0, %1" : : "r" (~arg.v64), "m" _PFL_GETA64(v) : "memory");
		break;
	}
}

static __inline void
_PFL_ATOMIC_SETMASK(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg, adj;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	adj = arg;

	switch (siz) {
	case 2:
		_PFL_ASM("orw %0, %1" : : "r" (arg.v16),
		    "m" _PFL_GETA16(v) : "memory");
		break;
	case 4:
		_PFL_ASM("orl %0, %1" : : "r" (arg.v32),
		    "m" _PFL_GETA32(v) : "memory");
		break;
	case 8:
		_PFL_ASM("orq %0, %1" : : "r" (arg.v64),
		    "m" _PFL_GETA64(v) : "memory");
		break;
	}
}

static __inline int64_t
_PFL_ATOMIC_CLEARMASK_GETNEW(void *v, size_t siz, ...)
{
	union pfl_atomic_arg arg, adj;
	va_list ap;

	va_start(ap, siz);
	_PFL_ATOMIC_GETARG(ap, siz, &arg);
	va_end(ap);

	adj = arg;
	switch (siz) {
	case 2:
		adj.v16 = ~adj.v16;
		_PFL_ASM("andw %0, %1;" : "=r" (arg.v16)
		    : "m" _PFL_GETA16(v), "0" (adj.v16));
		return (adj.v16 & ~arg.v16);
	case 4:
		adj.v32 = ~adj.v32;
		_PFL_ASM("andl %0, %1;" : "=r" (arg.v32)
		    : "m" _PFL_GETA32(v), "0" (adj.v32));
		return (adj.v32 & ~arg.v32);
	case 8:
		adj.v64 = ~adj.v64;
		_PFL_ASM("andq %0, %1;" : "=r" (arg.v64)
		    : "m" _PFL_GETA64(v), "0" (adj.v64));
		return (adj.v64 & ~arg.v64);
	default:
		psc_fatalx("%zu: bad size value", siz);
	}
}

static __inline int64_t
_PFL_ATOMIC_SETMASK_GETNEW(void *v, size_t siz, int64_t i)
{
	int64_t adj = i;

	switch (siz) {
	case 2:
		_PFL_ASM("orw %0, %1;" : "=r" (i) : "m" _PFL_GETA16(v), "0" (adj));
		break;
	case 4:
		_PFL_ASM("orl %0, %1;" : "=r" (i) : "m" _PFL_GETA32(v), "0" (adj));
		break;
	case 8:
		_PFL_ASM("orq %0, %1;" : "=r" (i) : "m" _PFL_GETA64(v), "0" (adj));
		break;
	default:
		psc_fatalx("%zu: bad size value", siz);
	}
	return (adj | i);
}

static __inline int64_t
_PFL_ATOMIC_CLEARMASK_GETOLD(void *v, size_t siz, int64_t i)
{
	int64_t adj = i;

	i = ~i;
	switch (siz) {
	case 2:
		_PFL_ASM("andw %0, %1" : "=r" (i) : "m" _PFL_GETA16(v), "0" (adj));
		break;
	case 4:
		_PFL_ASM("andl %0, %1" : "=r" (i) : "m" _PFL_GETA32(v), "0" (adj));
		break;
	case 8:
		_PFL_ASM("andq %0, %1" : "=r" (i) : "m" _PFL_GETA64(v), "0" (adj));
		break;
	default:
		psc_fatalx("%zu: bad size value", siz);
	}
	return (adj);
}

static __inline int64_t
_PFL_ATOMIC_SETMASK_GETOLD(void *v, size_t siz, int64_t i)
{
	int64_t adj = i;

	switch (siz) {
	case 2:
		_PFL_ASM("orw %0, %1;" : "=r" (i) : "m" _PFL_GETA16(v), "0" (adj));
		break;
	case 4:
		_PFL_ASM("orl %0, %1;" : "=r" (i) : "m" _PFL_GETA32(v), "0" (adj));
		break;
	case 8:
		_PFL_ASM("orq %0, %1;" : "=r" (i) : "m" _PFL_GETA64(v), "0" (adj));
		break;
	default:
		psc_fatalx("%zu: bad size value", siz);
	}
	return (adj);
}

static __inline int64_t
_PFL_ATOMIC_XCHG(void *v, size_t siz, int64_t i)
{
	switch (siz) {
	case 2:
		_PFL_NL_ASM("xchgw %0, %1" : "=r" (i) : "m" _PFL_GETA16(v), "0" (i) : "memory");
		break;
	case 4:
		_PFL_NL_ASM("xchgl %0, %1" : "=r" (i) : "m" _PFL_GETA32(v), "0" (i) : "memory");
		break;
	case 8:
		_PFL_NL_ASM("xchgq %0, %1" : "=r" (i) : "m" _PFL_GETA64(v), "0" (i) : "memory");
		break;
	default:
		psc_fatalx("%zu: bad size value", siz);
	}
	return (i);
}

static __inline int64_t
_PFL_ATOMIC_CMPXCHG(void *v, size_t siz, int64_t newv, int64_t cmpv)
{
	int64_t oldv;

	switch (siz) {
	case 2:
		_PFL_NL_ASM("cmpxchgw %1, %2" : "=a" (oldv) : "r" (newv),
		    "m" _PFL_GETA16(v), "0" (cmpv) : "memory");
		break;
	case 4:
		_PFL_NL_ASM("cmpxchgl %1, %2" : "=a" (oldv) : "r" (newv),
		    "m" _PFL_GETA32(v), "0" (cmpv) : "memory");
		break;
	case 8:
		_PFL_NL_ASM("cmpxchgq %1, %2" : "=a" (oldv) : "r" (newv),
		    "m" _PFL_GETA64(v), "0" (cmpv) : "memory");
		break;
	default:
		psc_fatalx("%zu: bad size value", siz);
	}
	return (oldv);
}

#endif /* _PFL_COMPAT_AMD64_ATOMIC_H_ */
