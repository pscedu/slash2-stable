/* $Id$ */

#include <sys/types.h>

#include <stdint.h>

#include "psc_util/parity.h"

void
parity_calc(const void *data, void *parity, uint32_t len)
{
        uint64_t *d64, *p64;
        uint8_t *d8, *p8;
        size_t i, n;

        /* take 'em 8 bytes at a time first, for speed */
        d64 = (uint64_t *)data;
        p64 = (uint64_t *)parity;
        n = len / 8;
        for (i = 0; i < n; i++)
                *p64++ ^= *d64++;

        /* now finish it off one byte at a time,
         * for those not divisible by 8 */
        d8 = (uint8_t *)d64;
        p8 = (uint8_t *)p64;
        n = len - 8 * n;
        for (i = 0; i < n; i++)
                *p8++ ^= *d8++;
}
