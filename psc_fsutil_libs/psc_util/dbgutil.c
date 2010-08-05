/* $Id$ */

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>

#include "pfl/fcntl.h"
#include "pfl/stat.h"
#include "pfl/types.h"
#include "psc_util/log.h"

void
pfl_print_flag(const char *str, int *seq)
{
	printf("%s%s", *seq ? "|" : "", str);
	*seq = 1;
}

#define PR_O_FLAG(fl, val, seq)						\
	do {								\
		if ((val) & (fl)) {					\
			pfl_print_flag(#fl, (seq));			\
			(val) &= ~(fl);					\
		}							\
	} while (0)

void
pfl_dump_statbuf(int level, const struct stat *stb)
{
	DEBUG_STATBUF(level, stb, "");
}

void
pfl_dump_fflags(int fflags)
{
	int seq = 0;

	PR_O_FLAG(O_WRONLY, fflags, &seq);
	PR_O_FLAG(O_RDWR, fflags, &seq);
	if ((fflags & O_ACCMODE) == O_RDONLY)
		pfl_print_flag("O_RDONLY", &seq);

	PR_O_FLAG(O_CREAT, fflags, &seq);
	PR_O_FLAG(O_EXCL, fflags, &seq);
	PR_O_FLAG(O_TRUNC, fflags, &seq);
	PR_O_FLAG(O_APPEND, fflags, &seq);
	PR_O_FLAG(O_NONBLOCK, fflags, &seq);
	PR_O_FLAG(O_SYNC, fflags, &seq);
	PR_O_FLAG(O_NOCTTY, fflags, &seq);
	PR_O_FLAG(O_NOFOLLOW, fflags, &seq);

	PR_O_FLAG(O_DSYNC, fflags, &seq);
	PR_O_FLAG(O_RSYNC, fflags, &seq);
	PR_O_FLAG(O_ASYNC, fflags, &seq);
	PR_O_FLAG(O_DIRECTORY, fflags, &seq);
	PR_O_FLAG(O_EXLOCK, fflags, &seq);
	PR_O_FLAG(O_SHLOCK, fflags, &seq);

	PR_O_FLAG(O_DIRECT, fflags, &seq);
	PR_O_FLAG(O_CLOEXEC, fflags, &seq);
	PR_O_FLAG(O_SYMLINK, fflags, &seq);
	PR_O_FLAG(O_NOATIME, fflags, &seq);
	PR_O_FLAG(O_LARGEFILE, fflags, &seq);

	if (fflags) {
		pfl_print_flag("", &seq);
		printf("%x", fflags);
	}
	printf("\n");
}
