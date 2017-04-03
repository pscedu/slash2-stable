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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/route.h>
#include <arpa/inet.h>

#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#ifdef HAVE_RTNETLINK
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#elif defined(sun)
#else
# include <sys/sysctl.h>
#endif

#ifdef HAVE_GETIFADDRS
# include <net/if.h>

# include <ifaddrs.h>
#endif

#ifdef HAVE_GETPEERUCRED
#include <ucred.h>
#endif

#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "pfl/alloc.h"
#include "pfl/bitflag.h"
#include "pfl/log.h"
#include "pfl/net.h"

/**
 * pfl_socket_setnosig - Try to set "no SIGPIPE" on a socket.
 * @sock: socket file descriptor.
 */
void
pfl_socket_setnosig(int sock)
{
#ifdef SO_NOSIGPIPE
	socklen_t optsiz;
	int optval;

	optval = 1;
	optsiz = sizeof(optval);
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE,
	    &optval, optsiz) == -1)
		psclog_error("setsockopt");
#else
	(void)sock;
#endif
}

/**
 * pfl_socket_getpeercred - Retrieve credentials of process on other end
 *	of a UNIX domain socket.
 * @s: socket file descriptor.
 * @uid: value-result user ID.
 * @gid: value-result group ID.
 */
int
pfl_socket_getpeercred(int s, uid_t *uid, gid_t *gid)
{
#ifdef HAVE_GETPEEREID
	if (getpeereid(s, uid, gid) == -1)
		return (errno);
#elif defined(HAVE_GETPEERUCRED)
	ucred_t *ucr;

	if (getpeerucred(s, &ucr) == -1)
		return (errno);
	*uid = ucred_geteuid(ucr);
	*gid = ucred_getegid(ucr);
	ucred_free(ucr);
#else
	struct ucred ucr;
	socklen_t len;

	len = sizeof(ucr);
	if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &ucr, &len))
		return (errno);
	*uid = ucr.uid;
	*gid = ucr.gid;
#endif
	return (0);
}

/**
 * pflnet_getifaddrs - Acquire list of network interface addresses.
 * @ifap: value-result base of addresses array, must be when finished.
 */
int
pflnet_getifaddrs(struct ifaddrs **ifap)
{
#ifdef HAVE_GETIFADDRS
	return (getifaddrs(ifap));
#else
	int nifs, rc, s, n;
	struct ifconf ifc;
	struct ifreq *ifr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		psc_fatal("socket");

	ifc.ifc_buf = NULL;
	rc = ioctl(s, SIOCGIFCONF, &ifc);
	if (rc == -1)
		psc_fatal("ioctl SIOCGIFCONF");

	/*
	 * If an interface is being added while we are fetching,
	 * there is no way to determine that we didn't get them
	 * all with this API.
	 */
	ifc.ifc_buf = PSCALLOC(ifc.ifc_len);
	rc = ioctl(s, SIOCGIFCONF, &ifc);
	if (rc == -1)
		psc_fatal("ioctl SIOCGIFCONF");

	close(s);

	nifs = ifc.ifc_len / sizeof(*ifr);
	*ifap = PSCALLOC(sizeof(**ifap) * nifs);

	ifr = (void *)ifc.ifc_buf;
	for (n = 0; n < nifs; n+++, ifr++)
		memcpy(*ifap + n, &ifr->ifr_addr, sizeof(**ifap));
#endif
	return (0);
}

/**
 * pflnet_freeifaddrs - Release network interface addresses.
 * @ifa: addresses to free.
 */
void
pflnet_freeifaddrs(struct ifaddrs *ifa)
{
#ifdef HAVE_GETIFADDRS
	freeifaddrs(ifa);
#else
	PSCFREE(ifa);
#endif
}

#ifdef HAVE_RTNETLINK
__static void
pflnet_getifname(int ifidx, char ifn[IFNAMSIZ])
{
	struct ifreq ifr;
	int rc, s;

	ifr.ifr_ifindex = ifidx;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		psc_fatal("socket");

	rc = ioctl(s, SIOCGIFNAME, &ifr);
	if (rc == -1)
		psc_fatal("ioctl SIOCGIFNAME");
	close(s);
	strlcpy(ifn, ifr.ifr_name, IFNAMSIZ);
}

