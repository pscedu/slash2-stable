/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <errno.h>
#include <string.h>

#include "pfl/dynarray.h"
#include "pfl/list.h"
#include "psc_rpc/rpc.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"
#include "psc_util/net.h"

#include "lnet/lib-types.h"
#include "lnet/lib-lnet.h"

void
pscrpc_getlocalprids(struct psc_dynarray *da)
{
	lnet_process_id_t prid, *pp;
	int rc, n;

	for (n = 0; ; n++) {
		rc = LNetGetId(n, &prid);
		if (rc == -ENOENT)
			break;
		else if (rc)
			psc_fatalx("LNetGetId: %s", strerror(-rc));

		pp = PSCALLOC(sizeof(*pp));
		*pp = prid;
		psc_dynarray_add(da, pp);
	}
	if (psc_dynarray_len(da) == 0)
		psc_fatalx("no LNET_NETWORKS specified");
}

void
pscrpc_getpridforpeer(lnet_process_id_t *pridp,
    const struct psc_dynarray *da, lnet_nid_t peer)
{
	lnet_process_id_t *pp;
	lnet_remotenet_t *lrn;
	lnet_route_t *lrt;
	int i, route = 1;

 rescan:
	DYNARRAY_FOREACH(pp, i, da)
		if (LNET_NIDNET(peer) == LNET_NIDNET(pp->nid)) {
			*pridp = *pp;
			return;
		}
	if (route) {
		route = 0;
		list_for_each_entry(lrn, &the_lnet.ln_remote_nets,
		    lrn_list) {
			if (LNET_NIDNET(peer) == lrn->lrn_net)
				list_for_each_entry(lrt,
				    &lrn->lrn_routes, lr_list) {
					peer = lrt->lr_gateway->lp_nid;
					goto rescan;
				}
		}
	}
	pridp->nid = LNET_NID_ANY;
}

void
pscrpc_req_getprids(const struct psc_dynarray *prids,
    struct pscrpc_request *rq, lnet_process_id_t *self,
    lnet_process_id_t *peer)
{
	if (rq->rq_import) {
		*peer = rq->rq_import->imp_connection->c_peer;
		pscrpc_getpridforpeer(self, prids, peer->nid);
		if (self->nid == LNET_NID_ANY) {
			errno = ENETUNREACH;
			psc_fatal("nid %"PSCPRIxLNID, peer->nid);
		}
	} else {
		/* there is no import, we must be a server padawan! */
		*peer = rq->rq_peer;
		self->nid = rq->rq_self;
		self->pid = the_lnet.ln_pid;
	}
}
