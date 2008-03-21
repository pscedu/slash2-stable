/* $Id$ */

#include <stdio.h>

#include "psc_util/lock.h"

void
printhex(void *ptr, size_t len)
{
	static psc_spinlock_t l = LOCK_INITIALIZER;
	unsigned char *p = ptr;
	size_t n;

	spinlock(&l);
	for (n = 0; n < len; p++, n++) {
		if (n) {
			if (n % 32 == 0)
				printf("\n");
			else {
				if (n % 8 == 0)
					printf(" ");
				printf(" ");
			}
		}
		printf("%02x", *p);
	}
printf("\n------------------------------------------\n");
	freelock(&l);
}
