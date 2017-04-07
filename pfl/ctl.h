/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
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

/*
 * Control interface for querying and modifying parameters of a
 * running daemon instance.
 */

#ifndef _PFL_CTL_H_
#define _PFL_CTL_H_

#include <sys/types.h>

#include "pfl/explist.h"
#include "pfl/fault.h"
#include "pfl/hashtbl.h"
#include "pfl/opstats.h"
#include "pfl/journal.h"
#include "pfl/listcache.h"
#include "pfl/meter.h"
#include "pfl/mlist.h"
#include "pfl/odtable.h"
#include "pfl/rpc_intrfc.h"
#include "pfl/service.h"
#include "pfl/thread.h"

#define PCTHRNAME_EVERYONE	"everyone"

#define PCE_ERRMSG_MAX		256

struct psc_ctlmsg_error {
	char			pce_errmsg[PCE_ERRMSG_MAX];
};

struct psc_ctlmsg_fault {
	char			pcflt_thrname[PSC_THRNAME_MAX];
	char			pcflt_name[PFL_FAULT_NAME_MAX];
	uint32_t		pcflt_flags;
	uint32_t		pcflt_hits;
	uint32_t		pcflt_unhits;
	uint32_t		pcflt_delay;
	uint32_t		pcflt_count;
	uint32_t		pcflt_begin;
	int32_t			pcflt_retval;
	int8_t			pcflt_chance;
	int32_t			pcflt_interval;
};

struct psc_ctlmsg_hashtable {
	int32_t			pcht_totalbucks;
	int32_t			pcht_usedbucks;
	int32_t			pcht_nents;
	int32_t			pcht_maxbucklen;
	int32_t			pcht_flags;
	char			pcht_name[PSC_HTNAME_MAX];
};

#define OPST_NAME_MAX 64
struct psc_ctlmsg_opstat {
	char			pco_name[OPST_NAME_MAX];
	struct pfl_opstat	pco_opst;
};

#define PCI_NAME_ALL		"all"

struct psc_ctlmsg_journal {
	char			pcj_name[PJ_NAME_MAX];
	uint32_t		pcj_flags;
	uint32_t		pcj_inuse;
	uint32_t		pcj_total;
	uint32_t		pcj_resrv;
	uint64_t		pcj_lastxid;
	uint64_t		pcj_commit_txg;
	uint64_t		pcj_replay_xid;
	uint64_t		pcj_dstl_xid;
	uint32_t		pcj_pndg_xids_cnt;
	uint32_t		pcj_dstl_xids_cnt;
	uint32_t		pcj_bufs_cnt;
	uint32_t		pcj_nwaiters;
	uint32_t		pcj_nextwrite;
	uint64_t		pcj_wraparound;
};

struct psc_ctlmsg_listcache {
	char			pclc_name[PEXL_NAME_MAX];
	uint64_t		pclc_size;	/* #items on list */
	uint64_t		pclc_nseen;	/* max #items list can attain */
	int32_t			pclc_flags;
	int32_t			pclc_nw_want;	/* #waiters waking for a want */
	int32_t			pclc_nw_empty;	/* #waiters waking on empty */
};

#define PCLC_NAME_ALL		"all"

struct psc_ctlmsg_lnetif {
	char			pclni_nid[PSCRPC_NIDSTR_SIZE];
	int32_t			pclni_maxtxcredits;
	int32_t			pclni_txcredits;
	int32_t			pclni_mintxcredits;
	int32_t			pclni_peertxcredits;
	int32_t			pclni_refcount;
};

struct psc_ctlmsg_thread {
	char			pct_thrname[PSC_THRNAME_MAX];
	char			pct_waitname[MAX_WQ_NAME];
	int32_t			pct_memnode;
	uint32_t		pct_flags;
	uint32_t		pct_tid;
	int32_t			pct_loglevels[0];
};

struct psc_ctlmsg_meter {
	struct pfl_meter	pcm_mtr;
};

struct psc_ctlmsg_mlist {
	uint64_t		pcml_nseen;
	uint32_t		pcml_size;
	uint32_t		pcml_nwaiters;
	char			pcml_name[PEXL_NAME_MAX];
};

struct psc_ctlmsg_odtable {
	char			pco_name[ODT_NAME_MAX];
	int32_t			pco_inuse;
	int32_t			pco_total;
	int32_t			pco_elemsz;
	int32_t			pco_opts;
};

#define PCP_FIELD_MAX		48
#define PCP_VALUE_MAX		4096

struct psc_ctlmsg_param {
	char			pcp_thrname[PSC_THRNAME_MAX];
	char			pcp_field[PCP_FIELD_MAX];
	char			pcp_value[PCP_VALUE_MAX];
	int32_t			pcp_flags;
};

#define PCPF_ADD		(1 << 0)	/* relative: addition */
#define PCPF_SUB		(1 << 1)	/* relative: subtraction */

