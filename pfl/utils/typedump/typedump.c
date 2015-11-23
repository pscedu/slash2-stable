/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>

/* start includes */
#include "pfl/_atomic32.h"
#include "pfl/acl.h"
#include "pfl/acsvc.h"
#include "pfl/aio.h"
#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/base64.h"
#include "pfl/bitflag.h"
#include "pfl/buf.h"
#include "pfl/cdefs.h"
#include "pfl/compat.h"
#include "pfl/completion.h"
#include "pfl/crc.h"
#include "pfl/ctl.h"
#include "pfl/ctlcli.h"
#include "pfl/ctlsvr.h"
#include "pfl/dynarray.h"
#include "pfl/endian.h"
#include "pfl/eqpollthr.h"
#include "pfl/err.h"
#include "pfl/explist.h"
#include "pfl/export.h"
#include "pfl/fault.h"
#include "pfl/fcntl.h"
#include "pfl/fmt.h"
#include "pfl/fmtstr.h"
#include "pfl/fs.h"
#include "pfl/fts.h"
#include "pfl/getopt.h"
#include "pfl/hashtbl.h"
#include "pfl/heap.h"
#include "pfl/hostname.h"
#include "pfl/journal.h"
#include "pfl/list.h"
#include "pfl/listcache.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/mem.h"
#include "pfl/memnode.h"
#include "pfl/meter.h"
#include "pfl/mkdirs.h"
#include "pfl/mlist.h"
#include "pfl/mspinlock.h"
#include "pfl/multiwait.h"
#include "pfl/net.h"
#include "pfl/odtable.h"
#include "pfl/opstats.h"
#include "pfl/parity.h"
#include "pfl/pfl.h"
#include "pfl/pool.h"
#include "pfl/printhex.h"
#include "pfl/procenv.h"
#include "pfl/prsig.h"
#include "pfl/pthrutil.h"
#include "pfl/random.h"
#include "pfl/rlimit.h"
#include "pfl/rpc.h"
#include "pfl/rpc_intrfc.h"
#include "pfl/rpclog.h"
#include "pfl/rsx.h"
#include "pfl/service.h"
#include "pfl/setresuid.h"
#include "pfl/stat.h"
#include "pfl/str.h"
#include "pfl/stree.h"
#include "pfl/subsys.h"
#include "pfl/sys.h"
#include "pfl/syspaths.h"
#include "pfl/thread.h"
#include "pfl/time.h"
#include "pfl/timerthr.h"
#include "pfl/treeutil.h"
#include "pfl/types.h"
#include "pfl/umask.h"
#include "pfl/usklndthr.h"
#include "pfl/vbitmap.h"
#include "pfl/waitq.h"
#include "pfl/walk.h"
#include "pfl/wndmap.h"
#include "pfl/workthr.h"
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
	PRVAL(sizeof(((struct stat *)NULL)->st_size));
	PRVAL(sizeof(((struct stat *)NULL)->st_mtime));
#ifdef HAVE_STB_MTIM
	PRVAL(sizeof(((struct stat *)NULL)->st_mtim));
