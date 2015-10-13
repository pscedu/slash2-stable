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
 */

#ifndef __LIBCFS_USER_TCPIP_H__
#define __LIBCFS_USER_TCPIP_H__

#ifndef __LIBCFS_LIBCFS_H__
#error Do not #include this file directly. #include <libcfs/libcfs.h> instead
#endif

#ifndef __KERNEL__

#include <sys/types.h>
#include <sys/uio.h>

#include "openssl/ssl.h"

/*
 * Functions to get network interfaces info
 */

int libcfs_sock_ioctl(unsigned long, void *);
int libcfs_ipif_query (char *name, int *up, __u32 *ip);
void libcfs_ipif_free_enumeration (char **names, int n);
int libcfs_ipif_enumerate (char ***namesp);

/*
 * Network function used by user-land lnet acceptor
 */

int libcfs_sock_listen (int *sockp, __u64, __u32 local_ip, int local_port, int backlog);
int libcfs_sock_accept (int *newsockp, int sock, __u32 *peer_ip, int *peer_port);
int libcfs_sock_connect(int fd, __u32 ip, __u16 port);
void libcfs_sock_abort_accept(__u64, __u16 port);

/*
 * Network functions of common use
 */

int libcfs_getpeername(int sock_fd, __u32 *ipaddr_p, __u16 *port_p);
int libcfs_socketpair(int *fdp);
int libcfs_fcntl_nonblock(int fd);
int libcfs_sock_set_keepalive(int, int, int, int, int);
int libcfs_sock_set_nagle(int fd, int nagle);
int libcfs_sock_set_maxseg(int fd, int maxseg);
int libcfs_sock_set_bufsiz(int fd, int bufsiz);
int libcfs_sock_create(int *fdp, __u64);
int libcfs_sock_bind_to_port(int fd, __u64, __u32, __u16 port);

struct lnet_xport;

struct lnet_xport_int {
	int		(*lxi_accept)(struct lnet_xport *, int);
	int		(*lxi_close)(struct lnet_xport *);
	int		(*lxi_connect)(struct lnet_xport *, int);
	int		(*lxi_init)(struct lnet_xport *);
	ssize_t		(*lxi_read)(struct lnet_xport *, void *, size_t, int);
	ssize_t		(*lxi_readv)(struct lnet_xport *, const struct iovec *, int);
	ssize_t		(*lxi_writev)(struct lnet_xport *, const struct iovec *, int);
};

struct lnet_xport {
	int			 lx_fd;
	SSL			*lx_ssl;
	struct lnet_xport_int	*lx_tab;
};

struct lnet_xport *
	lx_new(struct lnet_xport_int *);
void	lx_destroy(struct lnet_xport *);

#define lx_init(lx)							\
	do {								\
		if ((lx)->lx_tab->lxi_init)				\
			(lx)->lx_tab->lxi_init(lx);			\
	} while (0)

#define lx_close(lx)							\
	do {								\
		if ((lx)->lx_tab->lxi_close)				\
			(lx)->lx_tab->lxi_close(lx);			\
	} while (0)

#define lx_accept(lx, s)						\
	do {								\
		if ((lx)->lx_tab->lxi_accept)				\
			(lx)->lx_tab->lxi_accept((lx), (s));		\
		(lx)->lx_fd = (s);					\
	} while (0)

#define lx_connect(lx, s)						\
	do {								\
		if ((lx)->lx_tab->lxi_connect)				\
			(lx)->lx_tab->lxi_connect((lx), (s));		\
		(lx)->lx_fd = (s);					\
	} while (0)

#define lx_read(lx, buf, sz, t)	(lx)->lx_tab->lxi_read((lx), (buf), (sz), (t))
#define lx_readv(lx, iov, n)	(lx)->lx_tab->lxi_readv((lx), (iov), (n))
#define lx_writev(lx, iov, n)	(lx)->lx_tab->lxi_writev((lx), (iov), (n))

extern struct lnet_xport_int libcfs_ssl_lxi;
extern struct lnet_xport_int libcfs_sock_lxi;

/*
 * Macros for easy printing IP-adresses
 */

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#if defined(__LITTLE_ENDIAN) || defined(_LITTLE_ENDIAN)
#define HIPQUAD(addr)                \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]
#elif defined(__BIG_ENDIAN) || defined(_BIG_ENDIAN)
#define HIPQUAD NIPQUAD
#else
#error "Undefined byteorder??"
#endif /* __LITTLE_ENDIAN */

#endif /* !__KERNEL__ */

#endif
