/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>

/* start includes */
#include "pfl/cdefs.h"
#include "pfl/compat.h"
#include "pfl/fcntl.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "pfl/types.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/hash2.h"
#include "psc_ds/list.h"
#include "psc_ds/listcache.h"
#include "psc_ds/listguts.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/stree.h"
#include "psc_ds/treeutil.h"
#include "psc_ds/vbitmap.h"
#include "psc_rpc/export.h"
#include "psc_rpc/rpc.h"
#include "psc_rpc/rpclog.h"
#include "psc_rpc/rsx.h"
#include "psc_rpc/service.h"
#include "psc_util/acsvc.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/base64.h"
#include "psc_util/bitflag.h"
#include "psc_util/completion.h"
#include "psc_util/crc.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlcli.h"
#include "psc_util/ctlsvr.h"
#include "psc_util/daemon.h"
#include "psc_util/eqpollthr.h"
#include "psc_util/fault.h"
#include "psc_util/fmt.h"
#include "psc_util/fmtstr.h"
#include "psc_util/hostname.h"
#include "psc_util/iostats.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/mem.h"
#include "psc_util/memnode.h"
#include "psc_util/meter.h"
#include "psc_util/mkdirs.h"
#include "psc_util/mlist.h"
#include "psc_util/mspinlock.h"
#include "psc_util/multiwait.h"
#include "psc_util/net.h"
#include "psc_util/odtable.h"
#include "psc_util/parity.h"
#include "psc_util/pool.h"
#include "psc_util/printhex.h"
#include "psc_util/prsig.h"
#include "psc_util/pthrutil.h"
#include "psc_util/random.h"
#include "psc_util/rlimit.h"
#include "psc_util/semaphore.h"
#include "psc_util/setprocesstitle.h"
#include "psc_util/spinlock.h"
#include "psc_util/subsys.h"
#include "psc_util/thread.h"
#include "psc_util/time.h"
#include "psc_util/timerthr.h"
#include "psc_util/umask.h"
#include "psc_util/usklndthr.h"
#include "psc_util/waitq.h"
#include "psc_util/wndmap.h"
/* end includes */

#include "lnet/socklnd.h"

const char *progname;

void
pr(const char *name, uint64_t value)
{
	static int i;
	int n;

	if (i++ % 2) {
		n = printf("%s ", name);
		while (n++ <= 50)
			putchar('-');
		if (n < 53)
			printf("> ");
		printf("%"PRIu64"\n", value);
	} else
		printf("%-52s %"PRIu64"\n", name, value);
}

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

#define PRTYPE(type)	pr(#type, sizeof(type))
#define PRVAL(val)	pr(#val, (unsigned long)(val))

	/* base types/values */
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

	PRVAL(sizeof(((struct stat *)NULL)->st_dev));
	PRVAL(sizeof(((struct stat *)NULL)->st_nlink));
	PRVAL(sizeof(((struct stat *)NULL)->st_blksize));

	PRTYPE(dev_t);
	PRTYPE(ino_t);
	PRTYPE(mode_t);
	PRTYPE(nlink_t);
	PRTYPE(uid_t);
	PRTYPE(gid_t);
	PRTYPE(off_t);
	PRTYPE(blksize_t);
	PRTYPE(blkcnt_t);
	PRTYPE(time_t);

	PRTYPE(rlim_t);

	PRTYPE(lnet_nid_t);
	PRTYPE(lnet_pid_t);
	PRTYPE(lnet_process_id_t);
	PRTYPE(ksock_msg_t);