#endif

	PRTYPE(dev_t);
	PRTYPE(ino_t);
	PRTYPE(mode_t);
	PRTYPE(nlink_t);
	PRTYPE(uid_t);
	PRTYPE(gid_t);
	PRTYPE(off_t);
	PRTYPE(typeof(((struct stat *)NULL)->st_blksize));
	PRTYPE(typeof(((struct stat *)NULL)->st_blocks));
	PRTYPE(time_t);

	PRTYPE(rlim_t);

	PRTYPE(lnet_nid_t);
	PRTYPE(lnet_pid_t);
	PRTYPE(lnet_process_id_t);
	PRTYPE(ksock_msg_t);

	/* start structs */
	printf("structures:\n");
	PRTYPE(atomic_t);
	PRTYPE(blkcnt_t);
	PRTYPE(blksize_t);
	PRTYPE(psc_atomic_t);
	PRTYPE(pscfs_fgen_t);
	PRTYPE(pscfs_inum_t);
	PRTYPE(struct _ftsent);
	PRTYPE(struct aiocb);
	PRTYPE(struct ifaddrs);
	PRTYPE(struct l_wait_info);
	PRTYPE(struct pfl_callerinfo);
	PRTYPE(struct pfl_hashentry);
	PRTYPE(struct pfl_heap);
	PRTYPE(struct pfl_heap_entry);
	PRTYPE(struct pfl_iostats_grad);
	PRTYPE(struct pfl_iostats_rw);
	PRTYPE(struct pfl_logpoint);
	PRTYPE(struct pfl_mutex);
	PRTYPE(struct pfl_odt);
	PRTYPE(struct pfl_odt_entftr);
	PRTYPE(struct pfl_odt_hdr);
	PRTYPE(struct pfl_odt_ops);
	PRTYPE(struct pfl_odt_receipt);
	PRTYPE(struct pfl_odt_stats);
	PRTYPE(struct pfl_opstat);
	PRTYPE(struct pfl_rwlock);
	PRTYPE(struct pfl_strbuf);
	PRTYPE(struct pfl_timespec);
	PRTYPE(struct pfl_wk_thread);
	PRTYPE(struct pfl_workrq);
	PRTYPE(struct psc_atomic16);
	PRTYPE(struct psc_atomic32);
	PRTYPE(struct psc_atomic64);
	PRTYPE(struct psc_compl);
	PRTYPE(struct psc_ctlacthr);
	PRTYPE(struct psc_ctlcmd_req);
	PRTYPE(struct psc_ctlmsg_error);
	PRTYPE(struct psc_ctlmsg_fault);
	PRTYPE(struct psc_ctlmsg_hashtable);
	PRTYPE(struct psc_ctlmsg_journal);
	PRTYPE(struct psc_ctlmsg_listcache);
	PRTYPE(struct psc_ctlmsg_lnetif);
	PRTYPE(struct psc_ctlmsg_loglevel);
	PRTYPE(struct psc_ctlmsg_meter);
	PRTYPE(struct psc_ctlmsg_mlist);
	PRTYPE(struct psc_ctlmsg_odtable);
	PRTYPE(struct psc_ctlmsg_opstat);
	PRTYPE(struct psc_ctlmsg_param);
	PRTYPE(struct psc_ctlmsg_pool);
	PRTYPE(struct psc_ctlmsg_prfmt);
	PRTYPE(struct psc_ctlmsg_rpcrq);
	PRTYPE(struct psc_ctlmsg_rpcsvc);
	PRTYPE(struct psc_ctlmsg_subsys);
	PRTYPE(struct psc_ctlmsg_thread);
	PRTYPE(struct psc_ctlmsghdr);
	PRTYPE(struct psc_ctlop);
	PRTYPE(struct psc_ctlopt);
	PRTYPE(struct psc_ctlshow_ent);
	PRTYPE(struct psc_ctlthr);
	PRTYPE(struct psc_dynarray);
	PRTYPE(struct psc_explist);
	PRTYPE(struct psc_fault);
	PRTYPE(struct psc_hashbkt);
	PRTYPE(struct psc_hashtbl);
	PRTYPE(struct psc_journal);
	PRTYPE(struct psc_journal_cursor);
	PRTYPE(struct psc_journal_enthdr);
	PRTYPE(struct psc_journal_hdr);
	PRTYPE(struct psc_journal_xidhndl);
	PRTYPE(struct psc_listcache);
	PRTYPE(struct psc_lockedlist);
	PRTYPE(struct psc_memnode);
	PRTYPE(struct psc_meter);
	PRTYPE(struct psc_mlist);
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
	PRTYPE(struct psc_vbitmap);
	PRTYPE(struct psc_waitq);
	PRTYPE(struct psc_wndmap);
	PRTYPE(struct psc_wndmap_block);
	PRTYPE(struct pscfs);
	PRTYPE(struct pscfs_clientctx);
	PRTYPE(struct pscfs_creds);
	PRTYPE(struct pscfs_dirent);
	PRTYPE(struct psclist_head);
	PRTYPE(struct psclog_data);
	PRTYPE(struct pscrpc_async_args);
	PRTYPE(struct pscrpc_bulk_desc);
	PRTYPE(struct pscrpc_cb_id);
	PRTYPE(struct pscrpc_connection);
	PRTYPE(struct pscrpc_export);
	PRTYPE(struct pscrpc_handle);
	PRTYPE(struct pscrpc_import);
	PRTYPE(struct pscrpc_msg);
	PRTYPE(struct pscrpc_peer_qlen);
	PRTYPE(struct pscrpc_reply_state);
	PRTYPE(struct pscrpc_request);
	PRTYPE(struct pscrpc_request_buffer_desc);
	PRTYPE(struct pscrpc_request_pool);
	PRTYPE(struct pscrpc_request_set);
	PRTYPE(struct pscrpc_service);
	PRTYPE(struct pscrpc_svc_handle);
	PRTYPE(struct pscrpc_thread);
	PRTYPE(struct pscrpc_uuid);
	PRTYPE(struct pscrpc_wait_callback);
	PRTYPE(struct rnd_iterator);
	PRTYPE(struct rsx_msg_conversion);
	PRTYPE(struct rsx_msg_portablizer);
	PRTYPE(struct sigevent);
	/* end structs */

	/* start typedefs */
	/* end typedefs */

	/* start constants */
	/* end constants */

	/* start enums */
	printf("\nenums:\n");
	PRVAL(ACSOP_ACCESS);
	PRVAL(ACSOP_CHMOD);
	PRVAL(ACSOP_CHOWN);
	PRVAL(ACSOP_LINK);
	PRVAL(ACSOP_LSTAT);
	PRVAL(ACSOP_MKDIR);
	PRVAL(ACSOP_MKNOD);
	PRVAL(ACSOP_OPEN);
	PRVAL(ACSOP_READLINK);
	PRVAL(ACSOP_RENAME);
	PRVAL(ACSOP_RMDIR);
	PRVAL(ACSOP_STAT);
	PRVAL(ACSOP_STATFS);
	PRVAL(ACSOP_SYMLINK);
	PRVAL(ACSOP_TRUNCATE);
	PRVAL(ACSOP_UNLINK);
	PRVAL(ACSOP_UTIMES);
	PRVAL(NPCMT);
	PRVAL(PCMT_ERROR);
	PRVAL(PCMT_GETFAULT);
	PRVAL(PCMT_GETHASHTABLE);
	PRVAL(PCMT_GETJOURNAL);
	PRVAL(PCMT_GETLISTCACHE);
	PRVAL(PCMT_GETLNETIF);
	PRVAL(PCMT_GETLOGLEVEL);
	PRVAL(PCMT_GETMETER);
	PRVAL(PCMT_GETMLIST);
	PRVAL(PCMT_GETODTABLE);
	PRVAL(PCMT_GETOPSTATS);
	PRVAL(PCMT_GETPARAM);
	PRVAL(PCMT_GETPOOL);
	PRVAL(PCMT_GETRPCRQ);
	PRVAL(PCMT_GETRPCSVC);
	PRVAL(PCMT_GETSUBSYS);
	PRVAL(PCMT_GETTHREAD);
	PRVAL(PCMT_SETPARAM);
	PRVAL(PCOF_FLAG);
	PRVAL(PCOF_FUNC);
	PRVAL(PFLCTL_PARAMT_ATOMIC32);
	PRVAL(PFLCTL_PARAMT_INT);
	PRVAL(PFLCTL_PARAMT_NONE);
	PRVAL(PFLCTL_PARAMT_STR);
	PRVAL(PFLCTL_PARAMT_UINT64);
	PRVAL(PSCRPC_IMP_CLOSED);
	PRVAL(PSCRPC_IMP_CONNECTING);
	PRVAL(PSCRPC_IMP_DISCON);
	PRVAL(PSCRPC_IMP_EVICTED);
	PRVAL(PSCRPC_IMP_FULL);
	PRVAL(PSCRPC_IMP_NEW);
	PRVAL(PSCRPC_IMP_NOOP);
	PRVAL(PSCRPC_IMP_RECOVER);
	PRVAL(PSCRPC_IMP_REPLAY);
	PRVAL(PSCRPC_IMP_REPLAY_LOCKS);
	PRVAL(PSCRPC_IMP_REPLAY_WAIT);
	PRVAL(PSCRPC_RQ_PHASE_BULK);
	PRVAL(PSCRPC_RQ_PHASE_COMPLETE);
	PRVAL(PSCRPC_RQ_PHASE_INTERPRET);
	PRVAL(PSCRPC_RQ_PHASE_NEW);
	PRVAL(PSCRPC_RQ_PHASE_RPC);
	PRVAL(PSL_LOCKED);
	PRVAL(PSL_UNLOCKED);
	/* end enums */

	PRVAL(offsetof(struct psc_listcache, plc_listhd));
	PRVAL(offsetof(struct psc_journal_enthdr, pje_data));
	PRVAL(PSCFMT_HUMAN_BUFSIZ);
	PRVAL(PSCFMT_RATIO_BUFSIZ);
	PRVAL(PSCRPC_NIDSTR_SIZE);

	exit(0);
}
