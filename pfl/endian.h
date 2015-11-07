/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_ENDIAN_H_
#define _PFL_ENDIAN_H_

#ifdef HAVE_ENDIAN_H
# include <endian.h>
#else
# include <machine/endian.h>
#endif

#ifdef _BYTE_ORDER
# define PFL_BYTE_ORDER _BYTE_ORDER
#elif defined(__BYTE_ORDER)
# define PFL_BYTE_ORDER __BYTE_ORDER
#else
# error BYTE_ORDER unavailable
#endif

#define PFL_SWAP16(x)							\
	(((((uint16_t)x) & 0x00ff) << 8) |				\
	 ((((uint16_t)x) & 0xff00) >> 8))						\

#define PFL_SWAP32(x)							\
	(((((uint32_t)x) & 0x000000ff) << 24) |				\
	 ((((uint32_t)x) & 0x0000ff00) <<  8) |				\
	 ((((uint32_t)x) & 0x00ff0000) >>  8) |				\
	 ((((uint32_t)x) & 0xff000000) >> 24))						\

#if PFL_BYTE_ORDER == LITTLE_ENDIAN

# ifndef le32toh
#  define le32toh(x)	(x)
# endif

# ifndef le16toh
#  define le16toh(x)	(x)
# endif

#elif PFL_BYTE_ORDER == BIG_ENDIAN

# ifndef le32toh
#  define le32toh(x)	(x)	PFL_SWAP32(x)
# endif

# ifndef le16toh
# define le16toh(x)	(x)	PFL_SWAP16(x)
# endif

#else
# error unknown BYTE_ORDER
#endif

#endif /* _PFL_ENDIAN_H_ */
