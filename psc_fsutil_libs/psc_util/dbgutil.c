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

#define PR_O_FLAG(fl, val, seq)						\
	do {								\
		if ((val) & (fl)) {					\
			pfl_print_flag(#fl, (seq));			\
			(val) &= ~(fl);					\
		}							\
	} while (0)

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

void
pfl_dump_mode(mode_t modes)
{
	char ch, buf[10];
	uint32_t m = modes;

	if (S_ISDIR(m))
		ch = 'd';
	else if (S_ISCHR(m))
		ch = 'c';
	else if (S_ISBLK(m))
		ch = 'b';
	else if (S_ISSOCK(m))
		ch = 's';
	else if (S_ISDIR(m))
		ch = 'd';
	else if (S_ISLNK(m))
		ch = 'l';
	else if (S_ISFIFO(m))
		ch = 'f';
	else if (S_ISREG(m))
		ch = '-';
	else
		ch = '?';

	buf[0] = m & S_IRUSR ? 'r' : '-';
	buf[1] = m & S_IWUSR ? 'w' : '-';
	if (m & S_ISUID)
		buf[2] = m & S_IXUSR ? 's' : 'S';
	else
		buf[2] = m & S_IXUSR ? 'x' : '-';

	buf[3] = m & S_IRGRP ? 'r' : '-';
	buf[4] = m & S_IWGRP ? 'w' : '-';
	if (m & S_ISGID)
		buf[5] = m & S_IXGRP ? 's' : 'S';
	else
		buf[5] = m & S_IXGRP ? 'x' : '-';

	buf[6] = m & S_IROTH ? 'r' : '-';
	buf[7] = m & S_IWOTH ? 'w' : '-';
	if (m & S_ISVTX)
		buf[8] = m & S_IXOTH ? 't' : 'T';
	else
		buf[8] = m & S_IXOTH ? 'x' : '-';
	buf[9] = '\0';
	m &= ~(ALLPERMS | S_IFMT);

	printf("%c%s", ch, buf);
	if (m)
		printf(" unknown bits: %#o\n", m);
	printf("\n");
}

__weak int
psc_log_getlevel(__unusedx int subsys)
{
	return (0);
}

__weak void
_psclog(__unusedx const char *fn, __unusedx const char *func,
    __unusedx int line, __unusedx int subsys, __unusedx int level,
    __unusedx int options, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	printf("\n");
}

void
pfl_dump_statbuf(const struct stat *stb)
{
	DEBUG_STATBUF(PLL_MAX, stb, "");
}
