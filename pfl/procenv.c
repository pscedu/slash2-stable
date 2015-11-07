/* $Id$ */

#include <sys/param.h>

#include <stdio.h>
#include <string.h>

#include "pfl/str.h"

char *
pfl_getprocenv(int pid, const char *key)
{
	char fn[PATH_MAX], buf[BUFSIZ], *t, *p;
	FILE *fp;
	int c;

	if (snprintf(fn, sizeof(fn), "/proc/%d/environ", pid) == -1)
		return (NULL);

	fp = fopen(fn, "r");
	if (fp == NULL)
		return (NULL);

	t = buf;
	p = NULL;
	while ((c = fgetc(fp)) != EOF) {
		if (t < buf + sizeof(buf)) {
			*t++ = c;
			if (c == '\0' && t > buf + 1) {
				if ((p = strchr(buf, '=')) != NULL) {
					*p++ = '\0';
					if (strcmp(buf, key) == 0)
						break;
					p = NULL;
				}
			}
		}
		if (c == '\0')
			t = buf;
	}
	fclose(fp);
	return (p ? pfl_strdup(p) : NULL);
}
