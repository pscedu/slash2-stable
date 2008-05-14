/* $Id$ */

/*
 * Control interface for querying and modifying
 * parameters of a running daemon instance.
 */

#include <sys/types.h>

#include "psc_ds/hash.h"
#include "psc_ds/listcache.h"
#include "psc_util/iostats.h"
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

#define PCP_FIELD_MAX		30
#define PCP_VALUE_MAX		50

struct psc_ctlmsg_param {
	char			pcp_thrname[PSC_THRNAME_MAX];
	char			pcp_field[PCP_FIELD_MAX];
	char			pcp_value[PCP_VALUE_MAX];
};

struct psc_ctlmsg_iostats {
	struct iostats		pci_ist;
};

#define PCI_NAME_ALL		"all"

/* Control message types. */
#define PCMT_ERRMSG		0
#define PCMT_GETLOGLEVEL	1
#define PCMT_GETLC		2
#define PCMT_GETSTATS		3
#define PCMT_GETSUBSYS		4
#define PCMT_GETHASHTABLE	5
#define PCMT_GETPARAM		6
#define PCMT_SETPARAM		7
#define PCMT_GETIOSTATS		8
#define NPCMT			9

#define PSC_CTLDEFOPS								\
/* 0 */	{ NULL,	0 },								\
/* 1 */	{ psc_ctlrep_getloglevel,	sizeof(struct psc_ctlmsg_loglevel) },	\
/* 2 */	{ psc_ctlrep_getlc,		sizeof(struct psc_ctlmsg_lc) },		\
/* 3 */	{ psc_ctlrep_getstats,		sizeof(struct psc_ctlmsg_stats) },	\
/* 4 */	{ psc_ctlrep_getsubsys,		0 },					\
/* 5 */	{ psc_ctlrep_gethashtable,	sizeof(struct psc_ctlmsg_hashtable) },	\
/* 6 */	{ psc_ctlrep_getparam,		sizeof(struct psc_ctlmsg_param) },	\
/* 7 */	{ psc_ctlrep_setparam,		sizeof(struct psc_ctlmsg_param) },	\
/* 8 */	{ psc_ctlrep_getiostats,	sizeof(struct psc_ctlmsg_iostats) }

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

struct psc_ctlthr {
	int pc_st_nclients;
	int pc_st_nsent;
	int pc_st_nrecv;
};

struct psc_ctlops {
	void	(*pc_op)(int, struct psc_ctlmsghdr *, void *);
	size_t	  pc_siz;
};

#define PSC_CTL_FOREACH_THREAD(i, thr, thrname, threads, nthreads)	\
	for ((i) = 0; ((thr) = (threads)[i]) && (i) < (nthreads); i++)	\
		if (strncmp((thr)->pscthr_name, (thrname),		\
		    strlen(thrname)) == 0 ||				\
		    strcmp((thrname), PCTHRNAME_EVERYONE) == 0)

#define psc_ctlthr(thr)	((struct psc_ctlthr *)(thr)->pscthr_private)

void psc_ctlthr_main(const char *, const struct psc_ctlops *, int);
void psc_ctl_applythrop(int, struct psc_ctlmsghdr *, void *, const char *,
	void (*)(int, struct psc_ctlmsghdr *, void *, struct psc_thread *));
