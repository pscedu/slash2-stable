/* $Id$ */

#ifndef _PFL_CRC_H_
#define _PFL_CRC_H_

#include <stdint.h>

typedef	uint64_t psc_crc_t;

#define CRCSZ (sizeof(psc_crc_t))

/* Initialize a CRC accumulator */
#define PSC_CRC_INIT(crc)	((crc) = UINT64_C(0xffffffffffffffff))

/* Finish a CRC calculation */
#define PSC_CRC_FIN(crc)	((crc) ^= UINT64_C(0xffffffffffffffff))

void psc_crc_calc(psc_crc_t *, const void *, int);
void psc_crc_add(psc_crc_t *, const void *, int);

extern const uint64_t psc_crc_table[];

#endif /* _PFL_CRC_H_ */
