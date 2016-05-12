/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright 2015-2016, Pittsburgh Supercomputing Center
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
#include <string.h>

#include "pfl/ctl.h"
#include "pfl/ctlcli.h"
#include "pfl/str.h"

#include "mount_wokfs/ctl.h"

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
	char *endp;
	long l;

	if (ac != 3 && ac != 4) {
		fprintf(stderr, "usage: %s insert position path "
		    "[option,...]\n",
		    __progname);
		exit(1);
	}
	l = strtol(av[1], &endp, 10);
	if (l < 0 || l > MAX_MODULES ||
	    endp == av[1] || *endp)
		errx(1, "position: invalid value: %s", av[1]);
	wcms = psc_ctlmsg_push(WOKCMT_INSERT, sizeof(*wcms));
	if (realpath(av[2], wcms->wcms_path) == NULL)
		err(1, "path %s", av[2]);
	wcms->wcms_pos = l;

	if (ac == 4 && strlcpy(wcms->wcms_opts, av[3],
	    sizeof(wcms->wcms_opts)) >= sizeof(wcms->wcms_opts))
		err(1, "option list too long: %s", av[3]);
}

void
cmd_remove(int ac, char **av)
{
	struct wokctlmsg_modctl *wcmc;
	char *endp;
	long l;

	if (ac != 2) {
		fprintf(stderr, "usage: %s remove position\n",
		    __progname);
		exit(1);
	}
	l = strtol(av[1], &endp, 10);
	if (l < 0 || l > MAX_MODULES ||
	    endp == av[1] || *endp)
		errx(1, "position: invalid value: %s", av[1]);
	wcmc = psc_ctlmsg_push(WOKCMT_REMOVE, sizeof(*wcmc));
	wcmc->wcmc_pos = l;
}

void
cmd_reload(int ac, char **av)
{
	struct wokctlmsg_modctl *wcmc;
	char *endp;
	long l;

	if (ac != 2) {
		fprintf(stderr, "usage: %s reload position\n",
		    __progname);
		exit(1);
	}
	l = strtol(av[1], &endp, 10);
	if (l < 0 || l > MAX_MODULES ||
	    endp == av[1] || *endp)
		errx(1, "position: invalid value: %s", av[1]);
	wcmc = psc_ctlmsg_push(WOKCMT_RELOAD, sizeof(*wcmc));
	wcmc->wcmc_pos = l;
}

void
cmd_list(int ac, __unusedx char **av)
{
	if (ac != 1) {
		fprintf(stderr, "usage: %s list\n", __progname);
		exit(1);
	}
	psc_ctlmsg_push(WOKCMT_LIST, sizeof(struct wokctlmsg_modspec));
}

int
wok_list_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%3s %s\n",
	    "pos", "module");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
wok_list_prdat(__unusedx const struct psc_ctlmsghdr *mh, const void *m)
{
	const struct wokctlmsg_modspec *wcms = m;

	printf("%3d %s\n    %s\n", wcms->wcms_pos, wcms->wcms_path,
	    wcms->wcms_opts);
}

struct psc_ctlshow_ent psc_ctlshow_tab[] = {
	PSC_CTLSHOW_DEFS
};

struct psc_ctlmsg_prfmt psc_ctlmsg_prfmts[] = {
	PSC_CTLMSG_PRFMT_DEFS,
	{ NULL,			NULL,		0,					NULL },
	{ wok_list_prhdr,	wok_list_prdat,	sizeof(struct wokctlmsg_modspec),	NULL },
	{ NULL,			NULL,		0,					NULL },
	{ NULL,			NULL,		0,					NULL },
};

struct psc_ctlcmd_req psc_ctlcmd_reqs[] = {
	{ "insert",		cmd_insert },
	{ "list",		cmd_list },
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
