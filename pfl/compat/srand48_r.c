/* $Id$ */

#include <stdio.h>
#include <stdlib.h>

#include "pfl/lock.h"

struct drand48_data;

psc_spinlock_t pfl_srand48_lock = SPINLOCK_INIT;

int
srand48_r(long seed, struct drand48_data *buf)
{
	int rc = 0;

	(void)buf;
	spinlock(&pfl_srand48_lock);
	srand48(seed);
	freelock(&pfl_srand48_lock);
	return (rc);
}
