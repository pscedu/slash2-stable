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

#define DEBUG_SUBSYSTEM S_LNET
#include <lnet/lib-lnet.h>

#ifdef __KERNEL__
#else /* __KERNEL__ */
#ifdef HAVE_LIBPTHREAD

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static char *accept_type;
static int accept_port = 988;
static int accept_backlog;
static int accept_timeout;

static int la_init_ok;	/* bumped for each successful thread */
static int la_shutdown;
static struct cfs_completion la_compl;
static struct list_head laps = CFS_LIST_HEAD_INIT(laps);

int
lnet_acceptor_port(void)
{
	return accept_port;
}

int
lnet_parse_int_tunable(int *value, char *name, int dflt)
{
	char    *env = getenv(name);
	char    *end;

	if (env == NULL) {
		*value = dflt;
		return 0;
	}

	*value = strtoull(env, &end, 0);
	if (*end == 0)
		return 0;

	CERROR("Can't parse tunable %s=%s\n", name, env);
	return -EINVAL;
}

int
lnet_parse_string_tunable(char **value, char *name, char *dflt)
{
	char    *env = getenv(name);

	if (env == NULL)
		*value = dflt;
	else
		*value = env;

	return 0;
}

int
lnet_acceptor_get_tunables()
{
	int rc;
	rc = lnet_parse_string_tunable(&accept_type, "LNET_ACCEPT", "all");

	if (rc != 0)
		return rc;

	rc = lnet_parse_int_tunable(&accept_port, "LNET_ACCEPT_PORT", 988);

	if (rc != 0)
		return rc;

	rc = lnet_parse_int_tunable(&accept_backlog, "LNET_ACCEPT_BACKLOG", 127);

	if (rc != 0)
		return rc;

	rc = lnet_parse_int_tunable(&accept_timeout, "LNET_ACCEPT_TIMEOUT", 5);

	if (rc != 0)
		return rc;

	CDEBUG(D_NET, "accept_type     = %s\n", accept_type);
	CDEBUG(D_NET, "accept_port     = %d\n", accept_port);
	CDEBUG(D_NET, "accept_backlog  = %d\n", accept_backlog);
	CDEBUG(D_NET, "accept_timeout  = %d\n", accept_timeout);
	return 0;
}

static inline int
lnet_accept_magic(__u32 magic, __u32 constant)
{
	return (magic == constant ||
		magic == __swab32(constant));
}

/* user-land lnet_accept() isn't used by any LND's directly. So, we don't
 * do it visible outside acceptor.c and we can change its prototype
 * freely */
static int
lnet_accept(struct lnet_xport *lx, __u32 magic, __u32 peer_ip,
    __unusedx int peer_port)
{
	int rc, flip;
	lnet_acceptor_connreq_t cr;
	lnet_ni_t *ni;

	if (!lnet_accept_magic(magic, LNET_PROTO_ACCEPTOR_MAGIC)) {
		LCONSOLE_ERROR("Refusing connection from %u.%u.%u.%u magic %08x: "
			       "unsupported acceptor protocol\n",
			       HIPQUAD(peer_ip), magic);
		return -EPROTO;
	}

	flip = (magic != LNET_PROTO_ACCEPTOR_MAGIC);

	rc = lx_read(lx, &cr.acr_version, sizeof(cr.acr_version),
	    accept_timeout);
	if (rc != 0) {
		CERROR("Error %d reading connection request version from "
		       "%u.%u.%u.%u\n", rc, HIPQUAD(peer_ip));
		return -EIO;
	}

	if (flip)
		__swab32s(&cr.acr_version);

	if (cr.acr_version != LNET_PROTO_ACCEPTOR_VERSION)
		return -EPROTO;

	rc = lx_read(lx, &cr.acr_nid, sizeof(cr) -
	    offsetof(lnet_acceptor_connreq_t, acr_nid), accept_timeout);
	if (rc != 0) {
		CERROR("Error %d reading connection request from "
		       "%u.%u.%u.%u\n", rc, HIPQUAD(peer_ip));
		return -EIO;
	}

	if (flip)
		__swab64s(&cr.acr_nid);

	ni = lnet_net2ni(LNET_NIDNET(cr.acr_nid));

	if (ni == NULL ||                    /* no matching net */
	     ni->ni_nid != cr.acr_nid) {     /* right NET, wrong NID! */
		if (ni != NULL)
			lnet_ni_decref(ni);
		LCONSOLE_ERROR("Refusing connection from %u.%u.%u.%u for %s: "
			       " No matching NI\n",
			       HIPQUAD(peer_ip), libcfs_nid2str(cr.acr_nid));
		return -EPERM;
	}

	if (ni->ni_lnd->lnd_accept == NULL) {
		lnet_ni_decref(ni);
		LCONSOLE_ERROR("Refusing connection from %u.%u.%u.%u for %s: "
			       " NI does not accept IP connections\n",
			       HIPQUAD(peer_ip), libcfs_nid2str(cr.acr_nid));
		return -EPERM;
	}

	CDEBUG(D_NET, "Accept %s from %u.%u.%u.%u\n",
	       libcfs_nid2str(cr.acr_nid), HIPQUAD(peer_ip));

	rc = ni->ni_lnd->lnd_accept(ni, lx);

	lnet_ni_decref(ni);
	return rc;
}

