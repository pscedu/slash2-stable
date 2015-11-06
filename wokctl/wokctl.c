/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright 2015, Pittsburgh Supercomputing Center
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

#include <stdio.h>

#include "pfl/ctl.h"
#include "pfl/ctlcli.h"

int			 verbose;
int			 recursive;

extern const char	*__progname;
const char		*daemon_name = "mount_wokfs";

int
lookup(const char **tbl, int n, const char *name)
{
	int i;

	for (i = 0; i < n; i++)
		if (strcasecmp(name, tbl[i]) == 0)
			return (i);
	return (-1);
}

#define MAX_MODULES 128

void
cmd_insert(int ac, char **av)
{
	struct wokctlmsg_modspec *wcms;
	long l;

	if (ac != 3)
		errx(1, "usage: %s insert position path",
		    __progname);
	l = strtol(av[0], &endp, 10);
	if (l < 0 || l > MAX_MODULES ||
	    endp == av[0] || *endp)
		errx(1, "position: invalid value: %s", av[0]);
	wcms = psc_ctlmsg_push(WOKCMT_INSERT, sizeof(*wcms));
	strlcpy(wcms->wcms_path, av[1], sizeof(wcms->wcms_path));
	wcms->wcms_pos = l;
}

void
cmd_remove(int ac, char **av)
{
	struct wokctlmsg_modcmd *wcmc;
	char *endp;
	long l;

	if (ac != 1)
		errx(1, "usage: %s remove position", __progname);
	l = strtol(av[0], &endp, 10);
	if (l < 0 || l > MAX_MODULES ||
	    endp == av[0] || *endp)
		errx(1, "position: invalid value: %s", av[0]);
	wcmc = psc_ctlmsg_push(WOKCMT_REMOVE, sizeof(*wcmc));
	wcmc->wcmc_pos = l;
}

void
cmd_reload(int ac, char **av)
{
	struct wokctlmsg_modcmd *wcre;
	char *endp;
	long l;

	if (ac != 1)
		errx(1, "usage: %s reload position", __progname);
	if (l < 0 || l > MAX_MODULES ||
	    endp == av[0] || *endp)
		errx(1, "position: invalid value: %s", av[0]);
	wcmc = psc_ctlmsg_push(WOKCMT_RELOAD, sizeof(*wcmc));
	wcmc->wcmc_pos = l;
}

void
cmd_list(int ac, char **av)
{
	if (ac)
		errx(1, "usage: %s list", __progname);
	psc_ctlmsg_push(WOKCMT_LIST, sizeof(struct wokctl_modspec));
}

struct psc_ctlshow_ent psc_ctlshow_tab[] = {
	PSC_CTLSHOW_DEFS
};

struct psc_ctlmsg_prfmt psc_ctlmsg_prfmts[] = {
	PSC_CTLMSG_PRFMT_DEFS
};

psc_ctl_prthr_t psc_ctl_prthrs[] = {
/* FS		*/ NULL,
/* FSMGR	*/ NULL,
/* CTL		*/ psc_ctlthr_pr,
/* CTLAC	*/ psc_ctlacthr_pr,
/* OPSTIMER	*/ NULL,
/* WORKER	*/ NULL
};

struct psc_ctlcmd_req psc_ctlcmd_reqs[] = {
	{ "insert",		cmd_insert },
	{ "reload",		cmd_reload },
	{ "remove",		cmd_remove },
};

PFLCTL_CLI_DEFS;

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-HInv] [-p paramspec] [-S socket] [-s value] [cmd arg ...]\n",
	    __progname);
	exit(1);
}

struct psc_ctlopt opts[] = {
	{ 'H', PCOF_FLAG, &psc_ctl_noheader },
	{ 'I', PCOF_FLAG, &psc_ctl_inhuman },
	{ 'n', PCOF_FLAG, &psc_ctl_nodns },
	{ 'p', PCOF_FUNC, psc_ctlparse_param },
	{ 's', PCOF_FUNC, psc_ctlparse_show },
	{ 'v', PCOF_FLAG, &verbose }
};

int
main(int argc, char *argv[])
{
	pfl_init();
	psc_ctlcli_main(PATH_CTLSOCK, argc, argv, opts, nitems(opts));
	exit(0);
}