__static int
pflnet_rtexists_rtnetlink(const struct sockaddr *sa)
{
	struct {
		struct nlmsghdr	nmh;
		struct rtmsg	rtm;
#define RT_SPACE 8192
		unsigned char	buf[RT_SPACE];
		struct rtattr	rta;
		struct nlmsgerr	nlerr;
	} rq;
	struct sockaddr_in *sin = (void *)sa;
	in_addr_t cmpaddr, zero = 0;
	struct nlmsghdr *nmh;
	struct rtattr *rta;
	struct rtmsg *rtm;
	ssize_t rc, rca;
	int rv = 0, s;
	size_t nb;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s == -1)
		psc_fatal("socket");

	memset(&rq, 0, sizeof(rq));
	rq.nmh.nlmsg_len = NLMSG_SPACE(sizeof(rq.rtm));
	rq.nmh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	rq.nmh.nlmsg_type = RTM_GETROUTE;

	rq.rtm.rtm_family = sa->sa_family;
	rq.rtm.rtm_protocol = RTPROT_UNSPEC;
	rq.rtm.rtm_table = RT_TABLE_MAIN;
	rq.rtm.rtm_scope = RT_SCOPE_LINK;

	rta = (void *)((char *)&rq.rta + NLMSG_SPACE(sizeof(rq.rtm)));
	rta->rta_type = RTA_DST;
	rta->rta_len = RTA_LENGTH(sizeof(sin->sin_addr));
	memcpy(RTA_DATA(rta), &sin->sin_addr,
	    sizeof(sin->sin_addr));

	errno = 0;
	rc = write(s, &rq, rq.nmh.nlmsg_len);
	if (rc != (ssize_t)rq.nmh.nlmsg_len)
		psc_fatal("routing socket length mismatch");

	for (;;) {
		rc = read(s, &rq, sizeof(rq));
		if (rc == -1)
			psc_fatal("routing socket read");

		switch (rq.nmh.nlmsg_type) {
		case NLMSG_ERROR: {
			struct nlmsgerr *nlerr;

			nlerr = NLMSG_DATA(&rq.nlerr);
			psc_fatalx("netlink: %s", strerror(nlerr->error));
		    }
		case NLMSG_DONE:
			goto out;
		}

		nmh = &rq.nmh;
		nb = rc;
		for (; NLMSG_OK(nmh, nb); nmh = NLMSG_NEXT(nmh, nb)) {
			rtm = NLMSG_DATA(nmh);

			if (rtm->rtm_table != RT_TABLE_MAIN)
				continue;

			rta = RTM_RTA(rtm);
			rca = RTM_PAYLOAD(nmh);

			for (; RTA_OK(rta, rca);
			    rta = RTA_NEXT(rta, rca)) {
				switch (rta->rta_type) {
				case RTA_GATEWAY:
					cmpaddr = sin->sin_addr.s_addr;
					if (zero == cmpaddr) {
						rv = 1;
						goto out;
					}
					break;
				case RTA_DST:
					cmpaddr = sin->sin_addr.s_addr;

					pfl_bitstr_copy(&cmpaddr,
					    rtm->rtm_dst_len, &zero, 0,
					    sizeof(zero) * NBBY -
					    rtm->rtm_dst_len);

					if (cmpaddr == *(in_addr_t *)
					    RTA_DATA(rta)) {
						rv = 1;
						goto out;
					}
					break;
				}
			}
		}
	}
 out:
	close(s);
	return (rv);
}

