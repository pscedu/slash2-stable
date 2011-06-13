/* $Id$ */

#include <stdio.h>
#include <unistd.h>

#include "psc_rpc/rpc.h"

#include "lnet/lnet.h"

const char *progname;

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
