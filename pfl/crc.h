/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_CRC_H_
#define _PFL_CRC_H_

#include <stdint.h>

#include "pfl/cdefs.h"

/**
 * psc_crc64_calc - Compute a 64-bit CRC of some data.
 * @cp: pointer to an uninitialized CRC buffer.
 * @data: data to perform CRC over.
 * @len: amount of data.
 */
#define psc_crc64_calc(cp, data, len)					\
	do {								\
		psc_crc64_init(cp);					\
		psc_crc64_add((cp), (data), (len));			\
		psc_crc64_fini(cp);					\
	} while (0)

/**
 * psc_crc32_calc - Compute a 32-bit CRC of some data.
 * @cp: pointer to an uninitialized CRC buffer.
 * @data: data to perform CRC over.
 * @len: amount of data.
 */
#define psc_crc32_calc(cp, data, len)					\
	do {								\
		psc_crc32_init(cp);					\
		psc_crc32_add((cp), (data), (len));			\
		psc_crc32_fini(cp);					\
	} while (0)

__BEGIN_DECLS

void	psc_crc32_add(uint32_t *, const void *, int);
int	psc_crc32_verify(uint32_t, const void *, int);

void	psc_crc64_add(uint64_t *, const void *, int);
int	psc_crc64_verify(uint64_t, const void *, int);

#ifdef USE_GCRCUTIL

void	psc_crc32_init(uint32_t *);
void	psc_crc64_init(uint64_t *);

void	psc_crc32_fini(uint32_t *);
void	psc_crc64_fini(uint64_t *);

#else

#define psc_crc32_init(cp)	(*(cp) = 0xffffffff)
#define psc_crc64_init(cp)	(*(cp) = UINT64_C(0xffffffffffffffff))

#define psc_crc32_fini(cp)	(*(cp) ^= 0xffffffff)
#define psc_crc64_fini(cp)	(*(cp) ^= UINT64_C(0xffffffffffffffff))

#endif

__END_DECLS

#endif /* _PFL_CRC_H_ */
