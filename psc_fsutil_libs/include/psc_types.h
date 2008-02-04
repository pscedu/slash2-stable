/* $Id$ */

#ifndef _PFL_TYPES_H_
#define _PFL_TYPES_H_

#include "psc_util/subsys.h"

typedef unsigned int		u32;
typedef unsigned short int	u16;
typedef unsigned char		u8;

typedef signed int		s32;

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

# define UINT64CONST(x) ((u64) x##L)

/* printf(3) specifier modifiers for 64-bit types. */
# define _P_LP64 "l"
# define _P_OFFT "l"

#else /* i386 */
typedef unsigned long long int u64;

# define UINT64CONST(x) ((u64) x##LL)

# define _P_LP64 "ll"
# define _P_OFFT "ll"

#endif

typedef	u64 psc_crc_t;

#define CRCSZ (sizeof(psc_crc_t))

#define MASK_UPPER32 0x00000000ffffffffULL

#endif /* _PFL_TYPES_H_ */
