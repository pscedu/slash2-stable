/* $Id$ */

#ifndef __PFL_SEMAPHORE_H__
#define __PFL_SEMAPHORE_H__

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define PSEMS_LOCK	(1 << 0)
#define PSEMS_ULOCK	(1 << 1)
#define PSEMS_SHLOCK	(1 << 2)

#define psc_lck_op(sem, type) psc_sem_op((sem), (type))

void psc_sem_op(int, int);
int  psc_sem_init(key_t);

#endif /* __PFL_SEMAPHORE_H__ */
