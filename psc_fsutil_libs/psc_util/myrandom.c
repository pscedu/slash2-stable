/* $Id$ */

#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_types.h"
#include "psc_util/myrandom.h"

#define _PATH_URANDOM "/dev/urandom"
#define SMALL_BUF 128

psc_spinlock_t	 mrlock = LOCK_INITIALIZER;
unsigned char	 mrbuf[SMALL_BUF];
ssize_t		 mrsiz;
unsigned char	*mrp;

/**
 * myrandom_getbyte: get a byte from our random data buffer and refill
 *	our buffer if needed.
 * Notes: not reentrant!
 */
__static u64
myrandom_getbyte(void)
{
	int fd;

	if (mrp && mrp >= mrbuf + mrsiz)
		mrp = NULL;

	if (mrp == NULL) {
		fd = open(_PATH_URANDOM, O_RDONLY, 0);
		if (fd == -1)
			psc_fatal("open %s", _PATH_URANDOM);
		mrsiz = read(fd, mrbuf, sizeof(mrbuf));
		if (mrsiz == -1)
			psc_fatal("read %s", _PATH_URANDOM);
		if (mrsiz == 0)
			psc_fatalx("EOF on %s", _PATH_URANDOM);
		mrp = mrbuf;
		close(fd);
	}
	return (*mrp++);
}

/**
 * myrandom32: get a random 32-bit number from /dev/urandom.
 */
u32
myrandom32(void)
{
	u32 r;

	spinlock(&mrlock);
	r = myrandom_getbyte();
	r |= myrandom_getbyte() << 8;
	r |= myrandom_getbyte() << 16;
	r |= myrandom_getbyte() << 24;
	freelock(&mrlock);
	return (r);
}

/**
 * myrandom64: get a random 64-bit number from /dev/urandom.
 */
u64
myrandom64(void)
{
	u64 r;

	spinlock(&mrlock);
	r = myrandom_getbyte();
	r |= myrandom_getbyte() << 8;
	r |= myrandom_getbyte() << (8*2);
	r |= myrandom_getbyte() << (8*3);
	r |= myrandom_getbyte() << (8*4);
	r |= myrandom_getbyte() << (8*5);
	r |= myrandom_getbyte() << (8*6);
	r |= myrandom_getbyte() << (8*7);
	freelock(&mrlock);
	return (r);
}
