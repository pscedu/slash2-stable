#ifndef _LUSTRE_TYPES_H
#define _LUSTRE_TYPES_H

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif

#if __WORDSIZE == 64
# define __u64 __uint64_t
#endif

#endif /*_LUSTRE_TYPES_H */
