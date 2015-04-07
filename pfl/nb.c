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

#include <string.h>

#include "pfl/log.h"
#include "pfl/rpc.h"
#include "pfl/rpclog.h"
#include "pfl/thread.h"
#include "pfl/waitq.h"

struct pscrpc_nbreapthr {
	struct pscrpc_request_set	*pnbt_set;
};

/*
 * Add a new non-blocking request to a non-blocking set.
 */
int
pscrpc_nbreqset_add(struct pscrpc_request_set *nbs,
    struct pscrpc_request *rq)
{
	int rc;

	rc = pscrpc_push_req(rq);
	if (rc) {
		DEBUG_REQ(PLL_ERROR, rq, "send failure: %s",
		    strerror(rc));
	} else {
		/*
		 * Adding after is OK because it will sit on the list
		 * marked COMPLETED if it finishes before this add
		 * occurs.
		 */
		pscrpc_set_add_new_req(nbs->nb_reqset, rq);
	}
	return (rc);
}

void
pscrpc_nbreapthr_main(struct psc_thread *thr)
{
	struct pscrpc_request_set *set;
	struct pscrpc_nbreapthr *pnbt;

	pnbt = thr->pscthr_private;
	set = pnbt->pnbt_set;
	while (pscthr_run(thr)) {
		spinlock(&set->set_lock);
		if (pscrpc_set_checkone(set))
			freelock(&set->set_lock);
		else
			psc_waitq_wait(&set->set_waitq, &set->set_lock);
	}
}

void
pscrpc_nbreapthr_spawn(struct pscrpc_request_set *set, int thrtype,
    int nthr, const char *thrname)
{
	struct pscrpc_nbreapthr *pnbt;
	struct psc_thread *thr;
	int i;

	for (i = 0; i < nthr; i++) {
		thr = pscthr_init(thrtype, pscrpc_nbreapthr_main, NULL,
		    sizeof(*pnbt), thrname, i);
		pnbt = thr->pscthr_private;
		pnbt->pnbt_set = set;
		pscthr_setready(thr);
	}
}
