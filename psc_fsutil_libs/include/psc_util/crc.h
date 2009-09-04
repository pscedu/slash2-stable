/* $Id$ */

#ifndef _PFL_CRC_H_
#define _PFL_CRC_H_

#include <stdint.h>

#include "psc_types.h"

/*
 * Shamelessly heisted from TelegraphCQ-0.2
 *   pauln 11/08/06
 */

/* Initialize a CRC accumulator */
#define PSC_CRC_INIT(crc)	((crc) = UINT64_C(0xffffffffffffffff))

/* Finish a CRC calculation */
#define PSC_CRC_FIN(crc)	((crc) ^= UINT64_C(0xffffffffffffffff))

void psc_crc_calc(psc_crc_t *, const void *, int);
void psc_crc_add(psc_crc_t *, const void *, int);

extern const uint64_t psc_crc_table[];

#define PRI_PSC_CRC "%16"PRIx64

#endif /* _PFL_CRC_H_ */
