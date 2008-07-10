/* $Id$ */

#ifndef __PFL_TYPES_H__
#define __PFL_TYPES_H__

typedef unsigned int		u32;
typedef unsigned short int	u16;
typedef unsigned char		u8;

typedef signed int		s32;
typedef signed short int	s16;
typedef signed char		s8;

#define U8_MAX	0xffU
#define U16_MAX	0xffffU
#define U32_MAX	0xffffffffU
#define U64_MAX	0xffffffffffffffffULL

/*
 * This is bad, perhaps it can be solved better
 *  with a configure script?
 */
#if __x86_64
typedef unsigned long int u64;

# define U64CONST(x) ((u64)(x##L))

/* printf(3) specifier modifiers for 64-bit types. */
# define _P_U64 "l"
# define _P_OFFT "l"

#else /* i386 */
typedef unsigned long long int u64;

# define U64CONST(x) ((u64)(x##LL))

# define _P_U64 "ll"
# define _P_OFFT "ll"

#endif

#define _P_LU64 "L"

typedef	u64 psc_crc_t;

#define CRCSZ (sizeof(psc_crc_t))

#define MASK_UPPER32 0x00000000ffffffffULL

#include "psc_util/subsys.h"

#endif /* __PFL_TYPES_H__ */
