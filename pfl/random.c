/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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
 * Get a byte from our random data buffer and refill our buffer if
 * needed.
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
			pos = buf;
		}

		amt = MIN(len, sizeof(buf) - (pos - buf));
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
 * Get a uniformly distributed random 32-bit number from /dev/urandom.
 * @max: bound.
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
