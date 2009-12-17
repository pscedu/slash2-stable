/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <errno.h>
#include <string.h>

#include "psc_ds/dynarray.h"
#include "psc_rpc/rpc.h"
#include "psc_util/alloc.h"
#include "psc_util/log.h"

void
pscrpc_getlocalnids(struct psc_dynarray *da)
{
	lnet_process_id_t prid;
	lnet_nid_t *nid;
	int rc, n;

	for (n = 0; ; n++) {
		rc = LNetGetId(n, &prid);
		if (rc == -ENOENT)
			break;
		else if (rc)
			psc_fatalx("LNetGetId: %s", strerror(-rc));

		nid = PSCALLOC(sizeof(*nid));
		*nid = prid.nid;
		psc_dynarray_add(da, nid);
	}
	if (psc_dynarray_len(da) == 0)
		psc_fatalx("no LNET_NETWORKS specified");
}

lnet_nid_t
pscrpc_getnidforpeer(struct psc_dynarray *da, lnet_nid_t peer)
{
	lnet_nid_t *np;
	int i;

	DYNARRAY_FOREACH(np, i, da)
		if (LNET_NIDNET(peer) == LNET_NIDNET(*np))
			return (*np);
	return (LNET_NID_ANY);
}
