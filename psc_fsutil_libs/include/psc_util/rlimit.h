/* $Id$ */

extern psc_spinlock_t psc_rlimit_lock;

int psc_setrlimit(int, rlim_t, rlim_t);
int psc_getrlimit(int, rlim_t *, rlim_t *);
