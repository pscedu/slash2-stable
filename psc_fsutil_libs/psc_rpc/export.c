/* $Id: pscExport.c 2212 2007-11-19 16:49:37Z pauln $ */

#include "psc_util/subsys.h"
#define SUBSYS ZS_RPC

#include "psc_ds/tree.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/export.h"

void __pscrpc_export_put(struct pscrpc_export *exp)
{
        if (atomic_dec_and_test(&exp->exp_refcount)) {
                psc_info("destroying export %p/%s",
			 exp, (exp->exp_connection) ?
			 libcfs_id2str(exp->exp_connection->c_peer) : "<?>");

                /* "Local" exports (lctl, LOV->{mdc,osc}) have no connection. */
                if (exp->exp_connection)
			//ptlrpc_put_connection_superhack(exp->exp_connection);
                        pscrpc_put_connection(exp->exp_connection);
		// XXX Shield from the client for now,

		exp->exp_destroycb(exp);

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