struct lnet_acceptor_param {
	lnet_nid_t		lap_nid;
	int			lap_faux_secure;
	in_port_t		lap_port;
	struct in_addr		lap_addr;
	struct cfs_completion	lap_compl;
	struct list_head	lap_lentry;
};

int
lnet_acceptor(void *arg)
{
	struct lnet_acceptor_param *lap = arg;
	char           name[16];
	int            rc;
	int            newsock;
	__u32          peer_ip;
	int            peer_port;
	__u32          magic;
	struct lnet_xport *lx;
	int sock;

	snprintf(name, sizeof(name), "acceptor_%03d", lap->lap_port);
	cfs_daemonize(name);
	cfs_block_allsigs();

	rc = libcfs_sock_listen(&sock, lap->lap_nid, lap->lap_addr.s_addr,
	    lap->lap_port, accept_backlog);
	if (rc != 0) {
		if (rc == -EADDRINUSE)
			LCONSOLE_ERROR("Can't start acceptor on port %d: "
				       "port already in use\n",
				       lap->lap_port);
		else
			LCONSOLE_ERROR("Can't start acceptor on port %d: "
				       "unexpected error %d\n",
				       lap->lap_port, rc);

	} else {
		CDEBUG(D_NET, "Accept %s, port %d\n", accept_type, lap->lap_port);
	}

	/* set init status and unblock parent */
	if (rc == 0)
		la_init_ok++;
	cfs_complete(&lap->lap_compl);

	if (rc != 0)
		return rc;

	list_add(&lap->lap_lentry, &laps);

	while (!la_shutdown) {

		rc = libcfs_sock_accept(&newsock, sock,
					&peer_ip, &peer_port);
		if (rc != 0)
			continue;

		/* maybe we're waken up with libcfs_sock_abort_accept() */
		if (la_shutdown) {
			close(newsock);
			break;
		}

		lx = lx_new(LNET_NETTYP(lap->lap_nid) == SSLLND ?
		    &libcfs_ssl_lxi : &libcfs_sock_lxi);
		lx_accept(lx, newsock);

		if (lap->lap_faux_secure &&
		    peer_port > LNET_ACCEPTOR_MAX_RESERVED_PORT) {
			CERROR("Refusing connection from %u.%u.%u.%u: "
			       "insecure port %d\n",
			       HIPQUAD(peer_ip), peer_port);
			goto failed;
		}

		rc = lx_read(lx, &magic, sizeof(magic), accept_timeout);
		if (rc != 0) {
			CERROR("Error %d reading connection request from "
			       "%u.%u.%u.%u\n", rc, HIPQUAD(peer_ip));
			goto failed;
		}

		rc = lnet_accept(lx, magic, peer_ip, peer_port);
		if (rc != 0)
			goto failed;

		continue;

	  failed:
	  	lx_close(lx);
		close(lx->lx_fd);
		lx_destroy(lx);
	}

	close(sock);
	LCONSOLE(0,"Acceptor stopping\n");

	list_del_init(&lap->lap_lentry);

	/* unblock lnet_acceptor_stop() */
	cfs_complete(&lap->lap_compl);
	cfs_free(lap);

	return 0;
}

