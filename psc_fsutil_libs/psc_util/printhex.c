/* $Id$ */

#include <stdint.h>
#include <stdio.h>

#include "psc_util/lock.h"

void
printhex(void *ptr, size_t len)
{
	static psc_spinlock_t lk = LOCK_INITIALIZER;
	unsigned char *p = ptr;
	size_t n;

	spinlock(&lk);
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
	freelock(&lk);
}

void
printbin(uint64_t val)
{
	static psc_spinlock_t lk = LOCK_INITIALIZER;
	int i;

	spinlock(&lk);
	for (i = (int)sizeof(val) * NBBY - 1; i >= 0; i--)
		putchar(val & (UINT64_C(1) << i) ? '1': '0');
	putchar('\n');
	freelock(&lk);
}
