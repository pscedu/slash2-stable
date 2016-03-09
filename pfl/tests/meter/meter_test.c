/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2013-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/types.h>

#include <stdint.h>

#include "pfl/str.h"
#include "pfl/ctl.h"
#include "pfl/ctlcli.h"

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

	pcm.pcm_mtr.pm_cur = 10;   psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 20;   psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 100;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 200;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 250;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 275;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 300;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 325;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 350;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 500;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 990;  psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 1000; psc_ctlmsg_meter_prdat(NULL, &pcm);
	pcm.pcm_mtr.pm_cur = 1100; psc_ctlmsg_meter_prdat(NULL, &pcm);

	exit(0);
}
