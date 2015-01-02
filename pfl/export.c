/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#define PSC_SUBSYS PSS_RPC

#include "pfl/export.h"
#include "pfl/log.h"
#include "pfl/rpc.h"

void
_pscrpc_export_put(struct pscrpc_export *exp)
{
	if (atomic_dec_and_test(&exp->exp_refcount)) {
		psclog_debug("destroying export %p/%s",
		    exp, exp->exp_connection ?
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
	}
}
