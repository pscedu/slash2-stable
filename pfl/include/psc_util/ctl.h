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

/*
 * Control interface for querying and modifying parameters of a
 * running daemon instance.
 */

#ifndef _PFL_CTL_H_
#define _PFL_CTL_H_

#include <sys/types.h>

#include "pfl/explist.h"
#include "pfl/hashtbl.h"
#include "pfl/listcache.h"
#include "pfl/rpc.h"
#include "pfl/service.h"
#include "psc_util/fault.h"
#include "psc_util/iostats.h"
#include "psc_util/journal.h"
#include "psc_util/meter.h"
#include "psc_util/mlist.h"
#include "psc_util/odtable.h"
#include "psc_util/thread.h"

#define PCTHRNAME_EVERYONE	"everyone"

#define PCE_ERRMSG_MAX		256

struct psc_ctlmsg_error {
	char			pce_errmsg[PCE_ERRMSG_MAX];
};

struct psc_ctlmsg_fault {
	char			pcflt_thrname[PSC_THRNAME_MAX];
	char			pcflt_name[PSC_FAULT_NAME_MAX];
	uint32_t		pcflt_flags;
	uint32_t		pcflt_hits;
	uint32_t		pcflt_unhits;
	uint32_t		pcflt_delay;
	uint32_t		pcflt_count;
	uint32_t		pcflt_begin;
	int32_t			pcflt_retval;
	int8_t			pcflt_chance;
};

struct psc_ctlmsg_hashtable {
	int32_t			pcht_totalbucks;
	int32_t			pcht_usedbucks;
	int32_t			pcht_nents;
	int32_t			pcht_maxbucklen;
	int32_t			pcht_flags;
	char			pcht_name[PSC_HTNAME_MAX];
};

struct psc_ctlmsg_iostats {
	struct psc_iostats	pci_ist;
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

struct psc_ctlmsg_lc {
	char			pclc_name[PEXL_NAME_MAX];
	uint64_t		pclc_size;	/* #items on list */
	uint64_t		pclc_nseen;	/* max #items list can attain */
	int32_t			pclc_flags;
	int32_t			pclc_nw_want;	/* #waiters waking for a want */
	int32_t			pclc_nw_empty;	/* #waiters waking on empty */
};

#define PCLC_NAME_ALL		"all"

struct psc_ctlmsg_lni {
	char			pclni_nid[PSCRPC_NIDSTR_SIZE];
	int32_t			pclni_maxtxcredits;
	int32_t			pclni_txcredits;
	int32_t			pclni_mintxcredits;
	int32_t			pclni_peertxcredits;
	int32_t			pclni_refcount;
};

struct psc_ctlmsg_loglevel {
	char			pcl_thrname[PSC_THRNAME_MAX];
	int32_t			pcl_levels[0];
};

struct psc_ctlmsg_meter {
	struct psc_meter	pcm_mtr;
};

struct psc_ctlmsg_mlist {
	uint64_t		 pcml_nseen;
	uint32_t		 pcml_size;
	uint32_t		 pcml_nwaiters;
	char			 pcml_name[PEXL_NAME_MAX];
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

struct psc_ctlmsg_thread {
	char			pcst_thrname[PSC_THRNAME_MAX];
#ifdef HAVE_NUMA
	int32_t			pcst_memnode;
#endif
	int32_t			pcst_thrtype;
	int32_t			pcst_thrid;
	uint32_t		pcst_flags;
	uint32_t		pcst_u32_1;
	uint32_t		pcst_u32_2;
	uint32_t		pcst_u32_3;
	uint32_t		pcst_u32_4;
};

/* psc_ctlthr aliases */
#define pcst_nclients	pcst_u32_1
#define pcst_nsent	pcst_u32_2
#define pcst_nrecv	pcst_u32_3
#define pcst_ndrop	pcst_u32_4

/* Control message types. */
enum {
	PCMT_ERROR = 0,
	PCMT_GETFAULT,
	PCMT_GETHASHTABLE,
	PCMT_GETIOSTATS,
	PCMT_GETJOURNAL,
	PCMT_GETLC,
	PCMT_GETLNI,
	PCMT_GETLOGLEVEL,
	PCMT_GETMETER,
	PCMT_GETMLIST,
	PCMT_GETODTABLE,
	PCMT_GETPARAM,
	PCMT_GETPOOL,
	PCMT_GETRPCSVC,
	PCMT_GETSUBSYS,
	PCMT_GETTHREAD,
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
