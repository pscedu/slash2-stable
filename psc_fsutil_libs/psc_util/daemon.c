/* $Id$ */

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

/* a daemon startup routine, by Richard Stevens
 * sever all ties to the launching process
 */

void daemon_start(void){
	/* Ignore the terminal stop signals */
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif

	/* If we were not started in the background, fork and
	 * let the parent exit.  This also guarantees the first child
	 * is not a process group leader. */
	pid_t childpid = 0;
	if ( (childpid = fork()) < 0){
		/* using err() instead of psc_log
		 * because logging may not be initialized at this point */
		err(errno, "Can't fork first child");
	} else if (childpid > 0)
		exit(0); /* parent exits */


	/* Disassociate from controlling terminal and process group. */
	/* Ensure the process can't reacquire a new controlling terminal. */
	int fd = 0;

#ifdef	SIGTSTP	/* BSD */

	if ( (fd = open("/dev/tty", O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY, (char *)NULL); /* lose controlling tty */
		close(fd);
	}

	if (0!=setpgid(getpid(), getpid())){
		err(errno, "Can't change process group");
	}

#else /* System V */

	if (setpgrp() == -1){
		err(errno, "Can't change process group");
	}

	signal(SIGHUP, SIG_IGN); /* immune from pgrp leader death */

	if ( (childpid = fork()) < 0){
		err(errno, "Can't fork second child");
	} else if (childpid > 0)
		exit(0); /* first child dies */

	/* begin second child... */
#endif


	/* Move to a "safe" directory   */
	if (0!=chdir("/tmp")){
		err(errno, "Failed to CD to /tmp");
	}

	/* Clear any inherited file mode creation mask. */
	umask(0);

	/* Close any open files descriptors. */
	for (fd = 0; fd < NOFILE; fd++)
		close(fd);
	errno = 0; /* probably got set to EBADF from a close */
}

void write_pidfile(const char *dname, int id){
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
