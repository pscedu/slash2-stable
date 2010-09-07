/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_ALLOC_H_
#define _PFL_ALLOC_H_

#include <sys/types.h>
#include <sys/mman.h>

#include <stdint.h>
#include <stdlib.h>

#include "psc_util/log.h"

#ifndef HAVE_POSIX_MEMALIGN
# include "pfl/compat/posix_memalign.h"
#endif

/* aliases for common usage */
#define PSCALLOC(sz)		psc_alloc((sz), 0)
#define TRY_PSCALLOC(sz)	psc_alloc((sz), PAF_CANFAIL)

#define PSC_REALLOC(p, sz)	psc_realloc((p), (sz), 0)
#define PSC_TRY_REALLOC(p, sz)	psc_realloc((p), (sz), PAF_CANFAIL)

#ifdef DEBUG

#define PSC_ALLOC_MAGIC		UINT64_C(0xb4fa95df87b8a7fd)

#define psc_free_nolog(p)						\
	do {								\
		uint64_t *_tp;						\
									\
		_tp = (void *)(p);					\
		if (_tp) {						\
			_tp--;						\
			psc_assert(*_tp == PSC_ALLOC_MAGIC);		\
		}							\
		free(_tp);						\
		(p) = NULL;						\
	} while (0)

#define PSCFREE(p)							\
	do {								\
		psc_debugs(PSS_MEM, "free(%p) [guard]", (p));		\
		psc_free_nolog(p);					\
	} while (0)

#define _psc_free_noguard(p)						\
	do {								\
		psc_debugs(PSS_MEM, "free(%p) [noguard]", (p));		\
		free(p);						\
		(p) = NULL;						\
	} while (0)

#define _PSC_REALLOC(oldp, sz, fl)					\
	{								\
		uint64_t *_kp, *_op;					\
		size_t _tsz = (sz);					\
		void *_p;						\
									\
		/* if user is realloc()'ing, adjust for guard value */	\
		_op = (void *)oldp;					\
		if (_op)						\
			_op--;						\
									\
		/* new mem chunk should have space for 1 or 2 guards */	\
		_tsz += sizeof(*_kp);					\
		if (((fl) & (PAF_PAGEALIGN | PAF_LOCK)) == PAF_LOCK)	\
			_tsz += sizeof(*_kp);				\
									\
		_p = _psc_realloc(_op, _tsz, (fl));			\
									\
		if (_p && ((fl) & PAF_PAGEALIGN) == 0) {		\
			_kp = _p;					\
			*_kp++ = PSC_ALLOC_MAGIC;			\
			_p = _kp;					\
		}							\
									\
		if (((fl) & PAF_NOLOG) == 0) {				\
			if (oldp)					\
				psc_debugs(PSS_MEM, "realloc(%p)=%p "	\
				    "sz=%zu fl=%d", (oldp), _p, _tsz,	\
				    (fl));				\
			else						\
				psc_debugs(PSS_MEM, "alloc()=%p "	\
				    "sz=%zu fl=%d", _p, _tsz, (fl));	\
		}							\
									\
		if (_p && ((fl) & PAF_LOCK)) {				\
			_kp = (void *)((char *)_p + (sz));		\
			*_kp = PSC_ALLOC_MAGIC;				\
		}							\
									\
		_p;							\
	}

/**
 * psc_free_mlocked - Free mlock(2)'d memory.
 * @p: mlock(2)'d memory chunk to free.
 * @size: size of chunk.
 *
 * If DEBUG is enabled, this will check guards before and after the
 * memory region.
 */
#define psc_free_mlocked(p, size)					\
	do {								\
		void *_p = (p);						\
		uint64_t *_kp;						\
									\
		if (_p && munlock((uint64_t *)_p - 1,			\
		    (size) + sizeof(uint64_t) * 2) == -1)		\
			psc_fatal("munlock %p", _p);			\
		if (_p) {						\
			_kp = (uint64_t *)((char *)_p + (size));	\
			psc_assert(*_kp == PSC_ALLOC_MAGIC);		\
		}							\
		PSCFREE(p);						\
	} while (0)

/**
 * psc_free_aligned - Free page-aligned memory.
 * @p: memory chunk to free.
 *
 * posix_memalign(3) states that free(3) may be used to release memory.
 *
 * No guards are provided since the region must start at a page-aligned
 * boundary and we don't have the length.
 */
#define psc_free_aligned(p)	_psc_free_noguard(p)

/**
 * psc_free_mlocked_aligned - Free mlock(2)'d, page-aligned memory.
 * @p: memory chunk to free.
 * @size: size of chunk.
 *
 * If DEBUG is enabled, this will check guards only after the memory
 * region.
 */
#define psc_free_mlocked_aligned(p, size)				\
	do {								\
		void *_p = (p);						\
									\
		if (_p && munlock(_p, (size) + sizeof(uint64_t)) == -1)	\
			psc_fatal("munlock %p", _p);			\
		if (_p) {						\
			uint64_t *_kp;					\
									\
			_kp = (uint64_t *)((char *)_p + (size));	\
			psc_assert(*_kp == PSC_ALLOC_MAGIC);		\
		}							\
		_psc_free_noguard(p);					\
	} while (0)

#else

#define psc_free_nolog(p)						\
	do {								\
		free(p);						\
		(p) = NULL;						\
	} while (0)

#define PSCFREE(p)							\
	do {								\
		psc_debugs(PSS_MEM, "free(%p)", (p));			\
		psc_free_nolog(p);					\
	} while (0)

#define _PSC_REALLOC(oldp, sz, fl)					\
	{								\
		void *_p;						\
									\
		_p = _psc_realloc((oldp), (sz), (fl));			\
		if (((fl) & PAF_NOLOG) == 0) {				\
			if (oldp)					\
				psc_debugs(PSS_MEM, "realloc(%p)=%p "	\
				    "sz=%zu fl=%d", (oldp), _p,		\
				    (size_t)(sz), (fl));		\
			else						\
				psc_debugs(PSS_MEM, "alloc()=%p "	\
				    "sz=%zu fl=%d", _p, (size_t)(sz),	\
				    (fl));				\
		}							\
		_p;							\
	}

#define psc_free_mlocked(p, size)					\
	do {								\
		if ((p) && munlock((p), (size)) == -1)			\
			psc_fatal("munlock %p", (p));			\
		PSCFREE(p);						\
	} while (0)

#define psc_free_aligned(p)	PSCFREE(p)

#define psc_free_mlocked_aligned(p, size)				\
	do {								\
		if ((p) && munlock((p), (size)) == -1)			\
			psc_fatal("munlock %p", (p));			\
		PSCFREE(p);						\
	} while (0)

#endif

#define psc_alloc(sz, fl)	(_PSC_REALLOC(NULL, (sz), (fl)))
#define psc_realloc(p, sz, fl)	(_PSC_REALLOC((p),  (sz), (fl)))

/* allocation flags */
#define PAF_CANFAIL	(1 << 0)	/* return NULL instead of fatal */
#define PAF_PAGEALIGN	(1 << 1)	/* align to physmem page size */
#define PAF_NOREAP	(1 << 2)	/* don't reap pools if mem unavail */
#define PAF_LOCK	(1 << 3)	/* lock mem regions as unswappable */
#define PAF_NOZERO	(1 << 4)	/* don't force memory zeroing */
#define PAF_NOLOG	(1 << 5)	/* don't psclog this allocation */

void	*_psc_realloc(void *, size_t, int);
void	*psc_calloc(size_t, size_t, int);
char	*psc_strdup(const char *);

extern long pscPageSize;

#endif /* _PFL_ALLOC_H_ */
