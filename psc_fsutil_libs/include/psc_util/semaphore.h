/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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
