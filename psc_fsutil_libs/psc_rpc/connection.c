/* $Id$ */

#define PSC_SUBSYS PSS_RPC

#include "psc_ds/list.h"
#include "psc_rpc/export.h"
#include "psc_rpc/rpc.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

static psc_spinlock_t	   conn_lock        = LOCK_INITIALIZER;
static struct psclist_head conn_list        = PSCLIST_HEAD_INIT(conn_list);
static struct psclist_head conn_unused_list = PSCLIST_HEAD_INIT(conn_unused_list);

struct pscrpc_connection *
pscrpc_connection_addref(struct pscrpc_connection *c)
{
	atomic_inc(&c->c_refcount);
	CDEBUG (D_INFO, "connection=%p refcount %d to %s self=%s\n",
		c, atomic_read(&c->c_refcount),
		libcfs_nid2str(c->c_peer.nid), libcfs_nid2str(c->c_self));
	return (c);
}

struct pscrpc_connection *
pscrpc_lookup_conn_locked(lnet_process_id_t peer, lnet_nid_t self)
{
	struct pscrpc_connection *c;
	struct psclist_head          *tmp;

	psclist_for_each(tmp, &conn_list) {
		c = psclist_entry(tmp, struct pscrpc_connection, c_link);

		if (peer.nid == c->c_peer.nid &&
		    peer.pid == c->c_peer.pid &&
		    self == c->c_self)
			return pscrpc_connection_addref(c);
	}

	psclist_for_each(tmp, &conn_unused_list) {
		c = psclist_entry(tmp, struct pscrpc_connection, c_link);
		psc_dbg("unused conn %p@%s looking for %s",
			c, libcfs_id2str(c->c_peer), libcfs_id2str(peer));

		if (peer.nid == c->c_peer.nid &&
		    peer.pid == c->c_peer.pid &&
		    self     == c->c_self) {
			psclist_del(&c->c_link);
			psclist_xadd(&c->c_link, &conn_list);
			return pscrpc_connection_addref(c);
		}
	}

	return NULL;
}

struct pscrpc_connection *
pscrpc_get_connection(lnet_process_id_t peer,
    lnet_nid_t self, struct pscrpc_uuid *uuid)
{
	struct pscrpc_connection *c;
	struct pscrpc_connection *c2;

	//psc_assert(uuid != NULL);

	psc_info("self %s peer %s",
		 libcfs_nid2str(self), libcfs_id2str(peer));

	spinlock(&conn_lock);

	c = pscrpc_lookup_conn_locked(peer, self);

	freelock(&conn_lock);

	if (c != NULL) {
		psc_info("got self %s peer %s",
			 libcfs_nid2str(c->c_self), libcfs_nid2str(c->c_peer.nid));
		return (c);
	}

	psc_info("Malloc'ing a new rpc_conn for %s",
	      libcfs_id2str(peer));

	c = TRY_PSCALLOC(sizeof(*c));
	if (c == NULL)
		return (NULL);

	atomic_set(&c->c_refcount, 1);
	c->c_peer = peer;
	c->c_self = self;
	if (uuid != NULL)
		pscrpc_str2uuid(&c->c_remote_uuid, (char *)uuid->uuid);

	spinlock(&conn_lock);

	c2 = pscrpc_lookup_conn_locked(peer, self);
	if (c2 == NULL) {
		psc_notify("adding connection %p for %s",
			   c, libcfs_id2str(peer));
		psclist_xadd(&c->c_link, &conn_list);
	}

	freelock(&conn_lock);

	if (c2 == NULL)
		return (c);

	PSCRPC_OBD_FREE(c, sizeof(*c));
	return (c2);
}

int
pscrpc_put_connection(struct pscrpc_connection *c)
{
	int rc = 0;

	if (c == NULL) {
		CERROR("NULL connection\n");
		return (0);
	}

	psc_info("connection=%p refcount %d to %s",
		 c, atomic_read(&c->c_refcount) - 1,
		 libcfs_nid2str(c->c_peer.nid));

	if (atomic_dec_and_test(&c->c_refcount)) {
		int locked;
		psc_info("connection=%p to unused_list", c);

		locked = reqlock(&conn_lock);
		psclist_del(&c->c_link);
		psclist_xadd(&c->c_link, &conn_unused_list);
		ureqlock(&conn_lock, locked);
		rc = 1;
	}
	if (atomic_read(&c->c_refcount) < 0)
		psc_fatalx("connection %p refcount %d!",
			c, atomic_read(&c->c_refcount));

	return (rc);
}

void
pscrpc_drop_conns(lnet_process_id_t *peer)
{
	struct pscrpc_connection *c;

	spinlock(&conn_lock);
	psclist_for_each_entry(c, &conn_list, c_link)
		if (c->c_peer.nid == peer->nid &&
		    c->c_peer.pid == peer->pid) {
			if (c->c_exp)
				pscrpc_export_hldrop(c->c_exp);
			if (c->c_imp)
				pscrpc_fail_import(c->c_imp, 0);
		}
	freelock(&conn_lock);
}
