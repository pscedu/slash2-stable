/* $Id$ */

#ifndef _PFL_TYPES_H_
#define _PFL_TYPES_H_

#include <inttypes.h>

typedef uint64_t	u64;
typedef uint32_t	u32;
typedef uint16_t	u16;
typedef uint8_t		u8;

typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;

/* printf(3) specifier modifiers for custom types. */
#define PRIxLNID	PRIx64
#define PRIxOFF		PRIx64
#define PRIdOFF		PRId64

typedef	uint64_t psc_crc_t;

#define CRCSZ (sizeof(psc_crc_t))

#include "psc_util/subsys.h"

#endif /* _PFL_TYPES_H_ */
