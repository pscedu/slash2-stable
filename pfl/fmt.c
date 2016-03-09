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

#include <sys/stat.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/fmt.h"

void
pfl_fmt_human(char buf[PSCFMT_HUMAN_BUFSIZ], double num)
{
	int mag;

	/*
	 * 1234567
	 * 1000.3K
	 */
	for (mag = 0; num >= 1024.0; mag++)
		num /= 1024.0;
	if (mag > 6)
		snprintf(buf, PSCFMT_HUMAN_BUFSIZ, "%.1e", num);
	else if (mag == 0)
		snprintf(buf, PSCFMT_HUMAN_BUFSIZ, "%6dB", (int)(num + .5));
	else
		snprintf(buf, PSCFMT_HUMAN_BUFSIZ, "%6.1f%c", num, "BKMGTPE"[mag]);
}

void
pfl_fmt_ratio(char buf[PSCFMT_RATIO_BUFSIZ], int64_t n, int64_t d)
{
	double val;

	if (n == d)
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "100%%");
	else if (n == 0)
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "0%%");
	else if (d == 0)
		snprintf(buf, PSCFMT_RATIO_BUFSIZ, "<und>");
	else {
		val = n * 100.0 / d;

		if (val > 99.99)
			/*
			 * This rounds to 100.00 but that looks silly
			 * despite being technically more correct.
			 */
			snprintf(buf, PSCFMT_RATIO_BUFSIZ, "99.99%%");
		else
			snprintf(buf, PSCFMT_RATIO_BUFSIZ, "%.2f%%", val);
	}
}

ssize_t
pfl_humantonum(const char *s)
{
	ssize_t m = 1;
	char *endp;
	double sz;

	sz = strtod(s, &endp);

	if (s == endp || sz == HUGE_VAL)
		return (-EINVAL);

	if (sz < 0)
		return (-ERANGE);

	switch (tolower(*endp)) {
	case 'k':
		m = 1024;
		break;
	case 'm':
		m = 1024 * 1024;
		break;
	case 'g':
		m = 1024 * 1024 * 1024;
		break;
	case 't':
		m = 1024 * 1024 * 1024 * INT64_C(1024);
		break;
	case '\0':
		break;
	default:
		return (-EINVAL);
	}
	if (sz * m < sz)
		return (-ERANGE);
	return (sz * m);
}

const char *
pfl_fmt_mode(mode_t m, char buf[11])
{
	char *p = buf;

	if (S_ISDIR(m))
		*p++ = 'd';
	else if (S_ISCHR(m))
		*p++ = 'c';
	else if (S_ISBLK(m))
		*p++ = 'b';
	else if (S_ISSOCK(m))
		*p++ = 's';
	else if (S_ISDIR(m))
		*p++ = 'd';
	else if (S_ISLNK(m))
		*p++ = 'l';
	else if (S_ISFIFO(m))
		*p++ = 'f';
	else if (S_ISREG(m))
		*p++ = '-';
	else
		*p++ = '?';

	*p++ = m & S_IRUSR ? 'r' : '-';
	*p++ = m & S_IWUSR ? 'w' : '-';
	if (m & S_ISUID)
		*p++ = m & S_IXUSR ? 's' : 'S';
	else
		*p++ = m & S_IXUSR ? 'x' : '-';

	*p++ = m & S_IRGRP ? 'r' : '-';
	*p++ = m & S_IWGRP ? 'w' : '-';
	if (m & S_ISGID)
		*p++ = m & S_IXGRP ? 's' : 'S';
	else
		*p++ = m & S_IXGRP ? 'x' : '-';

	*p++ = m & S_IROTH ? 'r' : '-';
	*p++ = m & S_IWOTH ? 'w' : '-';
	if (m & S_ISVTX)
		*p++ = m & S_IXOTH ? 't' : 'T';
	else
		*p++ = m & S_IXOTH ? 'x' : '-';
	*p = '\0';
	return (buf);
}
