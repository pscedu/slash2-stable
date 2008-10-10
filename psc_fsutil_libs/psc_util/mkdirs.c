/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * mkdirs - simple recursive "mkdir -p" type functionality.
 * @dir: path to be created
 * @mode: dir creation mode
 */
int
mkdirs(const char *s, mode_t mode)
{
	char *p, *path;
	int rc;

	rc = -1;

	/* XXX realpath() must be used in here */

	/* Sanity check(s) */
	if (strlen(s) == 0) {
		errno = ENOENT;
		return (-1);
	}

	if (s[0] != '/') {
		errno = EINVAL;
		return (-1);
	}

	if (strcmp(s, "/") == 0)
		return (0);

	path = strdup(s);
	if (path == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	for (p = path; p != NULL; ) {
		*p++ = '/';
		if ((p = strchr(p, '/')) != NULL)
			*p = '\0';
		if (mkdir(path, mode) == -1 &&
		    errno != EEXIST)
			goto done;
	}
	rc = 0;
 done:
	free(path);
	return (rc);
}
