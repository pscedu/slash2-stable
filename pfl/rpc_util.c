/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <errno.h>
#include <string.h>

#include "pfl/alloc.h"
#include "pfl/dynarray.h"
#include "pfl/err.h"
#include "pfl/list.h"
#include "pfl/log.h"
#include "pfl/net.h"
#include "pfl/rpc.h"

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
	if (rq->rq_import && rq->rq_import->imp_connection) {
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

int
pflrpc_portable_errno(int error)
{
	switch (error) {
	case -ETIMEDOUT: return (-PFLERR_TIMEDOUT);
	}
	return (error);
}
