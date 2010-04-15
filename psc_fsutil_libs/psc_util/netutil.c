/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/route.h>
#include <arpa/inet.h>

#ifdef HAVE_RTNETLINK
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#endif

#ifdef HAVE_GETIFADDRS
# include <net/if.h>

# include <ifaddrs.h>
#endif

#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "psc_util/net.h"

/*
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
		psc_error("setsockopt");
#else
	(void)sock;
#endif
}

/*
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
#else
	struct ucred ucr;
	socklen_t len;

	len = sizeof(ucr);
	if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &ucr, &len))
		return (-errno);
	*uid = ucr.uid;
	*gid = ucr.gid;
#endif
	return (0);
}

/*
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

/*
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

__static void
pflnet_getifnfordst_rtnetlink(const struct sockaddr *sa, char ifn[IFNAMSIZ])
{
	struct {
		struct nlmsghdr	nmh;
		struct rtmsg	rtm;
#define RT_SPACE 8192
		unsigned char	buf[RT_SPACE];
	} rq;
	const struct sockaddr_in *sin;
	struct rtattr *rta;
	int s, ifidx;
	ssize_t rc;

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

	if (rq.nmh.nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *nlerr;

		nlerr = NLMSG_DATA(&rq.nmh);
		psc_fatalx("netlink: %s", strerror(nlerr->error));
	}

	rc -= NLMSG_SPACE(sizeof(rq.rtm));
	while (rc > 0) {
		if (rta->rta_type == RTA_OIF &&
		    RTA_PAYLOAD(rta) == sizeof(ifidx)) {
			memcpy(&ifidx, RTA_DATA(rta),
			    sizeof(ifidx));
			pflnet_getifname(ifidx, ifn);
			return;
		}
		rta = RTA_NEXT(rta, rc);
	}
	psc_fatalx("no route for addr");
}
#else
__static void
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

#define SOCKADDRLEN(sa)							\
	((sa)->sa_len ? PSC_ALIGN((sa)->sa_len, sizeof(int32_t)) : sizeof(int32_t))
#define ADDSOCKADDR(p, sa)						\
	do {								\
		memcpy((p), (sa), (sa)->sa_len);			\
		(p) += SOCKADDRLEN(sa);					\
	} while (0)
	ADDSOCKADDR(p, sa);

	memset(&psa, 0, sizeof(psa));
	psa.sdl.sdl_family = AF_LINK;
	psa.sdl.sdl_len = sizeof(psa.sdl);
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
	    p += SOCKADDRLEN(&psap->sa)) {
		j = ffs(rtm->rtm_addrs) - 1;
		psap = (void *)p;
		switch (1 << j) {
		case RTA_IFP:
			if (psap->sdl.sdl_family == AF_LINK &&
			    psap->sdl.sdl_nlen) {
				strncpy(ifn, psap->sdl.sdl_data, IFNAMSIZ - 1);
				ifn[IFNAMSIZ - 1] = '\0';
				return;
			}
			break;
		}
	}

	psc_fatalx("interface message not received");
}
#endif

/*
 * pflnet_getifnfordst - Obtain an interface name (e.g. eth0) for the
 *	given destination address.
 * @ifa0: base of ifaddrs list to directly compare for localhost.
 * @sa: destination address.
 * @ifn: value-result interface name to fill in.
 */
void
pflnet_getifnfordst(const struct ifaddrs *ifa0,
    const struct sockaddr *sa, char ifn[IFNAMSIZ])
{
	const struct sockaddr_in *sin;
	const struct ifaddrs *ifa;

	psc_assert(sa->sa_family == AF_INET);
	sin = (void *)sa;

	/*
	 * Scan interfaces for addr since netlink
	 * will always give us the lo interface.
	 */
	ifa = ifa0;
	for (ifa = ifa0; ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == sa->sa_family &&
		    memcmp(&sin->sin_addr,
		    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(sin->sin_addr)) == 0) {
			strlcpy(ifn, ifa->ifa_name, IFNAMSIZ);
			return;
		}

#ifdef HAVE_RTNETLINK
	pflnet_getifnfordst_rtnetlink(sa, ifn);
#else
	pflnet_getifnfordst_rtsock(sa, ifn);
#endif
}
