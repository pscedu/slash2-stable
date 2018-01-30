/* $Id$ */

/*
 *  Copyright (c) 2002, 2003 Cluster File Systems, Inc.
 *
 *   This file is part of the Lustre file system, http://www.lustre.org
 *   Lustre is a trademark of Cluster File Systems, Inc.
 *
 *   You may have signed or agreed to another license before downloading
 *   this software.  If so, you are bound by the terms and conditions
 *   of that agreement, and the following does not apply to you.  See the
 *   LICENSE file included with this distribution for more information.
 *
 *   If you did not agree to a different license, then this copy of Lustre
 *   is open source software; you can redistribute it and/or modify it
 *   under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   In either case, Lustre is distributed in the hope that it will be
 *   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   license text for more details.
 *
 */

#define PSC_SUBSYS PSS_RPC

#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/export.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/rpc.h"

struct psc_hashtbl pscrpc_conn_hashtbl;

struct pscrpc_connection *
pscrpc_connection_addref(struct pscrpc_connection *c)
{
	atomic_inc(&c->c_refcount);
	CDEBUG(D_OTHER, "connection=%p refcount %d to %s self=%s",
	    c, atomic_read(&c->c_refcount),
	    libcfs_nid2str(c->c_peer.nid), libcfs_nid2str(c->c_self));
	return (c);
}

int
pscrpc_conn_cmp(const void *a, const void *b)
{
	const struct pscrpc_connection *ca = a, *cb = b;

	return (ca->c_peer.nid == cb->c_peer.nid &&
	    ca->c_peer.pid == cb->c_peer.pid &&
	    ca->c_self == cb->c_self);
}

struct pscrpc_connection *
pscrpc_lookup_conn_locked(struct psc_hashbkt *b, lnet_process_id_t peer,
    lnet_nid_t self)
{
	struct pscrpc_connection *c, q;

	q.c_peer.nid = peer.nid;
	q.c_peer.pid = peer.pid;
	q.c_self = self;
	c = psc_hashbkt_search_cmp(&pscrpc_conn_hashtbl, b, &q, &peer.nid);
	if (c)
		return (pscrpc_connection_addref(c));
	return (NULL);
}

struct pscrpc_connection *
pscrpc_get_connection(lnet_process_id_t peer, lnet_nid_t self,
    struct pscrpc_uuid *uuid)
{
	struct pscrpc_connection *c, *c2;
	struct psc_hashbkt *b;

//	psc_assert(uuid);

	psclog_debug("self %s peer %s",
	    libcfs_nid2str(self), libcfs_id2str(peer));

	/* 04/21/2016: segfault on value b */
	b = psc_hashbkt_get(&pscrpc_conn_hashtbl, &peer.nid);
	c = pscrpc_lookup_conn_locked(b, peer, self);

	if (c) {
		psc_hashbkt_put(&pscrpc_conn_hashtbl, b);

		psclog_debug("got self %s peer %s",
		    libcfs_nid2str(c->c_self),
		    libcfs_nid2str(c->c_peer.nid));
		return (c);
	}
	psc_hashbkt_unlock(b);

	psclog_debug("malloc'ing a new rpc_conn for %s",
	    libcfs_id2str(peer));

	c = psc_pool_get(pscrpc_conn_pool);
	memset(c, 0, sizeof(*c));
	INIT_SPINLOCK(&c->c_lock);
	psc_hashent_init(&pscrpc_conn_hashtbl, c);
	atomic_set(&c->c_refcount, 1);
	c->c_peer = peer;
	c->c_self = self;
	if (uuid)
		pscrpc_str2uuid(&c->c_remote_uuid, (char *)uuid->uuid);

	c2 = pscrpc_lookup_conn_locked(b, peer, self);
	if (c2 == NULL) {
		psclog_info("adding connection %p for %s",
		    c, libcfs_id2str(peer));
		psc_hashbkt_add_item(&pscrpc_conn_hashtbl, b, c);
	}
	psc_hashbkt_put(&pscrpc_conn_hashtbl, b);

	if (c2 == NULL)
		return (c);

	psc_pool_return(pscrpc_conn_pool, c);
	return (c2);
}

int
pscrpc_put_connection(struct pscrpc_connection *c)
{
	int n, rc = 0;

	n = psc_atomic32_dec_getnew(&c->c_refcount);

	psc_assert(n >= 0);

	psclog_debug("connection=%p refcount %d to %s",
	    c, n, libcfs_nid2str(c->c_peer.nid));

	if (n == 0) {
//		psc_pool_return(pscrpc_conn_pool, c);
		rc = 1;
	}

	return (rc);
}

void
pscrpc_drop_conns(lnet_process_id_t *peer)
{
	struct pscrpc_connection *c;
	struct psc_hashbkt *b;

	PSC_HASHTBL_FOREACH_BUCKET(b, &pscrpc_conn_hashtbl) {
		/*
 		 * 02/07/2017: Hit b = NULL crash. usocklnd_poll_thread() -->
 		 * usocklnd_process_stale_list() --> usocklnd_tear_peer_conn -->
 		 * usocklnd_check_peer_stale() --> usocklnd_destroy_peer() -->
 		 * lnet_enq_event_locked() --> pscrpc_master_callback() -->
 		 * pscrpc_drop_callback().
 		 */
		psc_hashbkt_lock(b);
		PSC_HASHBKT_FOREACH_ENTRY(&pscrpc_conn_hashtbl, c, b)
			if ((c->c_peer.nid == peer->nid &&
			     c->c_peer.pid == peer->pid) ||
			    peer->nid == LNET_NID_ANY) {
				if (c->c_exp) {
					/*
					 * 01/30/2018: c->c_exp->exp_hldropf == NULL
					 * c->c_exp->exp_private = (void *) 0x2b8b14028900
					 */
					pscrpc_export_hldrop(c->c_exp);
					c->c_exp = NULL;
				}
				if (c->c_imp) {
					pscrpc_fail_import(c->c_imp, 0);
					c->c_imp = NULL;
				}
			}
		psc_hashbkt_unlock(b);
	}
}

void
pscrpc_conns_init(void)
{
	psc_hashtbl_init(&pscrpc_conn_hashtbl, 0,
	    struct pscrpc_connection, c_peer.nid, c_hentry, 3067,
	    pscrpc_conn_cmp, "rpcconn");
}

void
pscrpc_conns_destroy(void)
{
	struct pscrpc_connection *c, *c_next;
	struct psc_hashbkt *b;

	PSC_HASHTBL_FOREACH_BUCKET(b, &pscrpc_conn_hashtbl) {
		psc_hashbkt_lock(b);
		PSC_HASHBKT_FOREACH_ENTRY_SAFE(&pscrpc_conn_hashtbl, c,
		    c_next, b) {
			psc_hashbkt_del_item(&pscrpc_conn_hashtbl, b,
			    c);
			psc_pool_return(pscrpc_conn_pool, c);
		}
		psc_hashbkt_unlock(b);
	}

	psc_hashtbl_destroy(&pscrpc_conn_hashtbl);
}
