/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "psc_util/daemon.h"

void write_pidfile(const char *dname, int id)
{
	char  pidfile_name[MAXPATHLEN], pidStr[10];
	FILE *fp;
	int   fd;

	/* look for an existing PID file, and read the PID from it */
	if (0!=id)
		snprintf(pidfile_name, MAXPATHLEN, "%s/%s-%d.pid",
			 PIDFILE_DIR, dname, id);
	else
		snprintf(pidfile_name, MAXPATHLEN, "%s/%s.pid",
			 PIDFILE_DIR, dname);

	if (NULL==(fp=fopen(pidfile_name, "w+")))
		err(errno, "failed to open pid file (%s)", pidfile_name);

	/* lock it, to avoid race conditions */
	if (0>(fd = fileno(fp)))
		err(errno, "failed to acquire FD for pid file (%s)",
		    pidfile_name);
	if (0!=lockf(fd, F_LOCK, 0))
		err(errno, "failed to acquire lock on PID file (%s)",
		    pidfile_name);

	if (1==fscanf(fp, "%s", pidStr)){
#ifdef LINUX
		/* open the file entry in /proc for this PID
		 * to see if it's still running */
		char procfile_name[MAXPATHLEN];
		snprintf(procfile_name, MAXPATHLEN, "/proc/%s", pidStr);
		if (0<=open(procfile_name, O_RDONLY))
			errx(1, "There is already an active %s daemon on this node", dname);
		/* logically... no need to close this */
#endif /* LINUX */
	}

	/* get this PID, and write it to the file */
	snprintf(pidStr, 10, "%d", getpid());
	if (0!=fseek(fp, 0, SEEK_SET))
		err(errno, "seek failed on pid file (%s)", pidfile_name);
	if ((int)strlen(pidStr)+1!=fprintf(fp, "%s\n", pidStr))
		err(errno, "failed to write pid file (%s)", pidfile_name);
	if (0!=fseek(fp, 0, SEEK_SET))
		err(errno, "seek failed on pid file (%s)", pidfile_name);
	if (0!=lockf(fd, F_ULOCK, 0))
		err(errno, "failed to release lock on PID file (%s)",
		    pidfile_name);
	fclose(fp);
}
