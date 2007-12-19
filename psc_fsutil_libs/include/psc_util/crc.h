/* $Id$ */

#ifndef _PFL_CRC_H_
#define _PFL_CRC_H_

#include <sys/types.h>

#include "psc_types.h"

/*
 * Shamelessly heisted from TelegraphCQ-0.2
 *   pauln 11/08/06
 */
extern const u64 crc_table[];

/* Initialize a CRC accumulator */
#define CRC_INIT(crc) crc = UINT64CONST(0xffffffffffffffff)

/* Finish a CRC calculation */
#define CRC_FIN(crc)	(crc ^= UINT64CONST(0xffffffffffffffff))

/* Accumulate some (more) bytes into a CRC */
#define CRC_DO(crc, data, len)							\
	do {									\
		u64		__crc0 = crc;					\
		unsigned char  *__data = (unsigned char *) (data);		\
		u32		__len  = (len);					\
		while (__len-- > 0) {						\
			int __tab_index = ((int) (__crc0 >> 56) ^ *__data++) & 0xFF; \
			__crc0 = crc_table[__tab_index] ^ (__crc0 << 8);	\
		}								\
		crc = __crc0;							\
	} while (0)

int crc_valid(psc_crc_t crc, const void *p, size_t len);

#endif /* _PFL_CRC_H_ */
