/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "psc_rpc/rpc.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/strlcpy.h"

#include "slconfig.h"

char *progname;

int
psc_usklndthr_get_type(__unusedx const char *namefmt)
{
	return (0);
}

void
psc_usklndthr_get_namev(char buf[PSC_THRNAME_MAX], __unusedx const char *namefmt,
    __unusedx va_list ap)
{
	strlcpy(buf, "test", PSC_THRNAME_MAX);
}

struct resource_profile *
slcfg_new_res(void)
{
	struct resource_profile *res;

	res = PSCALLOC(sizeof(*res));
	INIT_RES(res);
	return (res);
}

struct resource_member *
slcfg_new_resm(void)
{
	struct resource_member *resm;

	resm = PSCALLOC(sizeof(*resm));
	return (resm);
}

struct site_profile *
slcfg_new_site(void)
{
	struct site_profile *site;

	site = PSCALLOC(sizeof(*site));
	INIT_SITE(site);
	return (site);
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-S] [-l level] [-i file]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *fn = "../../slashd/config/example.conf";
	int c;

	setenv("LNET_NETWORKS", "tcp10(lo)", 1);

	progname = argv[0];
	pfl_init();
	psc_log_setlevel(0, PLL_NOTICE);
	while (((c = getopt(argc, argv, "c:")) != -1))
		switch (c) {
		case 'c':
			fn = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	slashGetConfig(fn);
	libsl_init(PSC_CLIENT);
	exit(0);
}
