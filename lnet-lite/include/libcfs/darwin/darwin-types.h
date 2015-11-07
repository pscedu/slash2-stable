/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.lustre.org/lustre/docs/GPLv2.pdf
 *
 * Please contact Xyratex Technology, Ltd., Langstone Road, Havant, Hampshire.
 * PO9 1SA, U.K. or visit www.xyratex.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2002, 2013, Xyratex Technology, Ltd . All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Some portions of Lustre® software are subject to copyrights help by Intel Corp.
 * Copyright (c) 2011-2013 Intel Corporation, Inc.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre® and the Lustre logo are registered trademarks of
 * Xyratex Technology, Ltd  in the United States and/or other countries.
 *
 * lnet/include/libcfs/darwin/darwin-types.h
 *
 * Basic library routines.
 */

#ifndef __LIBCFS_DARWIN_XNU_TYPES_H__
#define __LIBCFS_DARWIN_XNU_TYPES_H__

#ifndef __LIBCFS_LIBCFS_H__
#error Do not #include this file directly. #include <libcfs/libcfs.h> instead
#endif

#ifdef HAVE_MACH_MACH_TYPES_H
#include <mach/mach_types.h>
#endif

#include <sys/types.h>

#ifndef _BLKID_TYPES_H
#define _BLKID_TYPES_H
#endif

typedef uint8_t         __u8;
typedef uint16_t        __u16;
typedef uint32_t        __u32;
typedef uint64_t        __u64;
typedef int8_t          __s8;
typedef int16_t         __s16;
typedef int32_t         __s32;
typedef int64_t         __s64;

#ifdef __KERNEL__

#include <kern/kern_types.h>


typedef struct { int e; }		event_chan_t;
typedef dev_t				kdev_t;

/*
 * Atmoic define
 */
#include <libkern/OSAtomic.h>

typedef struct { volatile uint32_t counter; }	atomic_t;

#define ATOMIC_INIT(i)			{ (i) }
#define atomic_read(a)			((a)->counter)
#define atomic_set(a, v)		(((a)->counter) = (v))
#ifdef __DARWIN8__
/* OS*Atomic return the value before the operation */
#define atomic_add(v, a)		OSAddAtomic(v, (SInt32 *)&((a)->counter))
#define atomic_sub(v, a)		OSAddAtomic(-(v), (SInt32 *)&((a)->counter))
#define atomic_inc(a)			OSIncrementAtomic((SInt32 *)&((a)->counter))
#define atomic_dec(a)			OSDecrementAtomic((SInt32 *)&((a)->counter))
#else /* !__DARWIN8__ */
#define atomic_add(v, a)		hw_atomic_add((__u32 *)&((a)->counter), v)
#define atomic_sub(v, a)		hw_atomic_sub((__u32 *)&((a)->counter), v)
#define atomic_inc(a)			atomic_add(1, a)
#define atomic_dec(a)			atomic_sub(1, a)
#endif /* !__DARWIN8__ */
#define atomic_sub_and_test(v, a)       (atomic_sub(v, a) == (v))
#define atomic_dec_and_test(a)          (atomic_dec(a) == 1)
#define atomic_inc_return(a)            (atomic_inc(a) + 1)
#define atomic_dec_return(a)            (atomic_dec(a) - 1)

#include <libsa/mach/mach.h>
typedef off_t   			loff_t;

#else	/* !__KERNEL__ */

#include <stdint.h>

typedef off_t   			loff_t;

#endif	/* __KERNEL END */
typedef unsigned short                  umode_t;

#endif  /* __XNU_CFS_TYPES_H__ */
