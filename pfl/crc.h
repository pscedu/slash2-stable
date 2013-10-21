/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_CRC_H_
#define _PFL_CRC_H_

#include <stdint.h>

/* Initialize a CRC accumulator */
#define PSC_CRC32_INIT(crcp)	(*(crcp) = 0xffffffff)
#define PSC_CRC64_INIT(crcp)	(*(crcp) = UINT64_C(0xffffffffffffffff))

/* Finish a CRC calculation */
#define PSC_CRC32_FIN(crcp)	(*(crcp) ^= 0xffffffff)
#define PSC_CRC64_FIN(crcp)	(*(crcp) ^= UINT64_C(0xffffffffffffffff))

/**
 * psc_crc64_calc - Compute a 64-bit CRC of some data.
 * @cp: pointer to an uninitialized CRC buffer.
 * @data: data to perform CRC over.
 * @len: amount of data.
 */
#define psc_crc64_calc(cp, data, len)					\
	do {								\
		PSC_CRC64_INIT(cp);					\
		psc_crc64_add(cp, data, len);				\
		PSC_CRC64_FIN(cp);					\
	} while (0)

/**
 * psc_crc32_calc - Compute a 32-bit CRC of some data.
 * @cp: pointer to an uninitialized CRC buffer.
 * @data: data to perform CRC over.
 * @len: amount of data.
 */
#define psc_crc32_calc(cp, data, len)					\
	do {								\
		PSC_CRC32_INIT(cp);					\
		psc_crc32_add(cp, data, len);				\
		PSC_CRC32_FIN(cp);					\
	} while (0)

void	psc_crc32_add(uint32_t *, const void *, int);
void	psc_crc64_add(uint64_t *, const void *, int);

int	psc_crc32_verify(uint32_t, const void *, int);
int	psc_crc64_verify(uint64_t, const void *, int);

#endif /* _PFL_CRC_H_ */
