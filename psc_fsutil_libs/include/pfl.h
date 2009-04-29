/* $Id$ */

#ifndef _PFL_H_
#define _PFL_H_

#include "psc_util/lock.h"

extern psc_spinlock_t psc_umask_lock;

void pfl_init(void);

#endif /* _PFL_H_ */
