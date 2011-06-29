/* $Id$ */
/* %PSC_COPYRIGHT% */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlcli.h"
#include "psc_util/fmt.h"
#include "psc_util/log.h"

#include "sliod/ctl_iod.h"
#include "ctl.h"
#include "ctlcli.h"
#include "pathnames.h"

struct psc_ctlshow_ent psc_ctlshow_tab[] = {
	PSC_CTLSHOW_DEFS
};

struct psc_ctlmsg_prfmt psc_ctlmsg_prfmts[] = {
	PSC_CTLMSG_PRFMT_DEFS
};

psc_ctl_prthr_t psc_ctl_prthrs[] = {
/* CTL		*/ psc_ctlthr_pr,
/* CTLAC	*/ psc_ctlacthr_pr,
/* LNETAC	*/ NULL,
/* TIOS		*/ NULL,
/* USKLNDPL	*/ NULL
};

struct psc_ctlcmd_req psc_ctlcmd_reqs[] = {
};

PFLCTL_CLI_DEFS;

const char	*progname;
const char	*daemon_name = "lnrtd";

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-HIn] [-p paramspec] [-S socket] [-s value] [cmd arg ...]\n",
	    progname);
	exit(1);
}

struct psc_ctlopt opts[] = {
	{ 'H', PCOF_FLAG, &psc_ctl_noheader },
	{ 'I', PCOF_FLAG, &psc_ctl_inhuman },
	{ 'i', PCOF_FUNC, psc_ctlparse_iostats },
	{ 'L', PCOF_FUNC, psc_ctlparse_lc },
	{ 'n', PCOF_FLAG, &psc_ctl_nodns },
	{ 'P', PCOF_FUNC, psc_ctlparse_pool },
	{ 'p', PCOF_FUNC, psc_ctlparse_param },
	{ 'R', PCOF_FUNC, &recursive },
	{ 's', PCOF_FUNC, psc_ctlparse_show },
	{ 'v', PCOF_FUNC, &verbose }
};

int
main(int argc, char *argv[])
{
	pfl_init();
	progname = argv[0];
	psc_ctlcli_main(SL_PATH_SLICTLSOCK, argc, argv, opts,
	    nitems(opts));
	exit(0);
}
