/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_NET_H_
#define _PFL_NET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#ifdef HAVE_NET_IF_DL_H
# include <net/if_dl.h>
#endif

#ifdef HAVE_GETIFADDRS
# include <ifaddrs.h>
#else
struct ifaddrs {
};
#endif

#ifdef MSG_NOSIGNAL
# define PFL_MSG_NOSIGNAL MSG_NOSIGNAL
#else
# define PFL_MSG_NOSIGNAL 0
#endif

#define PFL_HOSTNAME_MAX	256

union pfl_sockaddr {
	struct sockaddr_storage	 ss;
	struct sockaddr_in6	 sin6;
	struct sockaddr_in	 sin;
#ifdef HAVE_NET_IF_DL_H
	struct sockaddr_dl	 sdl;
#endif
	struct sockaddr		 sa;
};

union pfl_sockaddr_ptr {
	union pfl_sockaddr	*s;
	void			*p;
	const void		*cp;
};

#ifdef __APPLE__
# define SOCKADDR_ALIGNSZ	sizeof(int32_t)
#else
# define SOCKADDR_ALIGNSZ	sizeof(long)
#endif

#ifdef HAVE_SA_LEN

# define SOCKADDR_GETLEN(sa)						\
	((sa)->sa_len ? PSC_ALIGN((sa)->sa_len, SOCKADDR_ALIGNSZ) :	\
	 SOCKADDR_ALIGNSZ)

# define SOCKADDR_SETLEN(sa)						\
	(((struct sockaddr *)(sa))->sa_len = sizeof(*(sa)))

#else

/*
 * XXX this argument can't be of type 'struct sockaddr' or this value will be
 * wrong...
 */
# define SOCKADDR_GETLEN(sa)						\
	(PSC_ALIGN(sizeof(*(sa)), SOCKADDR_ALIGNSZ))

# define SOCKADDR_SETLEN(sa)

#endif

int  pfl_socket_getpeercred(int, uid_t *, gid_t *);
void pfl_socket_setnosig(int);

void pflnet_freeifaddrs(struct ifaddrs *);
int  pflnet_getifaddr(const struct ifaddrs *, const char *, union pfl_sockaddr *);
int  pflnet_getifaddrs(struct ifaddrs **);
int  pflnet_getifnfordst(const struct ifaddrs *, const struct sockaddr *, char []);
int  pflnet_rtexists(const struct sockaddr *);

#endif /* _PFL_NET_H_ */
