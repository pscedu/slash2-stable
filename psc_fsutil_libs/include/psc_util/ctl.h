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

/*
 * Control interface for querying and modifying
 * parameters of a running daemon instance.
 */

#ifndef _PFL_CTL_H_
#define _PFL_CTL_H_

#include <sys/types.h>

#include "psc_ds/hash2.h"
#include "psc_ds/listcache.h"
#include "psc_util/fault.h"
#include "psc_util/iostats.h"
#include "psc_util/meter.h"
#include "psc_util/mlist.h"
#include "psc_util/thread.h"

#define PCTHRNAME_EVERYONE	"everyone"

#define PCE_ERRMSG_MAX		128

struct psc_ctlmsg_error {
	char			pce_errmsg[PCE_ERRMSG_MAX];
};

#define PCSS_NAME_MAX 16	/* multiple of wordsize */

struct psc_ctlmsg_subsys {
	char			pcss_names[0];
};

struct psc_ctlmsg_loglevel {
	char			pcl_thrname[PSC_THRNAME_MAX];
	int32_t			pcl_levels[0];
};

struct psc_ctlmsg_lc {
	uint64_t		pclc_size;	/* #items on list */
	uint64_t		pclc_nseen;	/* max #items list can attain */
	int32_t			pclc_flags;
	int32_t			pclc_nw_want;	/* #waiters waking for a want */
	int32_t			pclc_nw_empty;	/* #waiters waking on empty */
	char			pclc_name[PLG_NAME_MAX];
};

#define PCLC_NAME_ALL		"all"

struct psc_ctlmsg_stats {
	char			pcst_thrname[PSC_THRNAME_MAX];
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

struct psc_ctlmsg_hashtable {
	int32_t			pcht_totalbucks;
	int32_t			pcht_usedbucks;
	int32_t			pcht_nents;
	int32_t			pcht_maxbucklen;
	int32_t			pcht_flags;
	char			pcht_name[PSC_HTNAME_MAX];
};

#define PCHT_NAME_ALL		"all"

#define PCP_FIELD_MAX		48
#define PCP_VALUE_MAX		4096

struct psc_ctlmsg_param {
	char			pcp_thrname[PSC_THRNAME_MAX];
	char			pcp_field[PCP_FIELD_MAX];
	char			pcp_value[PCP_VALUE_MAX];
	int32_t			pcp_flags;
};

#define PCPF_ADD	(1 << 0)	/* relative: addition */
#define PCPF_SUB	(1 << 1)	/* relative: subtraction */

struct psc_ctlmsg_iostats {
	struct psc_iostats	pci_ist;
};

#define PCI_NAME_ALL		"all"

struct psc_ctlmsg_meter {
	struct psc_meter	pcm_mtr;
};

#define PCM_NAME_ALL		"all"

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
	char			pcpl_name[PLG_NAME_MAX];
};

#define PCPL_NAME_ALL		"all"

struct psc_ctlmsg_mlist {
	uint64_t		 pcml_nseen;
	uint32_t		 pcml_size;
	uint32_t		 pcml_nwaiters;
	char			 pcml_name[PLG_NAME_MAX];
};

#define PCML_NAME_ALL		"all"

struct psc_ctlmsg_cmd {
	int32_t			 pcc_opcode;
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

#define PCFLT_NAME_ALL		"all"

/* Control message types. */
#define PCMT_ERROR		0
#define PCMT_GETLOGLEVEL	1
#define PCMT_GETLC		2
#define PCMT_GETSTATS		3
#define PCMT_GETSUBSYS		4
#define PCMT_GETHASHTABLE	5
#define PCMT_GETPARAM		6
#define PCMT_SETPARAM		7
#define PCMT_GETIOSTATS		8
#define PCMT_GETMETER		9
#define PCMT_GETPOOL		10
#define PCMT_GETMLIST		11
#define PCMT_GETFAULTS		12
#define PCMT_CMD		13
#define NPCMT			14

/*
 * Control message header.
 * This structure precedes each actual message.
 */
struct psc_ctlmsghdr {
	int			mh_type;
	int			mh_id;
	size_t			mh_size;
	unsigned char		mh_data[0];
};

#endif /* _PFL_CTL_H_ */
