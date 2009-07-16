/* $Id$ */

#ifndef _PFL_RLIMIT_H_
#define _PFL_RLIMIT_H_

#include "sys/resource.h"

int psc_setrlimit(int, rlim_t, rlim_t);
int psc_getrlimit(int, rlim_t *, rlim_t *);

extern psc_spinlock_t psc_rlimit_lock;

#endif
