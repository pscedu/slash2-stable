/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>

#include <stdint.h>

#include "pfl/str.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlcli.h"

const char *progname;

const char *daemon_name = "test";
struct psc_ctlshow_ent psc_ctlshow_tab[] = { };
psc_ctl_prthr_t psc_ctl_prthrs[] = { };
struct psc_ctlmsg_prfmt psc_ctlmsg_prfmts[] = { };
struct psc_ctlcmd_req psc_ctlcmd_reqs[] = { };

PFLCTL_CLI_DEFS;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct psc_ctlmsg_meter pcm;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();
	strlcpy(pcm.pcm_mtr.pm_name, "test",
	    sizeof(pcm.pcm_mtr.pm_name));

	pcm.pcm_mtr.pm_cur = 0;
	pcm.pcm_mtr.pm_max = 0;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 0;
	pcm.pcm_mtr.pm_max = 1;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 1;
	pcm.pcm_mtr.pm_max = 1000;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 10;
	pcm.pcm_mtr.pm_max = 1000;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 100;
	pcm.pcm_mtr.pm_max = 1000;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 990;
	pcm.pcm_mtr.pm_max = 1000;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 1000;
	pcm.pcm_mtr.pm_max = 1000;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	pcm.pcm_mtr.pm_cur = 1100;
	pcm.pcm_mtr.pm_max = 1000;
	psc_ctlmsg_meter_prdat(NULL, &pcm);

	exit(0);
}
