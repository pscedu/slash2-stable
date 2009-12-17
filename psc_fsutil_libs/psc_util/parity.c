/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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
