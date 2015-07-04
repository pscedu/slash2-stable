/* $Id: main.c 25363 2015-02-04 20:10:24Z zhihui $ */
/* %PSC_COPYRIGHT% */

#include <dlfcn.h>
#include <stdlib.h>

#include "pfl/cdefs.h"
#include "pfl/alloc.h"
#include "pfl/fs.h"
#include "pfl/fsmod.h"
#include "pfl/iostats.h"
#include "pfl/thread.h"
#include "pfl/timerthr.h"

#include "mount_wokfs.h"

#ifdef HAVE_FUSE_BIG_WRITES
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728,big_writes"
#else
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728"
#endif

struct pfl_iostats_grad		 wok_iosyscall_iostats[8];

const char			*progname;
const char			*ctlsockfn = PATH_CTLSOCK;
char				 mountpoint[PATH_MAX];

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
	if ((size_t)rc >= sizeof(buf))
		psc_fatalx("snprintf: umount %s: too long", mp);
	if (system(buf) == -1)
		psclog_warn("system(%s)", buf);
}

struct pscfs wok_pscfs = {
	PSCFS_INIT,
	"wokfs",
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
opt_lookup(const char *opt)
{
	struct {
		const char	*name;
		int		*var;
	} *io, opts[] = {
		{ NULL,		NULL }
	};

	for (io = opts; io->name; io++)
		if (strcmp(opt, io->name) == 0) {
			*io->var = 1;
			return (1);
		}
	return (0);
}

int
mod_load(const char *fn)
{
	void (*loadf)(struct pscfs *);
	struct pscfs *ops;
	void *h;

	h = dlopen(fn, RTLD_NOW);
	if (h == NULL)
		PFL_GOTOERR(error, 0);

	loadf = dlsym(h, "pscfs_module_load");
	if (h == NULL)
		PFL_GOTOERR(error, 0);

	ops = PSCALLOC(sizeof(*ops));
	loadf(ops);

	psc_dynarray_add(&pscfs_modules, ops);
	return (0);

 error:
	if (h)
		dlclose(h);
	psclog_errorx("unable to load module %s", dlerror());
	return (-1);
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

	pscthr_init(THRT_FSMGR, NULL, NULL, 0, "fsmgrthr");

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

	ctlthr_spawn();

	pfl_opstimerthr_spawn(THRT_OPSTIMER, "opstimerthr");

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

	mod_load("/home/yanovich/code/advsys/p/wokfs/passfs/passfs.so");

	psc_dynarray_add(&pscfs_modules, &wok_pscfs);

	exit(pscfs_main(0));
}
