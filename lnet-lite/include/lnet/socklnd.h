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
 * lnet/include/lnet/socklnd.h
 *
 * #defines shared between socknal implementation and utilities
 */
#ifndef __LNET_LNET_SOCKLND_H__
#define __LNET_LNET_SOCKLND_H__

#include <lnet/types.h>
#include <lnet/lib-types.h>

#define SOCKLND_CONN_NONE     (-1)
#define SOCKLND_CONN_ANY        0
#define SOCKLND_CONN_CONTROL    1
#define SOCKLND_CONN_BULK_IN    2
#define SOCKLND_CONN_BULK_OUT   3
#define SOCKLND_CONN_NTYPES     4

#define SOCKLND_CONN_ACK        SOCKLND_CONN_BULK_IN

typedef struct {
        __u32                   kshm_magic;     /* magic number of socklnd message */
        __u32                   kshm_version;   /* version of socklnd message */
        lnet_nid_t              kshm_src_nid;   /* sender's nid */
        lnet_nid_t              kshm_dst_nid;   /* destination nid */
        lnet_pid_t              kshm_src_pid;   /* sender's pid */
        lnet_pid_t              kshm_dst_pid;   /* destination pid */
        __u64                   kshm_src_incarnation; /* sender's incarnation */
        __u64                   kshm_dst_incarnation; /* destination's incarnation */
        __u32                   kshm_ctype;     /* connection type */
        __u32                   kshm_nips;      /* # IP addrs */
        __u32                   kshm_ips[0];    /* IP addrs */
} WIRE_ATTR ksock_hello_msg_t;

typedef struct {
        lnet_hdr_t              ksnm_hdr;       /* lnet hdr */
        char                    ksnm_payload[0];/* lnet payload */
} WIRE_ATTR ksock_lnet_msg_t;

typedef struct {
        __u32                   ksm_type;       /* type of socklnd message */
        __u32                   ksm_csum;       /* checksum if != 0 */
        __u64                   ksm_zc_cookies[2]; /* Zero-Copy request/ACK cookie */
        union {
                ksock_lnet_msg_t lnetmsg;       /* lnet message, it's empty if it's NOOP */
        } WIRE_ATTR ksm_u;
} WIRE_ATTR ksock_msg_t;

static inline void
socklnd_init_msg(ksock_msg_t *msg, int type)
{
        msg->ksm_csum           = 0;
        msg->ksm_type           = type;
        msg->ksm_zc_cookies[0]  = msg->ksm_zc_cookies[1]  = 0;
}

#define KSOCK_MSG_NOOP          0xc0            /* ksm_u empty */
#define KSOCK_MSG_LNET          0xc1            /* lnet msg */

/* We need to know this number to parse hello msg from ksocklnd in
 * other LND (usocklnd, for example) */
#define KSOCK_PROTO_V2          2
#define KSOCK_PROTO_V3          3

#endif
