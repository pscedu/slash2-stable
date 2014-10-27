/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2014, Pittsburgh Supercomputing Center (PSC).
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

#include "pfl/lock.h"

/**
 * pfl_unpack_hex - Display the hexadecimal representation of some data.
 * @ptr: data to display.
 * @len: number of bytes to display.
 * @buf: buffer to place representation in; must be at least 2 * @len +
 *	1 (for NUL);
 */
void
pfl_unpack_hex(const void *ptr, size_t len, char buf[])
{
	const unsigned char *p = ptr;
	const char tab[] = "0123456789abcdef";
	char *t = buf;
	size_t n;

	for (n = 0; n < len; p++, n++) {
		*t++ = tab[*p >> 4];
		*t++ = tab[*p & 0xf];
	}
	*t = '\0';
}

/**
 * printhex - Display the hexadecimal representation of some data.
 * @ptr: data to display.
 * @len: number of bytes to display.
 */
void
fprinthex(FILE *fp, const void *ptr, size_t len)
{
	const unsigned char *p = ptr;
	size_t n;

	flockfile(fp);
	for (n = 0; n < len; p++, n++) {
		if (n) {
			if (n % 32 == 0)
				fprintf(stderr, "\n");
			else {
				if (n % 8 == 0)
					fprintf(stderr, " ");
				fprintf(stderr, " ");
			}
		}
		fprintf(stderr, "%02x", *p);
	}
	fprintf(stderr, "\n------------------------------------------\n");
	funlockfile(fp);
}

/**
 * printvbinr - Display the bit representation of some data in reverse.
 * @ptr: data to display.
 * @len: number of bytes to display.
 */
void
printvbinr(const void *ptr, size_t len)
{
	const unsigned char *p;
	size_t n;
	int i;

	flockfile(stdout);
	for (n = 0, p = (const unsigned char *)ptr + len - 1; n < len;
	    p--, n++) {
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

/**
 * printvbin - Display the bit representation of some data.
 * @ptr: data to display.
 * @len: number of bytes to display.
 */
void
printvbin(const void *ptr, size_t len)
{
	const unsigned char *p = ptr;
	size_t n;
	int i;

	flockfile(stdout);
	for (n = 0, p = ptr; n < len; p++, n++) {
		if (n && n % 8 == 0)
			printf("\n");
		for (i = 0; i < NBBY; i++)
			putchar((*p & (1 << i)) ? '1': '0');
		if (n % 8 != 7 && n != len - 1)
			putchar(' ');
	}
	putchar('\n');
	funlockfile(stdout);
}
