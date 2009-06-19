/* $Id$ */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "psc_ds/hash2.h"
#include "psc_util/alloc.h"
#include "psc_util/fault.h"
#include "psc_util/lock.h"
#include "psc_util/strlcpy.h"

#define PSC_FAULT_NBUCKETS	255

struct psc_hashtbl psc_faults;

void
psc_faults_init(void)
{
	psc_hashtbl_init(&psc_faults, PHTF_STR, struct psc_fault,
	    pflt_name, pflt_hentry, PSC_FAULT_NBUCKETS, NULL, "faults");
}

int
psc_fault_add(const char *name)
{
	struct psc_fault *pflt;
	struct psc_hashbkt *b;
	int rc;

	if (strlen(name) >= sizeof(pflt->pflt_name))
		return (ENAMETOOLONG);

	pflt = PSCALLOC(sizeof(*pflt));
	LOCK_INIT(&pflt->pflt_lock);
	strlcpy(pflt->pflt_name, name, sizeof(pflt->pflt_name));

	rc = 0;
	b = psc_hashbkt_get(&psc_faults, name);
	psc_hashbkt_lock(b);
	if (psc_hashbkt_search(&psc_faults, b, NULL, NULL, name))
		rc = EEXIST;
	else {
		psc_hashbkt_add_item(&psc_faults, b, pflt);
		pflt = NULL;
	}
	psc_hashbkt_unlock(b);
	free(pflt);
	return (rc);
}

int
psc_fault_remove(const char *name)
{
	struct psc_fault *pflt;
	struct psc_hashbkt *b;
	int rc;

	rc = 0;
	b = psc_hashbkt_get(&psc_faults, name);
	psc_hashbkt_lock(b);
	pflt = psc_hashbkt_search(&psc_faults, b, NULL, NULL, name);
	if (pflt)
		psc_hashent_remove(&psc_faults, pflt);
	psc_hashbkt_unlock(b);
	free(pflt);
	if (pflt == NULL)
		rc = ENOENT;
	return (rc);
}

struct psc_fault *
psc_fault_lookup(const char *name)
{
	struct psc_fault *pflt;

	pflt = psc_hashtbl_search(&psc_faults, NULL, NULL, name);
	if (pflt)
		psc_fault_lock(pflt);
	return (pflt);
}
