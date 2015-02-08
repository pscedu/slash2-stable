/* $Id: main.c 25363 2015-02-04 20:10:24Z zhihui $ */
/* %PSC_COPYRIGHT% */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gcrypt.h>

#include "pfl/cdefs.h"

GCRY_THREAD_OPTION_PTHREAD_IMPL;

#ifdef HAVE_FUSE_BIG_WRITES
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728,big_writes"
#else
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728"
#endif

const char			*progname;
const char			*ctlsockfn = PATH_MSCTLSOCK;
char				 mountpoint[PATH_MAX];

__static void
msfsthr_teardown(void *arg)
{
	struct msfs_thread *mft = arg;

	spinlock(&msfsthr_uniqidmap_lock);
	psc_vbitmap_unset(&msfsthr_uniqidmap, mft->mft_uniqid);
	psc_vbitmap_setnextpos(&msfsthr_uniqidmap, 0);
	freelock(&msfsthr_uniqidmap_lock);
}

__static void
msfsthr_ensure(struct pscfs_req *pfr)
{
	struct msfs_thread *mft;
	struct psc_thread *thr;
	size_t id;

	thr = pscthr_get_canfail();
	if (thr == NULL) {
		spinlock(&msfsthr_uniqidmap_lock);
		if (psc_vbitmap_next(&msfsthr_uniqidmap, &id) != 1)
			psc_fatal("psc_vbitmap_next");
		freelock(&msfsthr_uniqidmap_lock);

		thr = pscthr_init(MSTHRT_FS, NULL, msfsthr_teardown,
		    sizeof(*mft), "msfsthr%02zu", id);
		mft = thr->pscthr_private;
		psc_multiwait_init(&mft->mft_mw, "%s",
		    thr->pscthr_name);
		mft->mft_uniqid = id;
		pscthr_setready(thr);
	}
	psc_assert(thr->pscthr_type == MSTHRT_FS);

	mft = thr->pscthr_private;
	mft->mft_pfr = pfr;
}

void
wokfsop_destroy(__unusedx struct pscfs_req *pfr)
{
	pscthr_killall();
}

void
unmount(const char *mp)
{
	char buf[BUFSIZ];
	int rc;

	/* XXX do not let this hang */
	rc = snprintf(buf, sizeof(buf),
	    "umount '%s' || umount -f '%s' || umount -l '%s'",
	    mp, mp, mp);
	if (rc == -1)
		psc_fatal("snprintf: umount %s", mp);
	if (rc >= (int)sizeof(buf))
		psc_fatalx("snprintf: umount %s: too long", mp);
	if (system(buf) == -1)
		psclog_warn("system(%s)", buf);
}

struct pscfs wok_pscfs = {
	NULL,
	NULL,
	NULL,	/* releasedir */
	NULL,
	NULL,
	NULL,
	NULL,	/* fsyncdir */
	NULL,
	NULL,	/* ioctl */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	wokfsop_destroy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-dQUVX] [-f conf] [-I iosystem] [-M mds]\n"
	    "\t[-o mountopt] [-S socket] node\n",
	    progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct pscfs_args args = PSCFS_ARGS_INIT(0, NULL);
	char c, *p, *noncanon_mp;
	int unmount_first = 0;

	progname = argv[0];
	pfl_init();

	pscfs_addarg(&args, "");		/* progname/argv[0] */
	pscfs_addarg(&args, "-o");
	pscfs_addarg(&args, STD_MOUNT_OPTIONS);

	p = getenv("CTL_SOCK_FILE");
	if (p)
		ctlsockfn = p;

	while ((c = getopt(argc, argv, "do:S:U")) != -1)
		switch (c) {
		case 'd':
			pscfs_addarg(&args, "-odebug");
			break;
		case 'o':
			if (!opt_lookup(optarg)) {
				pscfs_addarg(&args, "-o");
				pscfs_addarg(&args, optarg);
			}
			break;
		case 'S':
			ctlsockfn = optarg;
			break;
		case 'U':
			unmount_first = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	pscthr_init(MSTHRT_FSMGR, NULL, NULL, 0, "fsmgrthr");

	noncanon_mp = argv[0];
	if (unmount_first)
		unmount(noncanon_mp);

	/* canonicalize mount path */
	if (realpath(noncanon_mp, mountpoint) == NULL)
		psc_fatal("realpath %s", noncanon_mp);

//	pflog_get_fsctx_uprog = slc_log_get_fsctx_uprog;
//	pflog_get_fsctx_uid = slc_log_get_fsctx_uid;
//	pflog_get_fsctx_pid = slc_log_get_fsctx_pid;

	pscfs_mount(mountpoint, &args);
	pscfs_freeargs(&args);

//	drop_privs(1);

	msctlthr_spawn();

	pfl_opstimerthr_spawn(MSTHRT_OPSTIMER, "opstimerthr");

	wok_iosyscall_iostats[0].size =        1024;
	wok_iosyscall_iostats[1].size =    4 * 1024;
	wok_iosyscall_iostats[2].size =   16 * 1024;
	wok_iosyscall_iostats[3].size =   64 * 1024;
	wok_iosyscall_iostats[4].size =  128 * 1024;
	wok_iosyscall_iostats[5].size =  512 * 1024;
	wok_iosyscall_iostats[6].size = 1024 * 1024;
	wok_iosyscall_iostats[7].size = 0;
	pfl_iostats_grad_init(wok_iosyscall_iostats, OPSTF_BASE10, "iosz");

	pscfs_entry_timeout = 8.;
	pscfs_attr_timeout = 8.;

	psc_dynarray_add(&pscfs_modules, &wok_pscfs);

	exit(pscfs_main(0));
}
