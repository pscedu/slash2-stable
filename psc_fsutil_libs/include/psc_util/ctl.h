/* $Id$ */

/*
 * Control interface for querying and modifying
 * parameters of a running daemon instance.
 */

#ifndef __PFL_CTL_H__
#define __PFL_CTL_H__

#include <sys/types.h>

#include "psc_ds/hash.h"
#include "psc_ds/listcache.h"
#include "psc_util/iostats.h"
#include "psc_util/meter.h"
#include "psc_util/thread.h"

#define PCTHRNAME_EVERYONE	"everyone"

#define PCE_ERRMSG_MAX		50

struct psc_ctlmsg_error {
	char			pce_errmsg[PCE_ERRMSG_MAX];
};

#define PCSS_NAME_MAX 16	/* multiple of wordsize */

struct psc_ctlmsg_subsys {
	char			pcss_names[0];
};

struct psc_ctlmsg_loglevel {
	char			pcl_thrname[PSC_THRNAME_MAX];
	int			pcl_levels[0];
};

struct psc_ctlmsg_lc {
	char			pclc_name[LC_NAME_MAX];
	size_t			pclc_max;	/* max #items list can attain */
	size_t			pclc_size;	/* #items on list */
	size_t			pclc_nseen;	/* max #items list can attain */
};

#define PCLC_NAME_ALL		"all"

struct psc_ctlmsg_stats {
	char			pcst_thrname[PSC_THRNAME_MAX];
	int			pcst_thrtype;
	u32			pcst_u32_1;
	u32			pcst_u32_2;
	u32			pcst_u32_3;
};

#define pcst_nclients	pcst_u32_1
#define pcst_nsent	pcst_u32_2
#define pcst_nrecv	pcst_u32_3

struct psc_ctlmsg_hashtable {
	char			pcht_name[HTNAME_MAX];
	int			pcht_totalbucks;
	int			pcht_usedbucks;
	int			pcht_nents;
	int			pcht_maxbucklen;
};

#define PCHT_NAME_ALL		"all"

#define PCP_FIELD_MAX		32
#define PCP_VALUE_MAX		32

struct psc_ctlmsg_param {
	char			pcp_thrname[PSC_THRNAME_MAX];
	char			pcp_field[PCP_FIELD_MAX];
	char			pcp_value[PCP_VALUE_MAX];
	int			pcp_flags;
};

#define PCPF_ADD	(1 << 0)
#define PCPF_SUB	(1 << 1)

struct psc_ctlmsg_iostats {
	struct iostats		pci_ist;
};

#define PCI_NAME_ALL		"all"

struct psc_ctlmsg_meter {
	struct psc_meter	pcm_mtr;
};

#define PCM_NAME_ALL		"all"

struct psc_ctlmsg_pool {
	char			pcpm_name[LC_NAME_MAX];
	int			pcpm_min;
	int			pcpm_max;
	int			pcpm_total;
	int			pcpm_flags;
};

#define PCPM_NAME_ALL		"all"

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
#define NPCMT			11

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

#endif /* __PFL_CTL_H__ */
