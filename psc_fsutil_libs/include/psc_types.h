/* $Id$ */

#ifndef __PFL_TYPES_H__
#define __PFL_TYPES_H__

#include <inttypes.h>

typedef uint64_t	u64;
typedef uint32_t	u32;
typedef uint16_t	u16;
typedef uint8_t		u8;

typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;

/* printf(3) specifier modifiers for 64-bit types. */
#if defined(__x86_64) || defined(__ia64)
# define PRIlnidx	"lx"
# define PRIoff		"l"
# define _P_OFFT	"l"
# define _P_U64		"l"
#else
# define PRIlnidx	"Lx"
# define PRIoff		"ll"
# define _P_OFFT	"ll"
# define _P_U64		"ll"
#endif

typedef	u64 psc_crc_t;

#define CRCSZ (sizeof(psc_crc_t))

#include "psc_util/subsys.h"

#endif /* __PFL_TYPES_H__ */
