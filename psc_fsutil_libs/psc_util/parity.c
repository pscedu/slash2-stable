/* $Id$ */

#include <sys/types.h>

#include "psc_types.h"
#include "psc_util/parity.h"

void
parity_calc(const void *data, void *parity, u32 len)
{
        size_t i, imax;
        u64 *d64, *p64;
        u8  *d8, *p8;

        /* take 'em 8 bytes at a time first, for speed */
        d64  = (u64*)data;
        p64  = (u64*)parity;
        imax = len / 8;
        for (i=0; i<imax; i++, d64++, p64++)
                (*p64) ^= (*d64);

        /* now finish it off one byte at a time,
         * for those not divisible by 8 */
        d8   = (u8*)d64;
        p8   = (u8*)p64;
        imax = len - 8*imax;
        for (i=0; i<imax; i++, d8++, p8++)
                (*p8) ^= (*d8);
}
