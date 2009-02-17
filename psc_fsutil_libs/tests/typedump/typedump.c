/* $Id$ */

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>

#include "psc_ds/hash2.h"
#include "psc_ds/pool.h"
#include "psc_ds/stree.h"
#include "psc_ds/vbitmap.h"
#include "psc_mount/dhfh.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/service.h"
#include "psc_util/cdefs.h"
#include "psc_util/ctl.h"
#include "psc_util/humanscale.h"
#include "psc_util/journal.h"
#include "psc_util/mlist.h"
#include "psc_util/multilock.h"
#include "psc_util/waitq.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;

	progname = argv[0];
	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

#define PRTYPE(type) \
	printf("%-32s %zu\n", #type, sizeof(type))

#define PRVAL(val) \
	printf("%-32s %lu\n", #val, (unsigned long)(val))

	/* C types/values */
	PRTYPE(int);
	PRTYPE(unsigned char);
	PRTYPE(unsigned short);
	PRTYPE(void *);
	PRTYPE(uint8_t);
	PRTYPE(uint16_t);
	PRTYPE(uint32_t);
	PRTYPE(uint64_t);
	PRTYPE(int8_t);
	PRTYPE(int16_t);
	PRTYPE(int32_t);
	PRTYPE(int64_t);
	PRTYPE(INT_MAX);
	PRTYPE(UINT64_MAX);

	/* system types/values */
	PRVAL(PATH_MAX);

	/* PFL types/values */
	PRTYPE(struct psc_ctlmsghdr);
	PRTYPE(psc_crc_t);
	PRTYPE(struct dynarray);
	PRTYPE(struct psc_hashbkt);
	PRTYPE(struct psc_hashtbl);
	PRTYPE(struct psc_hashent);
	PRTYPE(struct psclist_head);
	PRTYPE(struct psc_listcache);
	PRTYPE(struct psc_lockedlist);
	PRTYPE(struct psc_poolset);
	PRTYPE(struct psc_poolmaster);
	PRTYPE(struct psc_poolmgr);
	PRTYPE(struct psc_streenode);
	PRTYPE(struct vbitmap);
	PRTYPE(struct fhent);

	PRTYPE(struct pscrpc_connection);
	PRTYPE(struct pscrpc_export);
	PRTYPE(struct pscrpc_import);
	PRTYPE(struct pscrpc_request);
	PRTYPE(struct pscrpc_bulk_desc);
	PRTYPE(struct pscrpc_service);
	PRTYPE(struct pscrpc_svc_handle);
	PRVAL(PSC_NIDSTR_SIZE);

	PRTYPE(struct psc_ctlmsg_error);
	PRTYPE(struct psc_ctlmsg_subsys);
	PRTYPE(struct psc_ctlmsg_loglevel);
	PRTYPE(struct psc_ctlmsg_lc);
	PRTYPE(struct psc_ctlmsg_stats);
	PRTYPE(struct psc_ctlmsg_hashtable);
	PRTYPE(struct psc_ctlmsg_param);
	PRTYPE(struct psc_ctlmsg_iostats);
	PRTYPE(struct psc_ctlmsg_meter);
	PRTYPE(struct psc_ctlmsg_pool);
	PRVAL(PSC_CTL_HUMANBUF_SZ);
	PRTYPE(struct iostats);
	PRTYPE(struct psc_journal);
	PRTYPE(struct psc_journal_enthdr);
	PRTYPE(struct psc_meter);
	PRTYPE(struct psc_mlist);
	PRTYPE(struct multilock);
	PRTYPE(struct multilock_cond);
	PRTYPE(struct psc_thread);
	PRTYPE(struct psc_waitq);

	PRVAL(offsetof(struct psc_listcache, lc_listhd));

	exit(0);
}
