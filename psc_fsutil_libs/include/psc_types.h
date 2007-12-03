/* $Id: types.h 1894 2007-10-16 23:42:25Z pauln $ */

#if (!defined HAVE_PSC_TYPES_INC)
#define HAVE_PSC_TYPES_INC 1

typedef unsigned int		u32;
typedef unsigned short int	u16;
typedef unsigned char		u8;

typedef int			s32;

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
typedef u64 zaddr_t;

# define UINT64CONST(x) ((u64) x##L)

/* printf(3) specifier modifiers for 64-bit types. */
# define ZLPX64 "lx"
# define ZLPU64 "lu"
/*
 * Lustre user mode stuff insists on using long long everywhere
 */
# define ZLLPX64 "Lx"
# define ZLLPU64 "Lu"


# define _P_OFFX "lx"
# define _P_OFFD "ld"

#else /* i386 */
typedef unsigned long long int u64;
typedef u32 zaddr_t;

# define UINT64CONST(x) ((u64) x##LL)

# define ZLPX64 "llx"
# define ZLPU64 "llu"

# define _P_OFFX "llx"
# define _P_OFFD "lld"

#endif

#define ZPATH_MAX 1024

/*
 * Note that incrementing this pointer must
 *  result in an increase of 1.
 */
typedef void psc_buffer_t;
typedef	u64 psc_crc_t;
typedef	u64 psc_magic_t;
typedef u32 psc_block_id_t; /* really only 24 bits, this + diskid == 32bits */
typedef u16 psc_node_id_t;
typedef u16 psc_disk_id_t;  /* can we get away with 1 byte here? - try to.. */

#define RBLD_DISK_ID ((1 << sizeof(psc_disk_id_t)) - 1)
#define RBLD_BLOCK_ID ((1 << sizeof(psc_block_id_t)) - 1)

#define CRCSZ (sizeof(psc_crc_t))

struct psc_iov {
	u64 ziov_flogical_offset;
	u32 ziov_len;
} __attribute__ ((packed));
typedef struct psc_iov psc_iov_t;

#define MASK_UPPER32 0x00000000ffffffffULL

#endif
