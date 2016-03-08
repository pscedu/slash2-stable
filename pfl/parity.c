/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/types.h>

#include <stdint.h>

#include "pfl/parity.h"

void
parity_calc(const void *data, void *parity, uint32_t len)
{
	const uint64_t *d64;
	const uint8_t *d8;
	uint64_t *p64;
	uint8_t *p8;
	size_t i, n;

	/* take 'em 8 bytes at a time first for speed */
	d64 = data;
	p64 = parity;
	n = len / 8;
	for (i = 0; i < n; i++)
		*p64++ ^= *d64++;

	/*
	 * Now finish it off one byte at a time,
	 * for those not divisible by 8.
	 */
	d8 = (const uint8_t *)d64;
	p8 = (uint8_t *)p64;
	n = len - 8 * n;
	for (i = 0; i < n; i++)
		*p8++ ^= *d8++;
}