static int skip_waiting_for_completion;

int
lnet_acceptor_start(void)
{
	struct lnet_acceptor_param *lap;
	struct in_addr ina;
	char addrbuf[40];
	long   secure;
	lnet_ni_t *ni;
	int count = 0;
	int rc;

	rc = lnet_acceptor_get_tunables();
	if (rc != 0)
		return rc;

	/* Do nothing if we're liblustre clients */
	if ((the_lnet.ln_pid & LNET_PID_USERFLAG) != 0)
		return 0;

	cfs_init_completion(&la_compl);

	if (!strcmp(accept_type, "secure")) {
		secure = 1;
	} else if (!strcmp(accept_type, "all")) {
		secure = 0;
	} else if (!strcmp(accept_type, "none")) {
		skip_waiting_for_completion = 1;
		return 0;
	} else {
		LCONSOLE_ERROR ("Can't parse 'accept_type=\"%s\"'\n", accept_type);
		cfs_fini_completion(&la_compl);
		return -EINVAL;
	}

	LNET_LOCK();
	list_for_each_entry(ni, &the_lnet.ln_nis, ni_list)
		if (ni->ni_lnd->lnd_accept &&
		    (ni->ni_flags & LNIF_ACCEPTOR)) {
			count++;

			lap = cfs_alloc(sizeof(*lap), 0);
			if (lap == NULL) {
				CERROR("malloc");
				abort();
			}
			lap->lap_faux_secure = secure;
			lap->lap_port = accept_port;
			lap->lap_addr.s_addr = LNET_NIDADDR(ni->ni_nid);
			lap->lap_nid = ni->ni_nid;
			cfs_init_completion(&lap->lap_compl);

			ina.s_addr = htonl(lap->lap_addr.s_addr);

			rc = cfs_create_thread(lnet_acceptor, lap,
			    "lnacthr-%s", inet_ntop(AF_INET, &ina,
			    addrbuf, sizeof(addrbuf)));
			if (rc != 0) {
				CERROR("Can't start acceptor thread: %d\n", rc);
				cfs_fini_completion(&la_compl);
				cfs_free(lap);
				return rc;
			}
			/* wait for acceptor to startup */
			cfs_wait_for_completion(&lap->lap_compl);
		}
	LNET_UNLOCK();

	if (count == 0) {
		skip_waiting_for_completion = 1;
		return 0;
	}

	if (la_init_ok > 0)
		return 0;

	cfs_fini_completion(&la_compl);
	return -ENETDOWN;
}

void
lnet_acceptor_stop(void)
{
	struct lnet_acceptor_param *lap, *lapn;

	/* Do nothing if we're liblustre clients */
	if ((the_lnet.ln_pid & LNET_PID_USERFLAG) != 0)
		return;

	if (!skip_waiting_for_completion) {
		la_shutdown = 1;

		/* block until acceptor signals exit */
		list_for_each_entry_safe(lap, lapn, &laps, lap_lentry) {
			libcfs_sock_abort_accept(lap->lap_nid, accept_port);
			cfs_wait_for_completion(&lap->lap_compl);
		}
	}

	cfs_fini_completion(&la_compl);
}
#else
int
lnet_acceptor_start(void)
{
	return 0;
}

void
lnet_acceptor_stop(void)
{
}
#endif /* !HAVE_LIBPTHREAD */
#endif /* !__KERNEL__ */
