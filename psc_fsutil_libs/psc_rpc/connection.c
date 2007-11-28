/* $Id: pscConnection.c 2114 2007-11-03 19:39:08Z pauln $ */

#include "subsys.h"
#define SUBSYS S_RPC

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_rpc/rpc.h"

static psc_spinlock_t  conn_lock        = LOCK_INITIALIZER;
static struct psclist_head conn_list        = PSCLIST_HEAD_INIT(conn_list);
static struct psclist_head conn_unused_list = PSCLIST_HEAD_INIT(conn_unused_list);

struct pscrpc_connection *
pscrpc_connection_addref(struct pscrpc_connection *c)
{
        ENTRY;
        atomic_inc(&c->c_refcount);
        CDEBUG (D_INFO, "connection=%p refcount %d to %s\n",
                c, atomic_read(&c->c_refcount),
                libcfs_nid2str(c->c_peer.nid));
        RETURN(c);
}

struct pscrpc_connection *
pscrpc_lookup_conn_locked (lnet_process_id_t peer)
{
        struct pscrpc_connection *c;
        struct psclist_head          *tmp;

        psclist_for_each(tmp, &conn_list) {
                c = psclist_entry(tmp, struct pscrpc_connection, c_link);

                if (peer.nid == c->c_peer.nid &&
                    peer.pid == c->c_peer.pid)
                        return pscrpc_connection_addref(c);
        }

        psclist_for_each(tmp, &conn_unused_list) {
                c = psclist_entry(tmp, struct pscrpc_connection, c_link);
		zdbg("unused conn %p@%s looking for %s",
		     c, libcfs_id2str(c->c_peer), libcfs_id2str(peer));

                if (peer.nid == c->c_peer.nid &&
                    peer.pid == c->c_peer.pid) {
                        psclist_del(&c->c_link);
                        psclist_add(&c->c_link, &conn_list);
                        return pscrpc_connection_addref(c);
                }
        }

        return NULL;
}

struct pscrpc_connection *
pscrpc_get_connection(lnet_process_id_t peer,
		       lnet_nid_t self, struct psc_uuid *uuid)
{
        struct pscrpc_connection *c;
        struct pscrpc_connection *c2;
        ENTRY;
	
	//psc_assert(uuid != NULL);

	pscinfo("self %s peer %s",
	      libcfs_nid2str(self), libcfs_id2str(peer));

        spin_lock(&conn_lock);

        c = pscrpc_lookup_conn_locked(peer);

        spin_unlock(&conn_lock);

        if (c != NULL)
                RETURN (c);

	pscinfo("Malloc'ing a new rpc_conn for %s",
	      libcfs_id2str(peer));

        c = TRY_ZALLOC(sizeof(*c));
        if (c == NULL)
                RETURN (NULL);

        atomic_set(&c->c_refcount, 1);
        c->c_peer = peer;
        c->c_self = self;
        if (uuid != NULL)
                psc_str2uuid(&c->c_remote_uuid, (char *)uuid->uuid);

        spin_lock(&conn_lock);

        c2 = pscrpc_lookup_conn_locked(peer);
        if (c2 == NULL) { 
		znotify("adding connection %p for %s", 
			c, libcfs_id2str(peer));
                psclist_add(&c->c_link, &conn_list);
	}

        spin_unlock(&conn_lock);

        if (c2 == NULL)
                RETURN (c);

        ZOBD_FREE(c, sizeof(*c));
        RETURN (c2);
}

int pscrpc_put_connection(struct pscrpc_connection *c)
{
        int rc = 0;
        ENTRY;

        if (c == NULL) {
                CERROR("NULL connection\n");
                RETURN(0);
        }

        pscinfo("connection=%p refcount %d to %s",
	      c, atomic_read(&c->c_refcount) - 1,
	      libcfs_nid2str(c->c_peer.nid));

        if (atomic_dec_and_test(&c->c_refcount)) {
		pscinfo("connection=%p to unused_list", c);

                spinlock(&conn_lock);
                psclist_del(&c->c_link);
                psclist_add(&c->c_link, &conn_unused_list);
                freelock(&conn_lock);
                rc = 1;
        }
        if (atomic_read(&c->c_refcount) < 0)
                psc_errorx("connection %p refcount %d!",
			c, atomic_read(&c->c_refcount));

        RETURN(rc);
}
