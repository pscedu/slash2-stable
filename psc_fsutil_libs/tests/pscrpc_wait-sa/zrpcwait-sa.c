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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "psc_util/waitq.h"

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

