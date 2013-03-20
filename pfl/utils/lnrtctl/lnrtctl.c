/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011-2013, Pittsburgh Supercomputing Center (PSC).
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

#include "../lnrtd/lnrtd.h"
#include "ctl.h"
#include "ctlcli.h"

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
	{ 'n', PCOF_FLAG, &psc_ctl_nodns },
	{ 'p', PCOF_FUNC, psc_ctlparse_param },
	{ 's', PCOF_FUNC, psc_ctlparse_show },
};

int
main(int argc, char *argv[])
{
	pfl_init();
	progname = argv[0];
	psc_ctlcli_main(PATH_CTLSOCK, argc, argv, opts,
	    nitems(opts));
	exit(0);
}
