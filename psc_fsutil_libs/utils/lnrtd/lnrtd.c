/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pfl/str.h"
#include "psc_rpc/rpc.h"

#include "lnet/lnet.h"

const char *progname;

enum {
	LRTHRT_LNETAC,
	LRTHRT_USKLNDPL,
};

int
psc_usklndthr_get_type(const char *namefmt)
{
	if (strstr(namefmt, "lnacthr"))
		return (LRTHRT_LNETAC);
	return (LRTHRT_USKLNDPL);
}

void
psc_usklndthr_get_namev(char buf[PSC_THRNAME_MAX], const char *namefmt,
    va_list ap)
{
	size_t n;

	n = strlcpy(buf, "ln", PSC_THRNAME_MAX);
	if (n < PSC_THRNAME_MAX)
		vsnprintf(buf + n, PSC_THRNAME_MAX - n, namefmt, ap);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int rc;

	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	rc = LNetInit();
	if (rc)
		psc_fatalx("failed to initialize lnet rc=%d", rc);
	lnet_server_mode();
	rc = LNetNIInit(PSCRPC_SVR_PID);
	if (rc)
		psc_fatalx("LNetNIInit failed rc=%d", rc);
	for (;;)
		sleep(60);
}
