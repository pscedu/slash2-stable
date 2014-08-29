/* $Id$ */

#include <stdint.h>

#include "std_headers.h"
#include "interface.h"

#include "pfl/crc.h"

extern "C" {

using namespace crcutil_interface;

void
psc_crc64_add(uint64_t *cp, const void *datap, int len)
{
	CRC *crc;

	// UINT64 poly_lo, UINT64 poly_hi, size_t degree, bool canonical,
	// UINT64 roll_start_value_lo, UINT64 roll_start_value_hi,
	// size_t roll_length, bool use_sse4_2, const void **allocated_memory

	crc = CRC::Create(0xEB31D82E, 0, 32, true, 0x1111, 0, 4,
	    CRC::IsSSE42Available(), NULL);
	crc->Compute(datap, len, cp);
	crc->Delete();
}

}
