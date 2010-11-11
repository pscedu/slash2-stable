/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL__ATOMIC32_H_
#define _PFL__ATOMIC32_H_

#include <stdint.h>

#include "pfl/cdefs.h"

struct psc_atomic32 { volatile int32_t value32; } __packed;

#define psc_atomic32_t struct psc_atomic32

#endif /* _PFL__ATOMIC32_H_ */
