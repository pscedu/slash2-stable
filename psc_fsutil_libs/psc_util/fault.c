/* $Id$ */

#include <sys/time.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psc_ds/hash2.h"
#include "psc_util/alloc.h"
#include "psc_util/fault.h"
#include "psc_util/lock.h"
#include "psc_util/random.h"
#include "psc_util/strlcpy.h"

#define PSC_FAULT_NBUCKETS	16

static	int	fault_enabled = 0;

atomic_t		psc_fault_count;
struct psc_hashtbl	psc_fault_table;

void
psc_fault_init(void)
{
	atomic_set(&psc_fault_count, 0);

	psc_hashtbl_init(&psc_fault_table, PHTF_STR, struct psc_fault,
		pflt_name, pflt_hentry, PSC_FAULT_NBUCKETS, NULL, "faults");
}

int
psc_fault_add(const char *name)
{
	int			 i;
	struct psc_hashbkt	*b;
	int			 rc;
	struct psc_fault	*pflt;

	if (strlen(name) >= sizeof(pflt->pflt_name))
		return (ENAMETOOLONG);

	i = 0;
	rc = 0;
	while (1) {
		if (psc_fault_names[i] == NULL)
			return ENOENT;
		if (strcmp(name, psc_fault_names[i]) == 0)
			break;
		i++;
	}

	pflt = PSCALLOC(sizeof(*pflt));
	LOCK_INIT(&pflt->pflt_lock);
	strlcpy(pflt->pflt_name, name, sizeof(pflt->pflt_name));
	pflt->pflt_flags = PFLTF_ACTIVE;
	pflt->pflt_delay = 0;		/* no internal delay enforced */
	pflt->pflt_begin = 0;		/* wait zero time before triggered */
	pflt->pflt_chance = 100;	/* alway happens */
	pflt->pflt_count = 1;		/* one time only */
	pflt->pflt_retval = 0;		/* keep the original error code */

	b = psc_hashbkt_get(&psc_fault_table, name);
	psc_hashbkt_lock(b);
	if (psc_hashbkt_search(&psc_fault_table, b, NULL, NULL, name)) {
		rc = EEXIST;
		free(pflt);
	} else {
		fault_enabled = 1;
		atomic_inc(&psc_fault_count);
		psc_hashbkt_add_item(&psc_fault_table, b, pflt);
	}
	psc_hashbkt_unlock(b);
	return (rc);
}

int
psc_fault_remove(const char *name)
{
	struct psc_fault *pflt;
	struct psc_hashbkt *b;
	int rc;

	rc = ENOENT;
	b = psc_hashbkt_get(&psc_fault_table, name);
	psc_hashbkt_lock(b);
	pflt = psc_hashbkt_search(&psc_fault_table, b, NULL, NULL, name);
	if (pflt) {
		rc = 0;
		atomic_dec(&psc_fault_count);
		psc_hashent_remove(&psc_fault_table, pflt);
		free(pflt);
	}
	psc_hashbkt_unlock(b);
	return (rc);
}

void
psc_fault_take_lock(void *p)
{
	struct psc_fault *pflt;

	pflt = p;
	psc_fault_lock(pflt);
}


struct psc_fault *
psc_fault_lookup(const char *name)
{
	struct psc_fault *pflt;

	pflt = psc_hashtbl_search(&psc_fault_table, NULL, psc_fault_take_lock, name);
	return (pflt);
}

/*
 * Alternative to add().  This function allows us to set fault points even before control thread
 * is ready to receive commands.
 */
int
psc_fault_register(const char *name, int delay, int begin, int chance, int count, int retval)
{
	int			 i;
	struct psc_hashbkt	*b;
	int			 rc;
	struct psc_fault	*pflt;

	if (strlen(name) >= sizeof(pflt->pflt_name))
		return (ENAMETOOLONG);

	i = 0;
	rc = 0;
	while (1) {
		if (psc_fault_names[i] == NULL)
			return ENOENT;
		if (strcmp(name, psc_fault_names[i]) == 0)
			break;
		i++;
	}

	pflt = PSCALLOC(sizeof(*pflt));
	LOCK_INIT(&pflt->pflt_lock);
	strlcpy(pflt->pflt_name, name, sizeof(pflt->pflt_name));

	b = psc_hashbkt_get(&psc_fault_table, name);
	psc_hashbkt_lock(b);
	if (psc_hashbkt_search(&psc_fault_table, b, NULL, NULL, name)) {
		free(pflt);
	} else {
		atomic_inc(&psc_fault_count);
		psc_hashbkt_add_item(&psc_fault_table, b, pflt);
	}
	pflt->pflt_flags = PFLTF_ACTIVE;
	pflt->pflt_delay = delay;		/* no internal delay enforced */
	pflt->pflt_begin = begin;		/* wait zero time before triggered */
	pflt->pflt_chance = chance;		/* alway happens */
	pflt->pflt_count = count;		/* one time only */
	pflt->pflt_retval = retval;		/* keep the original error code */
	psc_hashbkt_unlock(b);
	return (rc);
}

void
psc_fault_here(const char *name, int *rc)
{
	struct psc_fault	*pflt;
	int			 dice;

	if (!fault_enabled) {
		return;
	}

	pflt = psc_hashtbl_search(&psc_fault_table, NULL, psc_fault_take_lock, name);
	if (pflt == NULL)
		return;

	if (!(pflt->pflt_flags & PFLTF_ACTIVE)) {
		psc_fault_unlock(pflt);
		return;
	}
	if (pflt->pflt_unhits < pflt->pflt_begin) {
		pflt->pflt_unhits++;
		psc_fault_unlock(pflt);
		return;
	}
	if (pflt->pflt_hits >= pflt->pflt_count) {
		pflt->pflt_unhits++;
		psc_fault_unlock(pflt);
		return;
	}
	dice = psc_random32u(100);
	if (dice > pflt->pflt_chance) {
		pflt->pflt_unhits++;
		psc_fault_unlock(pflt);
		return;
	}
	pflt->pflt_hits++;
	if (pflt->pflt_delay) {
		usleep(pflt->pflt_delay);
	}
	if (pflt->pflt_retval) {
		*rc = pflt->pflt_retval;
	}
	psc_fault_unlock(pflt);
}

void
psc_enter_debugger(char *str)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	printf("timestamp %lu:%lu, enter debugger (%s) ...\n", tv.tv_sec, tv.tv_usec, str);
	psc_notify("timestamp %lu:%lu, enter debugger (%s) ...\n", tv.tv_sec, tv.tv_usec, str);
	__asm__ __volatile__ ("int3");
}
