/* $Id$ */

#include <stdint.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	int64_t val, oldv, newv, cmpv;

	(void)argc;
	(void)argv;

	val = 42;
	cmpv = 42;
	newv = 53;
	__asm__ __volatile__("lock; cmpxchg8b %3" :
	    "=A" (oldv) : "b" ((int32_t)newv),
	    "c" (newv >> 32), "m" (val),
	    "0" (cmpv) : "memory");
	exit(0);
}
