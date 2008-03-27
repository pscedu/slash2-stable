/* $Id$ */

#ifndef __PFL_CRC_H__
#define __PFL_CRC_H__

#include "psc_types.h"

/*
 * Shamelessly heisted from TelegraphCQ-0.2
 *   pauln 11/08/06
 */

/* Initialize a CRC accumulator */
#define PSC_CRC_INIT(crc)	((crc) = UINT64CONST(0xffffffffffffffff))

/* Finish a CRC calculation */
#define PSC_CRC_FIN(crc)	((crc) ^= UINT64CONST(0xffffffffffffffff))

/* Accumulate some (more) bytes into a CRC */
#define PSC_CRC_ADD(crc, data, len)						\
	do {									\
		unsigned char *__data = (unsigned char *)(data);		\
		u64 __crc0 = (crc);						\
		u32 __len = (len);						\
		int __idx;							\
										\
		while (__len-- > 0) {						\
			__idx = ((int)(__crc0 >> 56) ^ *__data++) & 0xff;	\
			__crc0 = psc_crc_table[__idx] ^ (__crc0 << 8);		\
		}								\
		crc = __crc0;							\
	} while (0)

#define PSC_CRC_CALC(crc, buf, len)						\
	do {									\
		PSC_CRC_INIT(crc);						\
		PSC_CRC_ADD((crc), (buf), (len));				\
		PSC_CRC_FIN(crc);						\
	} while (0)

void psc_crc_add(psc_crc_t *, const void *, int);
void psc_crc_calc(psc_crc_t *, const void *, int);

extern const u64 psc_crc_table[];

#endif /* __PFL_CRC_H__ */
