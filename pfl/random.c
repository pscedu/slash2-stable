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

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/random.h"

#define _PATH_URANDOM	"/dev/urandom"

#define RBUFSZ		4096

/*
 * Get some data random data.  An internal buffer is used and
 * continually refilled until the request has been satisfied.
 */
void
pfl_random_getbytes(void *p, size_t len)
{
	static unsigned char buf[RBUFSZ], *pos = buf + sizeof(buf);
	static psc_spinlock_t lock = SPINLOCK_INIT;
	static int fd = -1;
	size_t rem, amt;
	ssize_t rc;

	spinlock(&lock);
	if (fd == -1) {
		fd = open(_PATH_URANDOM, O_RDONLY, 0);
		if (fd == -1)
			psc_fatal("open %s", _PATH_URANDOM);
	}
	for (rem = len; rem; rem -= amt, pos += amt, p += amt) {
		if (pos >= buf + sizeof(buf)) {
			rc = read(fd, buf, sizeof(buf));
			if (rc == -1)
				psc_fatal("read %s", _PATH_URANDOM);
			if (rc == 0)
				psc_fatalx("EOF on %s", _PATH_URANDOM);
			// XXX check early return
			pos = buf;
		}

		amt = MIN(rem, sizeof(buf) - (pos - buf));
		memcpy(p, pos, amt);
	}
	freelock(&lock);
}

/*
 * Get a random 32-bit number from /dev/urandom.
 */
uint32_t
psc_random32(void)
{
	uint32_t r;

	pfl_random_getbytes(&r, sizeof(r));
	return (r);
}

/*
 * Get a random 64-bit number from /dev/urandom.
 */
uint64_t
psc_random64(void)
{
	uint64_t r;

	pfl_random_getbytes(&r, sizeof(r));
	return (r);
}

/*
 * Get a uniformly distributed random 32-bit number between [0,@max).
 * @max: one beyond the upper bound.
 */
uint32_t
psc_random32u(uint32_t max)
{
	uint32_t r, min;

	if (max < 2)
		return (0);

	min = UINT64_C(0x100000000) % max;

	for (;;) {
		r = psc_random32();
		if (r >= min)
			break;
	}
	return (r % max);
}
