/* $Id$ */

#ifndef _PFL_CRC_H_
#define _PFL_CRC_H_

#include <stdint.h>

typedef	uint64_t		psc_crc64_t;
typedef	uint32_t		psc_crc32_t;

/* Initialize a CRC accumulator */
#define PSC_CRC32_INIT(crcp)	(*(crcp) = 0xffffffff)
#define PSC_CRC64_INIT(crcp)	(*(crcp) = UINT64_C(0xffffffffffffffff))

/* Finish a CRC calculation */
#define PSC_CRC32_FIN(crcp)	(*(crcp) ^= 0xffffffff)
#define PSC_CRC64_FIN(crcp)	(*(crcp) ^= UINT64_C(0xffffffffffffffff))

/*
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

/*
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

void	psc_crc32_add(psc_crc32_t *, const void *, int);
void	psc_crc64_add(psc_crc64_t *, const void *, int);

int	psc_crc32_verify(psc_crc32_t, const void *, int);
int	psc_crc64_verify(psc_crc64_t, const void *, int);

#endif /* _PFL_CRC_H_ */