struct psc_ctlmsg_pool {
	int32_t			pcpl_min;
	int32_t			pcpl_max;
	int32_t			pcpl_total;
	int32_t			pcpl_free;
	int32_t			pcpl_flags;
	int32_t			pcpl_thres;
	int32_t			pcpl_nw_want;
	int32_t			pcpl_nw_empty;
	uint64_t		pcpl_ngrow;
	uint64_t		pcpl_nshrink;
	uint64_t		pcpl_nseen;
	char			pcpl_name[PEXL_NAME_MAX];
};

#define PCPL_NAME_ALL		"all"

struct psc_ctlmsg_rpcrq {
	uint64_t		 pcrq_addr;
	 int32_t		 pcrq_type;
	 int32_t		 pcrq_status;
	 int32_t		 pcrq_timeout;
	 int32_t		 pcrq_request_portal;
	 int32_t		 pcrq_reply_portal;
	 int32_t		 pcrq_nob_received;
	 int32_t		 pcrq_reqlen;
	 int32_t		 pcrq_replen;
	 int32_t		 pcrq_import_generation;
	 int32_t		 pcrq_opc;
	 int64_t		 pcrq_sent;
	uint64_t		 pcrq_transno;
	uint64_t		 pcrq_xid;
	uint64_t		 pcrq_history_seq;
	unsigned int		 pcrq_intr:1,
				 pcrq_replied:1,
				 pcrq_err:1,
				 pcrq_timedout:1,
				 pcrq_resend:1,
				 pcrq_restart:1,
				 pcrq_replay:1,
				 pcrq_no_resend:1,
				 pcrq_waiting:1,
				 pcrq_receiving_reply:1,
				 pcrq_no_delay:1,
				 pcrq_net_err:1,
				 pcrq_abort_reply:1,
				 pcrq_timeoutable:1,

				 pcrq_has_bulk:1,
				 pcrq_has_set:1,
				 pcrq_has_intr:1,

				 pcrq_bulk_abortable:1;
	 int32_t		 pcrq_refcount;
	 int32_t		 pcrq_retries;
	char			 pcrq_peer[PSCRPC_NIDSTR_SIZE];
	char			 pcrq_self[PSCRPC_NIDSTR_SIZE];
	uint32_t		 pcrq_phase;
	 int32_t		 pcrq_send_state;
	 int32_t		 pcrq_nwaiters;
};

struct psc_ctlmsg_rpcsvc {
	char			pcrs_name[PSCRPC_SVCNAME_MAX];
	uint32_t		pcrs_flags;
	uint32_t		pcrs_rqptl;
	uint32_t		pcrs_rpptl;
	int32_t			pcrs_rqsz;
	int32_t			pcrs_rpsz;
	int32_t			pcrs_bufsz;
	int32_t			pcrs_nbufs;
	int32_t			pcrs_nthr;
	int32_t			pcrs_nque;
	int32_t			pcrs_nact;
	int32_t			pcrs_nwq;
	int32_t			pcrs_nrep;
	int32_t			pcrs_nrqbd;
};

#define PCSS_NAME_MAX		16	/* must be multiple of wordsize */

struct psc_ctlmsg_subsys {
	char			pcss_names[0];
};

struct pfl_ctlmsg_fsrq {
	char			 pcfr_mod[16];
	uint64_t		 pcfr_req;
	uint64_t		 pcfr_ufsi_req;
	uint64_t		 pcfr_ufsi_fhdata;
	 int32_t		 pcfr_euid;
	 int32_t		 pcfr_pid;
	struct pfl_timespec	 pcfr_start;
	 int32_t		 pcfr_flags;
	 int32_t		 pcfr_refcnt;
	char			 pcfr_opname[16];
	char			 pcfr_thread[PSC_THRNAME_MAX];
	 int32_t		 pcfr_rc;
};

#define PFLCTL_FSRQF_INTR	(1 << 0)

struct pfl_ctlmsg_workrq {
	uint64_t		 pcw_addr;
	char			 pcw_type[32];
};

/* Control message types. The folowing must match PSC_CTLDEFOPS */
enum {
	PCMT_ERROR = 0,
	PCMT_GETFAULT,
	PCMT_GETFSRQ,
	PCMT_GETHASHTABLE,
	PCMT_GETJOURNAL,
	PCMT_GETLISTCACHE,
	PCMT_GETLNETIF,
	PCMT_GETMETER,
	PCMT_GETMLIST,
	PCMT_GETODTABLE,
	PCMT_GETOPSTATS,
	PCMT_GETPARAM,
	PCMT_GETPOOL,
	PCMT_GETRPCRQ,
	PCMT_GETRPCSVC,
	PCMT_GETSUBSYS,
	PCMT_GETTHREAD,
	PCMT_GETWORKRQ,
	PCMT_SETPARAM,
	NPCMT
};

/*
 * Control message header.
 * This structure precedes each control message.
 */
struct psc_ctlmsghdr {
	int			mh_type;
	int			mh_id;
	size_t			mh_size;
	unsigned char		mh_data[0];
};

#endif /* _PFL_CTL_H_ */
