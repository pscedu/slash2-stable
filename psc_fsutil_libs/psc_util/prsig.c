/* $Id$ */

#include <sys/param.h>

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>

#include "psc_util/log.h"
#include "psc_util/prsig.h"
#include "psc_util/strlcat.h"

const char *signames[] = {
	"<zero>",
/*  1 */ "HUP",
/*  2 */ "INT",
/*  3 */ "QUIT",
/*  4 */ "ILL",
/*  5 */ "TRAP",
/*  6 */ "ABRT",
/*  6 */ "IOT",
/*  7 */ "BUS",
/*  8 */ "FPE",
/*  9 */ "KILL",
/* 10 */ "USR1",
/* 11 */ "SEGV",
/* 12 */ "USR2",
/* 13 */ "PIPE",
/* 14 */ "ALRM",
/* 15 */ "TERM",
/* 16 */ "STKFLT",
/* 17 */ "CHLD",
/* 18 */ "CONT",
/* 19 */ "STOP",
/* 20 */ "TSTP",
/* 21 */ "TTIN",
/* 22 */ "TTOU",
/* 23 */ "URG",
/* 24 */ "XCPU",
/* 25 */ "XFSZ",
/* 26 */ "VTALRM",
/* 27 */ "PROF",
/* 28 */ "WINCH",
/* 29 */ "IO",
/* 30 */ "PWR",
/* 31 */ "SYS"
};

void
psc_sigappend(char buf[LINE_MAX], const char *str)
{
	if (buf[0] != '\0')
		psc_strlcat(buf, ",", sizeof(buf));
	psc_strlcat(buf, str, sizeof(buf));
}

void
psc_prsig(void)
{
	struct sigaction sa;
	char buf[LINE_MAX];
	uint64_t mask;
	int i, j;

	for (i = 1; i < NSIG; i++) {
		if (sigaction(i, &sa, NULL) == -1)
			psc_fatal("sigaction");

		buf[0] = '\0';
		if (sa.sa_handler == SIG_DFL)
			psc_sigappend(buf, "default");
		else if (sa.sa_handler == SIG_IGN)
			psc_sigappend(buf, "ignored");
		else if (sa.sa_handler == NULL)
			psc_sigappend(buf, "zero?");
		else
			psc_sigappend(buf, "caught");

		mask = 0;
		for (j = 1; j < NSIG; j++)
			if (sigismember(&sa.sa_mask, j))
				mask |= 1 << (j - 1);
		printf("%s\t\t%016"PRIx64"\t%s\n", signames[i], mask, buf);
	}
}
