/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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
void pflnet_getifnfordst(const struct ifaddrs *, const struct sockaddr *, char []);
int  pflnet_rtexists(const struct sockaddr *);

#endif /* _PFL_NET_H_ */
