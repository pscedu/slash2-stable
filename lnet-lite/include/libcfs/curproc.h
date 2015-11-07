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
 * lnet/include/libcfs/curproc.h
 *
 * Lustre curproc API declaration
 *
 * Author: Nikita Danilov <nikita@clusterfs.com>
 */

#ifndef __LIBCFS_CURPROC_H__
#define __LIBCFS_CURPROC_H__

#ifdef __KERNEL__
/*
 * Portable API to access common characteristics of "current" UNIX process.
 *
 * Implemented in portals/include/libcfs/<os>/
 */
uid_t  cfs_curproc_uid(void);
gid_t  cfs_curproc_gid(void);
uid_t  cfs_curproc_euid(void);
gid_t  cfs_curproc_egid(void);
uid_t  cfs_curproc_fsuid(void);
gid_t  cfs_curproc_fsgid(void);
pid_t  cfs_curproc_pid(void);
int    cfs_curproc_groups_nr(void);
int    cfs_curproc_is_in_groups(gid_t group);
void   cfs_curproc_groups_dump(gid_t *array, int size);
mode_t cfs_curproc_umask(void);
char  *cfs_curproc_comm(void);


/*
 * Plus, platform-specific constant
 *
 * CFS_CURPROC_COMM_MAX,
 *
 * and opaque scalar type
 *
 * cfs_kernel_cap_t
 */
#endif

typedef __u32 cfs_cap_t;

/* check if task is running in compat mode.*/
int cfs_curproc_is_32bit(void);

#define CFS_CAP_CHOWN                   0
#define CFS_CAP_DAC_OVERRIDE            1
#define CFS_CAP_DAC_READ_SEARCH         2
#define CFS_CAP_FOWNER                  3
#define CFS_CAP_FSETID                  4
#define CFS_CAP_LINUX_IMMUTABLE         9
#define CFS_CAP_SYS_ADMIN              21
#define CFS_CAP_SYS_BOOT               23
#define CFS_CAP_SYS_RESOURCE           24

#define CFS_CAP_FS_MASK ((1 << CFS_CAP_CHOWN) |                 \
                         (1 << CFS_CAP_DAC_OVERRIDE) |          \
                         (1 << CFS_CAP_DAC_READ_SEARCH) |       \
                         (1 << CFS_CAP_FOWNER) |                \
                         (1 << CFS_CAP_FSETID ) |               \
                         (1 << CFS_CAP_LINUX_IMMUTABLE) |       \
                         (1 << CFS_CAP_SYS_ADMIN) |             \
                         (1 << CFS_CAP_SYS_BOOT) |              \
                         (1 << CFS_CAP_SYS_RESOURCE))

void cfs_cap_raise(cfs_cap_t cap);
void cfs_cap_lower(cfs_cap_t cap);
int cfs_cap_raised(cfs_cap_t cap);
void cfs_kernel_cap_pack(cfs_kernel_cap_t kcap, cfs_cap_t *cap);
void cfs_kernel_cap_unpack(cfs_kernel_cap_t *kcap, cfs_cap_t cap);
cfs_cap_t cfs_curproc_cap_pack(void);
void cfs_curproc_cap_unpack(cfs_cap_t cap);
int cfs_capable(cfs_cap_t cap);

/* __LIBCFS_CURPROC_H__ */
#endif
/*
 * Local variables:
 * c-indentation-style: "K&R"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 80
 * scroll-step: 1
 * End:
 */