/* start structs */
	PRTYPE(atomic_t);
	PRTYPE(psc_atomic_t);
	PRTYPE(psc_crc32_t);
	PRTYPE(psc_crc64_t);
	PRTYPE(psc_spinlock_t);
	PRTYPE(pscrpc_svc_handle_t);
	PRTYPE(struct ifaddrs);
	PRTYPE(struct l_wait_info);
	PRTYPE(struct odtable);
	PRTYPE(struct odtable_entftr);
	PRTYPE(struct odtable_hdr);
	PRTYPE(struct odtable_receipt);
	PRTYPE(struct psc_atomic16);
	PRTYPE(struct psc_atomic32);
	PRTYPE(struct psc_atomic64);
	PRTYPE(struct psc_completion);
	PRTYPE(struct psc_ctl_thrstatfmt);
	PRTYPE(struct psc_ctlcmd_req);
	PRTYPE(struct psc_ctlmsg_cmd);
	PRTYPE(struct psc_ctlmsg_error);
	PRTYPE(struct psc_ctlmsg_fault);
	PRTYPE(struct psc_ctlmsg_hashtable);
	PRTYPE(struct psc_ctlmsg_iostats);
	PRTYPE(struct psc_ctlmsg_lc);
	PRTYPE(struct psc_ctlmsg_loglevel);
	PRTYPE(struct psc_ctlmsg_meter);
	PRTYPE(struct psc_ctlmsg_mlist);
	PRTYPE(struct psc_ctlmsg_param);
	PRTYPE(struct psc_ctlmsg_pool);
	PRTYPE(struct psc_ctlmsg_prfmt);
	PRTYPE(struct psc_ctlmsg_stats);
	PRTYPE(struct psc_ctlmsg_subsys);
	PRTYPE(struct psc_ctlmsghdr);
	PRTYPE(struct psc_ctlop);
	PRTYPE(struct psc_ctlshow_ent);
	PRTYPE(struct psc_ctlthr);
	PRTYPE(struct psc_dynarray);
	PRTYPE(struct psc_fault);
	PRTYPE(struct psc_hashbkt);
	PRTYPE(struct psc_hashent);
	PRTYPE(struct psc_hashtbl);
	PRTYPE(struct psc_iostats);
	PRTYPE(struct psc_journal);
	PRTYPE(struct psc_journal_enthdr);
	PRTYPE(struct psc_journal_hdr);
	PRTYPE(struct psc_journal_shdw);
	PRTYPE(struct psc_journal_shdw_tile);
	PRTYPE(struct psc_journal_xidhndl);
	PRTYPE(struct psc_listcache);
	PRTYPE(struct psc_listcache_entry);
	PRTYPE(struct psc_listguts);
	PRTYPE(struct psc_lockedlist);
	PRTYPE(struct psc_memnode);
	PRTYPE(struct psc_meter);
	PRTYPE(struct psc_mlist);
	PRTYPE(struct psc_msg);
	PRTYPE(struct psc_mspinlock);
	PRTYPE(struct psc_multiwait);
	PRTYPE(struct psc_multiwaitcond);
	PRTYPE(struct psc_nodemask);
	PRTYPE(struct psc_poolmaster);
	PRTYPE(struct psc_poolmgr);
	PRTYPE(struct psc_poolset);
	PRTYPE(struct psc_spinlock);
	PRTYPE(struct psc_streenode);
	PRTYPE(struct psc_thread);
	PRTYPE(struct psc_usklndthr);
	PRTYPE(struct psc_uuid);
	PRTYPE(struct psc_vbitmap);
	PRTYPE(struct psc_waitq);
	PRTYPE(struct psc_wndmap);
	PRTYPE(struct psc_wndmap_block);
	PRTYPE(struct psclist_head);
	PRTYPE(struct psclog_data);
	PRTYPE(struct pscrpc_async_args);
	PRTYPE(struct pscrpc_bulk_desc);
	PRTYPE(struct pscrpc_cb_id);
	PRTYPE(struct pscrpc_client);
	PRTYPE(struct pscrpc_connection);
	PRTYPE(struct pscrpc_export);
	PRTYPE(struct pscrpc_handle);
	PRTYPE(struct pscrpc_import);
	PRTYPE(struct pscrpc_nbreqset);
	PRTYPE(struct pscrpc_peer_qlen);
	PRTYPE(struct pscrpc_reply_state);
	PRTYPE(struct pscrpc_request);
	PRTYPE(struct pscrpc_request_buffer_desc);
	PRTYPE(struct pscrpc_request_pool);
	PRTYPE(struct pscrpc_request_set);
	PRTYPE(struct pscrpc_service);
	PRTYPE(struct pscrpc_svc_handle);
	PRTYPE(struct pscrpc_thread);
	PRTYPE(struct pscrpc_wait_callback);
	PRTYPE(struct rsx_msg_conversion);
	PRTYPE(struct rsx_msg_portablizer);
	/* end structs */

	PRVAL(offsetof(struct psc_listcache, lc_listhd));
	PRVAL(offsetof(struct psc_journal_enthdr, pje_data));
	PRVAL(PSCFMT_HUMAN_BUFSIZ);
	PRVAL(PSCFMT_RATIO_BUFSIZ);
	PRVAL(PSC_NIDSTR_SIZE);

	exit(0);
}
