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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pfl/waitq.h"

struct l_wait_info {
	time_t lwi_timeout;
	long   lwi_interval;
	int  (*lwi_on_timeout)(void *);
	void (*lwi_on_signal)(void *);
	void  *lwi_cb_data;
};

/* NB: LWI_TIMEOUT ignores signals completely */
#define LWI_TIMEOUT(time, cb, data)             \
((struct l_wait_info) {                         \
	.lwi_timeout    = time,                 \
	.lwi_on_timeout = cb,                   \
	.lwi_cb_data    = data,                 \
	.lwi_interval   = 0                     \
})

#define  __zest_server_wait_event(wq, condition, info, ret, excl, lck)       \
do {                                                                         \
	time_t __now       = time(NULL);                                     \
	time_t __timeout   = info->lwi_timeout;                              \
	time_t __then      = 0;                                              \
	int    __timed_out = 0;                                              \
	struct timespec abstime = {0, 0};                                    \
									     \
	ret = 0;                                                             \
	if (condition) break;                                                \
									     \
	while (!condition) {                                                 \
		if (__timeout)                                               \
		       abstime.tv_sec = __timeout + __now;                   \
		abstime.tv_nsec = 0;                                         \
		ret = psc_waitq_timedwait(wq, lck, &abstime);                   \
		if (ret) {                                                   \
			ret = -ret;                                          \
			break;                                               \
		}                                                            \
		if (condition) break;                                        \
									     \
		if (!__timed_out && info->lwi_timeout != 0) {                \
			__now = time(NULL);                                  \
			__timeout -= __now - __then;                         \
			__then = __now;                                      \
									     \
			if (__timeout > 0) continue;                         \
			__timeout = 0;                                       \
			__timed_out = 1;                                     \
			if (info->lwi_on_timeout == NULL ||                  \
			    info->lwi_on_timeout(info->lwi_cb_data)) {       \
				ret = -ETIMEDOUT;                            \
				break;                                       \
			}                                                    \
		}                                                            \
	}                                                                    \
} while (0)

#define psc_svr_wait_event(wq, condition, info, lck)                       \
({                                                                      \
	int                 __ret;                                      \
	struct l_wait_info *__info = (info);                            \
									\
	__zest_server_wait_event(wq, condition, __info, __ret, 0, lck); \
	__ret;                                                          \
})


int
main(void)
{
	struct l_wait_info lwi;
	struct psc_waitq wq;
	int rc;

	psc_waitq_init(&wq);

	lwi = LWI_TIMEOUT(1, NULL, NULL);
	rc = psc_svr_wait_event(&wq, 0, &lwi, NULL);

	printf("rc=%d [ETIMEDOUT=%d, EINVAL=%d, EPERM=%d]\n%s\n",
	       rc, ETIMEDOUT, EINVAL, EPERM, strerror(-rc));

	return 0;
}

