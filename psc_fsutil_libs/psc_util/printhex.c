/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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
#include <stdio.h>

#include "psc_util/lock.h"

void
printhex(const void *ptr, size_t len)
{
	const unsigned char *p = ptr;
	size_t n;

	flockfile(stdout);
	for (n = 0; n < len; p++, n++) {
		if (n) {
			if (n % 32 == 0)
				printf("\n");
			else {
				if (n % 8 == 0)
					printf(" ");
				printf(" ");
			}
		}
		printf("%02x", *p);
	}
	printf("\n------------------------------------------\n");
	funlockfile(stdout);
}

/*
 * printvbin - display the bit representation of some data.
 */
void
printvbin(const void *ptr, size_t len)
{
	const unsigned char *p = ptr;
	size_t n;
	int i;

	flockfile(stdout);
	for (n = 0, p = ptr + len - 1; n < len; p--, n++) {
		if (n && n % 8 == 0)
			printf("\n");
		for (i = NBBY - 1; i >= 0; i--)
			putchar((*p & (1 << i)) ? '1': '0');
		if (n % 8 != 7 && n != len - 1)
			putchar(' ');
	}
	putchar('\n');
	funlockfile(stdout);
}
