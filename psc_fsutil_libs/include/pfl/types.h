/* $Id$ */

#ifndef _PFL_TYPES_H_
#define _PFL_TYPES_H_

#include <inttypes.h>

typedef uint32_t	u32;

/* printf(3) specifier modifiers for custom types. */
#define PRIxLNID	PRIx64
#define PRIxOFF		PRIx64
#define PRIdOFF		PRId64
#define PRIxCRC		PRIx64
#define PRI_PSC_CRC	"%016"PRIx64

#include "psc_util/subsys.h"

#endif /* _PFL_TYPES_H_ */
