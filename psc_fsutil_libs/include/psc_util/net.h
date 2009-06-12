/* $Id$ */

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
