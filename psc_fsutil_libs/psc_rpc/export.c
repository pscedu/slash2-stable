/* $Id: pscExport.c 2212 2007-11-19 16:49:37Z pauln $ */

#include "psc_util/subsys.h"
#define SUBSYS ZS_RPC

#include "psc_ds/tree.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/export.h"
//#include "pscInode.h"
//#include "zeil.h"

void __zclass_export_put(struct pscrpc_export *exp)
{
#ifdef ZESTION
	struct zeil *zeil, *next;
#endif
        if (atomic_dec_and_test(&exp->exp_refcount)) {
                psc_info("destroying export %p/%s",
			 exp, (exp->exp_connection) ?
			 libcfs_id2str(exp->exp_connection->c_peer) : "<?>");

                /* "Local" exports (lctl, LOV->{mdc,osc}) have no connection. */
                if (exp->exp_connection)
			//ptlrpc_put_connection_superhack(exp->exp_connection);
                        pscrpc_put_connection(exp->exp_connection);
		// XXX Shield from the client for now,
#ifdef ZESTION
		//printf("cleaning up ZEILS");
		for (zeil = SPLAY_MIN(zeiltree, &exp->exp_zeiltree);
		    zeil; zeil = next) {
			//printf("  removing ino link %p\n", zeil);
			/* Remove from zeil tree. */
			next = SPLAY_NEXT(zeiltree, &exp->exp_zeiltree, zeil);
			SPLAY_REMOVE(zeiltree, &exp->exp_zeiltree, zeil);

			/* Remove from inode list. */
			ZINODE_LOCK(zeil->zeil_ino);
			zlist_del(&zeil->zeil_ino_entry);
			ZINODE_ULOCK(zeil->zeil_ino);

			free(zeil);
		}
#endif
		/* Outstanding replies refers to 'difficult' replies
		   Not sure what h_link is for - pauln */
                //LASSERT(list_empty(&exp->exp_outstanding_replies));
                //LASSERT(list_empty(&exp->exp_handle.h_link));
#if PAULS_TODO
                obd_destroy_export(exp);
#endif
                ZOBD_FREE(exp, sizeof(*exp));
		/**
		 * Psc is not using obd's and our exports are attached to
                class_decref(obd);
		*/
        }
}
