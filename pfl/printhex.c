/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>

#include <stdint.h>
#include <stdio.h>

/*
 * Display the hexadecimal representation of some data.
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

/*
 * Display the hexadecimal representation of some data.
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
				fprintf(fp, "\n");
			else {
				if (n % 8 == 0)
					fprintf(fp, " ");
				fprintf(fp, " ");
			}
		}
		fprintf(fp, "%02x", *p);
	}
	fprintf(fp, "\n------------------------------------------\n");
	funlockfile(fp);
}

/*
 * Display the bit representation of some data in reverse.
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

/*
 * Display the bit representation of some data.
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
