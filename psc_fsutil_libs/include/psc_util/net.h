/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_NET_H_
#define _PFL_NET_H_

#define p_ntoh_u16(v) ntoh16(v)
#define p_ntoh_u32(v) ntoh32(v)

#define p_hton_u16(v) hton16(v)
#define p_hton_u32(v) hton32(v)

#define p_ntoh_s16(v) ntoh16(v)
#define p_ntoh_s32(v) ntoh32(v)

#define p_hton_s16(v) hton16(v)
#define p_hton_s32(v) hton32(v)

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define p_ntoh_u64(v)		((((v) & UINT64_C(0x00000000000000ff)) << 56) |	\
				 (((v) & UINT64_C(0x000000000000ff00)) << 40) |	\
				 (((v) & UINT64_C(0x0000000000ff0000)) << 24) |	\
				 (((v) & UINT64_C(0x00000000ff000000)) <<  8) |	\
				 (((v) & UINT64_C(0x000000ff00000000)) >>  8) |	\
				 (((v) & UINT64_C(0x0000ff0000000000)) >> 24) |	\
				 (((v) & UINT64_C(0x00ff000000000000)) >> 40) |	\
				 (((v) & UINT64_C(0xff00000000000000)) >> 56))
# define p_hton_u64(v) p_ntoh_u64(v)

# define p_ntoh_s64(v) ERROR
# define p_hton_s64(v) ERROR
#else
# define p_ntoh_u64(v) (v)
# define p_hton_u64(v) (v)

# define p_ntoh_s64(v) (v)
# define p_hton_s64(v) (v)
#endif

#endif /* _PFL_NET_H_ */
