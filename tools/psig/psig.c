/* $Id$ */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/signal.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "pathnames.h"
#include "putils.h"
#include "symtab.h"

#define PID_MAX	INT_MAX

int		 psig(char *);
void		 prhandler(struct kinfo_proc2 *, struct symtab *, int);
__dead void	 usage(void);

int		 pr_hnd = 0;
kvm_t		*kd = NULL;

int
main(int argc, char *argv[])
{
	char buf[_POSIX2_LINE_MAX];
	int c, status;

	while ((c = getopt(argc, argv, "h")) != -1) {
		switch (c) {
		case 'h':
			pr_hnd = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argv += optind;

	if (*argv == NULL)
		usage();
	if ((kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY,
	    buf)) == NULL)
		errx(EX_OSERR, "kvm_openfiles: %s", buf);
	status = 0;
	while (*argv != NULL)
		status |= psig(*argv++);
	(void)kvm_close(kd);
	exit(status ? EX_UNAVAILABLE : EX_OK);
}

int
psig(char *s)
{
	int pcnt, i, blocked, wrote, hasprocfs;
	char **argv, *p, fil[MAXPATHLEN];
	struct kinfo_proc2 *kp;
	struct sigacts *sa;
	const char *errstr;
	struct symtab *st;
	u_int32_t sig;
	pid_t pid;

	/*
	 * If /proc is not mounted, getpidpath() will fail.
	 * That does not mean that the program has to quit
	 * though; behavior that requires things in
	 * /proc/pid/ can just be ignored.
	 */
	if ((p = getpidpath(s, &pid, P_NODIE)) == NULL) {
		pid = strtonum(s, 0, PID_MAX, &errstr);
		if (errstr != NULL) {
			warnx("%s: %s", s, errstr);
			return (1);
		}
		hasprocfs = 0;
	} else {
		hasprocfs = 1;
		(void)snprintf(fil, sizeof(fil), "%s%s", p, _RELPATH_FILE);
	}
	kp = kvm_getproc2(kd, KERN_PROC_PID, pid, sizeof(*kp), &pcnt);
	if (kp == NULL)
		errx(EX_OSERR, "kvm_getproc2: %s", kvm_geterr(kd));
	else if (pcnt == 0) {
		errno = ESRCH;
		warn("%s", s);
		return (1);
	}
	if ((argv = kvm_getargv2(kd, kp, 0)) == NULL)
		(void)printf("%d:\t%s\n", pid, kp->p_comm);
	else {
		(void)printf("%d:\t", pid);
		for (; *argv != NULL; argv++)
			(void)printf("%s%s", *argv, argv[1] == NULL ?
			    "" : " ");
		(void)printf("\n");
	}
	sa = malloc(sizeof(*sa));
	if (hasprocfs)
		/* Silently ignore failures. */
		st = symtab_open(fil);
	else
		st = NULL;
	for (i = 1; i < NSIG; i++) {
		sig = 1 << (i - 1);
		switch (i) {
		case SIGHUP:	(void)printf("HUP");	break;
		case SIGINT:	(void)printf("INT");	break;
		case SIGQUIT:	(void)printf("QUIT");	break;
		case SIGILL:	(void)printf("ILL");	break;
		case SIGABRT:	(void)printf("ABRT");	break;
		case SIGFPE:	(void)printf("FPE");	break;
		case SIGKILL:	(void)printf("KILL");	break;
		case SIGSEGV:	(void)printf("SEGV");	break;
		case SIGPIPE:	(void)printf("PIPE");	break;
		case SIGALRM:	(void)printf("ARLM");	break;
		case SIGTERM:	(void)printf("TERM");	break;
		case SIGSTOP:	(void)printf("STOP");	break;
		case SIGTSTP:	(void)printf("TSTP");	break;
		case SIGCONT:	(void)printf("CONT");	break;
		case SIGCHLD:	(void)printf("CHLD");	break;
		case SIGTTIN:	(void)printf("TTIN");	break;
		case SIGTTOU:	(void)printf("TTOU");	break;
		case SIGUSR1:	(void)printf("USR1");	break;
		case SIGUSR2:	(void)printf("USR2");	break;
#ifndef _POSIX_SOURCE
		case SIGTRAP:	(void)printf("TRAP");	break;
		case SIGEMT:	(void)printf("EMT");	break;
		case SIGBUS:	(void)printf("BUS");	break;
		case SIGSYS:	(void)printf("SYS");	break;
		case SIGURG:	(void)printf("URG");	break;
		case SIGIO:	(void)printf("IO");	break;
		case SIGXCPU:	(void)printf("XCPU");	break;
		case SIGXFSZ:	(void)printf("XFSZ");	break;
		case SIGVTALRM:	(void)printf("VTARLM");	break;
		case SIGPROF:	(void)printf("PROF");	break;
		case SIGWINCH:	(void)printf("WINCH");	break;
		case SIGINFO:	(void)printf("INFO");	break;
#endif
		default:
			warnx("%u: unsupported signal number", sig);
			goto nextsig;
			/* NOTREACHED */
		}

		(void)printf("\t");
		blocked = 0;
		wrote = 0;
		if (kp->p_sigmask & sig) {
			wrote += printf("blocked");
			blocked = 1;
		}
		if (kp->p_sigignore & sig)
			wrote += printf("%signore", blocked ? "," : "");
		else if (kp->p_sigcatch & sig)
			wrote += printf("%scaught", blocked ? "," : "");
		else
			wrote += printf("%sdefault", blocked ? "," : "");
		if (!(kp->p_sigignore & sig))
			(void)printf("%s\t%d", wrote > 8 ? "" : "\t",
			    kp->p_siglist & sig);

		if (pr_hnd && (kp->p_sigcatch & sig) && st != NULL)
			prhandler(kp, st, i);

		/* XXX: show flags */

		(void)printf("\n");
nextsig:
		;
	}
	free(sa);
	if (st != NULL)
		symtab_close(st);
	return (1);
}

void
prhandler(struct kinfo_proc2 *kp, struct symtab *st, int i)
{
	unsigned long addr;
	struct sigacts sa;
	const char *nam;
	uid_t uid;

	uid = getuid();
	if (uid != 0 && uid != kp->p_uid)
		return;
	if (kvm_read(kd, (u_long)kp->p_sigacts, &sa, sizeof(sa)) !=
	    sizeof(sa))
		return;
	addr = (unsigned long)sa.ps_sigact[i];
	if ((nam = symtab_getsymname(st, addr)) != NULL)
		(void)printf("\t%s()", nam);
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-h] pid ...\n", __progname);
	exit(EX_USAGE);
}