__static int 
pflnet_getifnfordst_rtnetlink(const struct sockaddr *sa,
    char ifn[IFNAMSIZ])
{
	struct {
		struct nlmsghdr	nmh;
		struct rtmsg	rtm;
#define RT_SPACE 8192
		unsigned char	buf[RT_SPACE];
	} rq;
	const struct sockaddr_in *sin;
	struct nlmsghdr *nmh;
	struct rtattr *rta;
	struct rtmsg *rtm;
	ssize_t rc, rca;
	int s, ifidx;
	size_t nb;

	sin = (void *)sa;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s == -1)
		psc_fatal("socket");

	memset(&rq, 0, sizeof(rq));
	rq.nmh.nlmsg_len = NLMSG_SPACE(sizeof(rq.rtm)) +
	    RTA_LENGTH(sizeof(sin->sin_addr));
	rq.nmh.nlmsg_flags = NLM_F_REQUEST;
	rq.nmh.nlmsg_type = RTM_GETROUTE;

	rq.rtm.rtm_family = sa->sa_family;
	rq.rtm.rtm_protocol = RTPROT_UNSPEC;
	rq.rtm.rtm_table = RT_TABLE_MAIN;
	/* # bits filled in target addr */
	rq.rtm.rtm_dst_len = sizeof(sin->sin_addr) * NBBY;
	rq.rtm.rtm_scope = RT_SCOPE_LINK;

	rta = (void *)((char *)&rq + NLMSG_SPACE(sizeof(rq.rtm)));
	rta->rta_type = RTA_DST;
	rta->rta_len = RTA_LENGTH(sizeof(sin->sin_addr));
	memcpy(RTA_DATA(rta), &sin->sin_addr,
	    sizeof(sin->sin_addr));

	errno = 0;
	rc = write(s, &rq, rq.nmh.nlmsg_len);
	if (rc != (ssize_t)rq.nmh.nlmsg_len)
		psc_fatal("routing socket length mismatch");

	rc = read(s, &rq, sizeof(rq));
	if (rc == -1)
		psc_fatal("routing socket read");
	close(s);

	switch (rq.nmh.nlmsg_type) {
	case NLMSG_ERROR: {
		struct nlmsgerr *nlerr;

		nlerr = NLMSG_DATA(&rq.nmh);
		psclog_warnx("netlink: %s", strerror(nlerr->error));
		break;
	    }
	case NLMSG_DONE:
		psclog_warnx("netlink: unexpected EOF");
	}

	nmh = &rq.nmh;
	nb = rc;
	for (; NLMSG_OK(nmh, nb); nmh = NLMSG_NEXT(nmh, nb)) {
		rtm = NLMSG_DATA(nmh);

		if (rtm->rtm_table != RT_TABLE_MAIN)
			continue;

		rta = RTM_RTA(rtm);
		rca = RTM_PAYLOAD(nmh);

		for (; RTA_OK(rta, rca); rta = RTA_NEXT(rta, rca)) {
			switch (rta->rta_type) {
			case RTA_OIF:
				memcpy(&ifidx, RTA_DATA(rta),
				    sizeof(ifidx));
				pflnet_getifname(ifidx, ifn);
				return (0);
			}
		}
	}
	psclog_warnx("no route for addr %s", inet_ntoa(sin->sin_addr));
	return (1);
}
#endif

#ifdef HAVE_RT_SYSCTL
__static int
pflnet_rtexists_sysctl(const struct sockaddr *sa)
{
	union {
		struct rt_msghdr *rtm;
		char *ch;
		void *p;
	} u;
	union pfl_sockaddr_ptr s, os;
	int rc = 0, mib[6];
	char *buf = NULL;
	size_t len;

	os.cp = sa;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = AF_INET;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, nitems(mib), NULL, &len, NULL, 0) == -1)
		psc_fatal("route-sysctl-estimate");
	if (len) {
		buf = PSCALLOC(len);
		if (sysctl(mib, nitems(mib), buf, &len, NULL, 0) == -1)
			psc_fatal("actual retrieval of routing table");
	}

	for (u.p = buf; u.ch && u.ch < buf + len;
	    u.ch += u.rtm->rtm_msglen) {
		if (u.rtm->rtm_version != RTM_VERSION)
			continue;
		s.p = u.rtm + 1;

		if (s.s->sin.sin_addr.s_addr ==
		    os.s->sin.sin_addr.s_addr) {
			rc = 1;
			break;
		}
	}

	PSCFREE(buf);

	return (rc);
}
#endif

