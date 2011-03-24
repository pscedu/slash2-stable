/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2011, Pittsburgh Supercomputing Center (PSC).
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

#define PSC_SUBSYS PSS_RPC

#include "psc_ds/tree.h"
#include "psc_rpc/export.h"
#include "psc_rpc/rpc.h"
#include "psc_util/log.h"

void
_pscrpc_export_put(struct pscrpc_export *exp)
{
	if (atomic_dec_and_test(&exp->exp_refcount)) {
		psclog_debug("destroying export %p/%s",
		    exp, (exp->exp_connection) ?
		    libcfs_id2str(exp->exp_connection->c_peer) : "<?>");

		if (exp->exp_connection)
			pscrpc_put_connection(exp->exp_connection);

		pscrpc_export_hldrop(exp);

		/* Outstanding replies refers to 'difficult' replies
		   Not sure what h_link is for - pauln */
		//LASSERT(list_empty(&exp->exp_outstanding_replies));
		//LASSERT(list_empty(&exp->exp_handle.h_link));
#if PAULS_TODO
		obd_destroy_export(exp);
#endif
		PSCRPC_OBD_FREE(exp, sizeof(*exp));
		/*
		 * pscrpc is not using obd's and our exports are attached to
		class_decref(obd);
		*/
	}
}
