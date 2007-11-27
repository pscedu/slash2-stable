/* $Id: zestConnection.c 2114 2007-11-03 19:39:08Z pauln $ */

#include "subsys.h"
#define ZSUBSYS ZS_RPC

#include "zestAlloc.h"
#include "zestAtomic.h"
#include "zestList.h"
#include "zestLock.h"
#include "zestRpc.h"

static zest_spinlock_t  conn_lock        = LOCK_INITIALIZER;
static struct zlist_head conn_list        = ZLIST_HEAD_INIT(conn_list);
static struct zlist_head conn_unused_list = ZLIST_HEAD_INIT(conn_unused_list);

struct zestrpc_connection *
zestrpc_connection_addref(struct zestrpc_connection *c)
{
        ENTRY;
        atomic_inc(&c->c_refcount);
        CDEBUG (D_INFO, "connection=%p refcount %d to %s\n",
                c, atomic_read(&c->c_refcount),
                libcfs_nid2str(c->c_peer.nid));
        RETURN(c);
}

struct zestrpc_connection *
zestrpc_lookup_conn_locked (lnet_process_id_t peer)
{
        struct zestrpc_connection *c;
        struct zlist_head          *tmp;

        zlist_for_each(tmp, &conn_list) {
                c = zlist_entry(tmp, struct zestrpc_connection, c_link);

                if (peer.nid == c->c_peer.nid &&
                    peer.pid == c->c_peer.pid)
                        return zestrpc_connection_addref(c);
        }

        zlist_for_each(tmp, &conn_unused_list) {
                c = zlist_entry(tmp, struct zestrpc_connection, c_link);
		zdbg("unused conn %p@%s looking for %s",
		     c, libcfs_id2str(c->c_peer), libcfs_id2str(peer));

                if (peer.nid == c->c_peer.nid &&
                    peer.pid == c->c_peer.pid) {
                        zlist_del(&c->c_link);
                        zlist_add(&c->c_link, &conn_list);
                        return zestrpc_connection_addref(c);
                }
        }

        return NULL;
}

struct zestrpc_connection *
zestrpc_get_connection(lnet_process_id_t peer,
		       lnet_nid_t self, struct zest_uuid *uuid)
{
        struct zestrpc_connection *c;
        struct zestrpc_connection *c2;
        ENTRY;
	
	//zest_assert(uuid != NULL);

	zinfo("self %s peer %s",
	      libcfs_nid2str(self), libcfs_id2str(peer));

        spin_lock(&conn_lock);

        c = zestrpc_lookup_conn_locked(peer);

        spin_unlock(&conn_lock);

        if (c != NULL)
                RETURN (c);

	zinfo("Malloc'ing a new rpc_conn for %s",
	      libcfs_id2str(peer));

        c = TRY_ZALLOC(sizeof(*c));
        if (c == NULL)
                RETURN (NULL);

        atomic_set(&c->c_refcount, 1);
        c->c_peer = peer;
        c->c_self = self;
        if (uuid != NULL)
                zest_str2uuid(&c->c_remote_uuid, (char *)uuid->uuid);

        spin_lock(&conn_lock);

        c2 = zestrpc_lookup_conn_locked(peer);
        if (c2 == NULL) { 
		znotify("adding connection %p for %s", 
			c, libcfs_id2str(peer));
                zlist_add(&c->c_link, &conn_list);
	}

        spin_unlock(&conn_lock);

        if (c2 == NULL)
                RETURN (c);

        ZOBD_FREE(c, sizeof(*c));
        RETURN (c2);
}

int zestrpc_put_connection(struct zestrpc_connection *c)
{
        int rc = 0;
        ENTRY;

        if (c == NULL) {
                CERROR("NULL connection\n");
                RETURN(0);
        }

        zinfo("connection=%p refcount %d to %s",
	      c, atomic_read(&c->c_refcount) - 1,
	      libcfs_nid2str(c->c_peer.nid));

        if (atomic_dec_and_test(&c->c_refcount)) {
		zinfo("connection=%p to unused_list", c);

                spinlock(&conn_lock);
                zlist_del(&c->c_link);
                zlist_add(&c->c_link, &conn_unused_list);
                freelock(&conn_lock);
                rc = 1;
        }
        if (atomic_read(&c->c_refcount) < 0)
                zerrorx("connection %p refcount %d!",
			c, atomic_read(&c->c_refcount));

        RETURN(rc);
}
