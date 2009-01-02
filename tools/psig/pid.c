/* $Id$ */

#include <sys/param.h>
#include <sys/mount.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "pathnames.h"
#include "putils.h"

#define PID_MAX INT_MAX

static int readpid(char *, pid_t *);

int
parsepid(char *s, pid_t *pid)
{
	int isnum;
	char *p;
	long l;

	isnum = 1;
	for (p = s; *p != '\0'; p++)
		if (!isdigit(*p)) {
			isnum = 0;
			break;
		}
	if (p == s)
		goto noent;
	if (isnum) {
		l = strtoul(s, NULL, 10);
		if (l <= PID_MAX) {
			*pid = (pid_t)l;
			return (1);
		} else {
			errno = ESRCH;
			return (0);
		}
	}

	return (readpid(s, pid));
}

char *
getpidpath(char *s, pid_t *pid, int flags)
{
	char *p, fil[MAXPATHLEN];
	struct statfs fst;
	int isnum;
	long l;

	/* shutup gcc */
	l = 0;

	isnum = 1;
	for (p = s; *p != '\0'; p++)
		if (!isdigit(*p)) {
			isnum = 0;
			break;
		}
	if (p == s)
		goto noent;
	if (isnum) {
		if ((l = strtoul(s, NULL, 10)) > PID_MAX) {
			errno = ESRCH;
			return (NULL);
		}
		snprintf(fil, sizeof(fil), "%s/%s", _PATH_PROC, s);
		s = fil;
	}
	if (statfs(s, &fst) == -1) {
#if 0
		if (errno != ENOENT)
			warn("%s", s);
#endif
		return (NULL);
	}
	if (strcmp(fst.f_fstypename, MOUNT_PROCFS) != 0) {
		/* If we built the path, /proc is not mounted. */
		if (isnum && (flags & P_NODIE) == 0)
			errx(EX_UNAVAILABLE, "/proc: not mounted");
		goto noent;
	}
	/* Success; copy the pid. */
	if (isnum) {
		*pid = (pid_t)l;
		if ((p = strdup(s)) == NULL)
			err(EX_OSERR, "strdup");
		return (p);
	} else {
		/* Else, open and read the pid. */
		if (readpid(s, pid)) {
			if ((p = strdup(s)) == NULL)
				err(EX_OSERR, "strdup");
			return (p);
		} else
			return (NULL);
	}

noent:
	errno = ENOENT;
	return (NULL);
}

static int
readpid(char *s, pid_t *pid)
{
	char fil[MAXPATHLEN];
	FILE *fp;
	long l;
	int ch;

	(void)snprintf(fil, sizeof(fil), "%s%s", s, _RELPATH_STATUS);
	if ((fp = fopen(fil, "r")) == NULL)
		return (0);
	while (!isdigit(ch = fgetc(fp)) && ch != EOF)
		;
	for (l = 0; isdigit(ch); ch = fgetc(fp))
		l = l * 10 + (ch - '0');
	(void)fclose(fp);

	if (l <= PID_MAX && l >= 0) {
		*pid = (pid_t)l;
		return (1);
	}
	errx(1, "invalid pid");
}
