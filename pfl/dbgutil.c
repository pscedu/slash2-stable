/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>

#include "pfl/cdefs.h"
#include "pfl/fcntl.h"
#include "pfl/stat.h"
#include "pfl/types.h"

void
pfl_print_flag(const char *str, int *seq)
{
	printf("%s%s", *seq ? "|" : "", str);
	*seq = 1;
}

void
pfl_dump_fflags(int fflags)
{
	int seq = 0;

	PFL_PRFLAG(O_WRONLY, &fflags, &seq);
	PFL_PRFLAG(O_RDWR, &fflags, &seq);
	if ((fflags & O_ACCMODE) == O_RDONLY)
		pfl_print_flag("O_RDONLY", &seq);

	PFL_PRFLAG(O_CREAT, &fflags, &seq);
	PFL_PRFLAG(O_EXCL, &fflags, &seq);
	PFL_PRFLAG(O_TRUNC, &fflags, &seq);
	PFL_PRFLAG(O_APPEND, &fflags, &seq);
	PFL_PRFLAG(O_NONBLOCK, &fflags, &seq);
	PFL_PRFLAG(O_SYNC, &fflags, &seq);
	PFL_PRFLAG(O_NOCTTY, &fflags, &seq);
	PFL_PRFLAG(O_NOFOLLOW, &fflags, &seq);

	PFL_PRFLAG(O_DSYNC, &fflags, &seq);
	PFL_PRFLAG(O_RSYNC, &fflags, &seq);
	PFL_PRFLAG(O_ASYNC, &fflags, &seq);
	PFL_PRFLAG(O_DIRECTORY, &fflags, &seq);
	PFL_PRFLAG(O_EXLOCK, &fflags, &seq);
	PFL_PRFLAG(O_SHLOCK, &fflags, &seq);

	PFL_PRFLAG(O_DIRECT, &fflags, &seq);
	PFL_PRFLAG(O_CLOEXEC, &fflags, &seq);
	PFL_PRFLAG(O_SYMLINK, &fflags, &seq);
	PFL_PRFLAG(O_NOATIME, &fflags, &seq);
	PFL_PRFLAG(O_LARGEFILE, &fflags, &seq);

	if (fflags) {
		pfl_print_flag("", &seq);
		printf("%x", fflags);
	}
	printf("\n");
}

void
pfl_dump_mode(mode_t m)
{
	char buf[11];

	pfl_fmt_mode(modes, buf);
	m &= ~(ALLPERMS | S_IFMT);

	printf("%s", buf);
	if (m)
		printf(" unknown bits: %#o\n", m);
	printf("\n");
}

#undef _psclogk
#define _psclogk(ss, lvl, flg, fmt, ...)				\
	do {								\
		fprintf(stderr, fmt, ## __VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	} while (0)

void
pfl_dump_statbuf(const struct stat *stb)
{
	DEBUG_STATBUF(PLL_MAX, stb, "");
}
