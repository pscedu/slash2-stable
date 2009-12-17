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

#include <stdint.h>

#include "psc_util/base64.h"

/*
 * psc_base64_encode - Encode data into standard base 64 encoding.
 * @buf: data to encode.
 * @enc: value-result buffer to receive encoding.
 * @siz: length of data.
 * Note: @enc must be sized 4/3+1 the size of buf!
 */
void
psc_base64_encode(const void *buf, char *enc, size_t siz)
{
	static char pr[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz0123456789+/";
	const unsigned char *p;
	uint32_t val;
	size_t pos;
	int i;

	i = 0;
	for (pos = 0, p = buf; pos <= siz + 3; pos += 3, p += 3) {
		/*
		 * Convert 3 bytes of input (3*8 bits) into
		 * 4 bytes of output (4*6 bits).
		 *
		 * If fewer than 3 bytes are available for this
		 * round, use zeroes in their place.
		 */
		val = p[0] << 16;
		if (pos + 1 < siz)
			val |= p[1] << 8;
		if (pos + 2 < siz)
			val |= p[2];

		enc[i++] = pr[val >> 18];
		enc[i++] = pr[(val >> 12) & 0x3f];
		if (pos + 1 >= siz)
			break;
		enc[i++] = pr[(val >> 6) & 0x3f];
		if (pos + 2 >= siz)
			break;
		enc[i++] = pr[val & 0x3f];
	}
	if (pos + 1 >= siz) {
		enc[i++] = '=';
		enc[i++] = '=';
	} else if (pos + 2 >= siz)
		enc[i++] = '=';
	enc[i++] = '\0';
}
