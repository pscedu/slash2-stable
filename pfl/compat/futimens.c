/* $Id$ */

#include <sys/time.h>

int
futimens(int fd, const struct timespec ts[2])
{
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;

	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	return (futimes(fd, tv));
}