#ifdef RTM_GET
__static int
pflnet_getifnfordst_rtsock(const struct sockaddr *sa, char ifn[IFNAMSIZ])
{
	struct {
		struct rt_msghdr	rtm;
#define RT_SPACE 512
		unsigned char		buf[RT_SPACE];
	} m;
	struct rt_msghdr *rtm = &m.rtm;
	union pfl_sockaddr psa, *psap;
	unsigned char *p = m.buf;
	ssize_t len, rc;
	pid_t pid;
	int j, s;

	memset(&m, 0, sizeof(m));

#ifdef HAVE_SA_LEN
#define ADDSOCKADDR(p, sa)						\
	do {								\
		memcpy((p), (sa), (sa)->sa_len);			\
		(p) += SOCKADDR_GETLEN(sa);					\
	} while (0)
#else
#define ADDSOCKADDR(p, sa)						\
	do {								\
		memcpy((p), (sa), sizeof(*(sa)));			\
		(p) += SOCKADDR_GETLEN(sa);					\
	} while (0)
#endif

	ADDSOCKADDR(p, sa);

	memset(&psa, 0, sizeof(psa));
	psa.sdl.sdl_family = AF_LINK;
#ifdef HAVE_SA_LEN
	psa.sdl.sdl_len = sizeof(psa.sdl);
#endif
	ADDSOCKADDR(p, &psa.sa);

	rtm->rtm_type = RTM_GET;
	rtm->rtm_flags = RTF_STATIC | RTF_UP | RTF_HOST | RTF_GATEWAY;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_addrs = RTA_DST | RTA_IFP;
#ifdef HAVE_RTM_HDRLEN
	rtm->rtm_hdrlen = sizeof(m.rtm);
#endif
	rtm->rtm_msglen = len = p - (unsigned char *)&m;

	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s == -1)
		psc_fatal("socket");

	rc = write(s, &m, len);
	if (rc == -1)
		psc_fatal("writing to routing socket");
	if (rc != len)
		psc_fatalx("writing to routing socket: short write");

	pid = getpid();
	do {
		rc = read(s, &m, sizeof(m));
	} while (rc > 0 && (rtm->rtm_version != RTM_VERSION ||
	    rtm->rtm_seq || rtm->rtm_pid != pid));

	if (rc == -1)
		psc_fatal("read from routing socket");

	if (rtm->rtm_version != RTM_VERSION)
		psc_fatalx("routing message version mismatch; has=%d got=%d",
		    RTM_VERSION, rtm->rtm_version);
	if (rtm->rtm_errno)
		psc_fatalx("routing error: %s", strerror(rtm->rtm_errno));
	if (rtm->rtm_msglen > rc)
		psc_fatalx("routing message too large");

	close(s);

	p = m.buf;
	for (; rtm->rtm_addrs; rtm->rtm_addrs &= ~(1 << j),
	    p += SOCKADDR_GETLEN(&psap->sa)) {
		j = ffs(rtm->rtm_addrs) - 1;
		psap = (void *)p;
		switch (1 << j) {
		case RTA_IFP:
			if (psap->sdl.sdl_family == AF_LINK &&
			    psap->sdl.sdl_nlen) {
				strncpy(ifn, psap->sdl.sdl_data, IFNAMSIZ - 1);
				ifn[IFNAMSIZ - 1] = '\0';
				return (0);
			}
			break;
		}
	}

	psclog_warnx("interface message not received");
	return (1);
}
#endif

int
pflnet_rtexists(const struct sockaddr *sa)
{
	int rc;

#ifdef HAVE_RTNETLINK
	rc = pflnet_rtexists_rtnetlink(sa);
#elif HAVE_RT_SYSCTL
	rc = pflnet_rtexists_sysctl(sa);
#else
	(void)sa;
	errno = ENOTSUP;
	psc_fatal("rtexists");
#endif
	return (rc);
}

/**
 * pflnet_getifnfordst - Obtain an interface name (e.g. eth0) for the
 *	given destination address.
 * @ifa0: base of ifaddrs list to directly compare for localhost.
 * @sa: destination address.
 * @ifn: value-result interface name to fill in.
 */
int
pflnet_getifnfordst(const struct ifaddrs *ifa0,
    const struct sockaddr *sa, char ifn[IFNAMSIZ])
{
	int rc;
	const struct sockaddr_in *sin;
	const struct ifaddrs *ifa;

	psc_assert(sa->sa_family == AF_INET);
	sin = (void *)sa;

	/*
	 * Scan interfaces for addr since netlink
	 * will always give us the lo interface.
	 */
	for (ifa = ifa0; ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == sa->sa_family &&
		    memcmp(&sin->sin_addr,
		    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(sin->sin_addr)) == 0) {
			strlcpy(ifn, ifa->ifa_name, IFNAMSIZ);
			return (0);
		}

#ifdef HAVE_RTNETLINK
	rc = pflnet_getifnfordst_rtnetlink(sa, ifn);
#elif defined(RTM_GET)
	rc = pflnet_getifnfordst_rtsock(sa, ifn);
#else
	rc = ENOTSUP;
	psc_warnx("getifnfordst not supported");
#endif
	return (rc);
}

int
pflnet_getifaddr(const struct ifaddrs *ifa0, const char *ifname,
    union pfl_sockaddr *sa)
{
	const struct ifaddrs *ifa;
	struct ifreq ifr;
	int rc, s;

	if (ifa0) {
		for (ifa = ifa0; ifa; ifa = ifa->ifa_next)
			if (strcmp(ifa->ifa_name, ifname) == 0 &&
			    ifa->ifa_addr->sa_family == AF_INET) {
				memcpy(&sa->sa, ifa->ifa_addr,
				    sizeof(sa->sin));
				return (1);
			}
	} else {
psc_fatalx("broke");
		strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s == -1)
			psc_fatal("socket");

		rc = ioctl(s, SIOCGIFADDR, &ifr);
		if (rc == -1)
			psc_fatal("ioctl SIOCGIFNAME");
		close(s);

//		memcpy(sap, ifr.ifr_addr, );
	}
	return (0);
}
