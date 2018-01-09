/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2015, Pittsburgh Supercomputing Center
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <dlfcn.h>
#include <stdlib.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/fs.h"
#include "pfl/fsmod.h"
#include "pfl/opstats.h"
#include "pfl/str.h"
#include "pfl/sys.h"
#include "pfl/thread.h"
#include "pfl/timerthr.h"
#include "pfl/workthr.h"

#include "mount_wokfs.h"
#include "ctl.h"

#ifdef HAVE_FUSE_BIG_WRITES
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728,big_writes"
#else
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728"
#endif

const char			*ctlsockfn = PATH_CTLSOCK;
char				 mountpoint[PATH_MAX];

/*
 * Killing the FUSE userland process leaves the mount point open.
 * So we have to do an explicit unmount here.
 */
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

struct wok_module *
mod_load(const char *path, const char *opts, char *errbuf,
    size_t errlen)
{
	int (*loadf)(struct pscfs *);
	struct wok_module *wm;
	void *h;
	int rc;

	h = dlopen(path, RTLD_NOW);
	if (h == NULL) {
		snprintf(errbuf, LINE_MAX, "%s\n", dlerror()); 
		fprintf(stderr, "%s\n", errbuf);
		return (NULL);
	}

	loadf = dlsym(h, "pscfs_module_load");
	if (loadf == NULL) {
		dlclose(h);
		snprintf(errbuf, LINE_MAX,
		    "symbol pscfs_module_load undefined.\n");
		fprintf(stderr, "%s",
		    "symbol pscfs_module_load undefined.\n");
		return (NULL);
	}

	wm = PSCALLOC(sizeof(*wm));
	wm->wm_path = pfl_strdup(path);
	wm->wm_handle = h;
	wm->wm_opts = pfl_strdup(opts);
	wm->wm_module.pf_private = wm;
	pflfs_module_init(&wm->wm_module, opts);
	rc = loadf(&wm->wm_module);

	/*
	 * XXX XXX XXX
	 * This is a complete hack but this flush somehow avoids a bunch
	 * of zeroes from ending up in the log...
	 * XXX XXX XXX
	 */
	fflush(stderr);

	if (rc) {
		wm->wm_module.pf_handle_destroy = NULL;
		pflfs_module_destroy(&wm->wm_module);

		dlclose(h);
		PSCFREE(wm->wm_path);
		PSCFREE(wm);
		psclog_warnx("module failed to load: rc=%d module=%s",
		    rc, path);
		strlcpy(errbuf, strerror(rc), errlen);
		return (NULL);
	}
	return (wm);
}

void
mod_destroy(struct wok_module *wm)
{
	dlclose(wm->wm_handle);
	PSCFREE(wm->wm_path);
	PSCFREE(wm);
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr,
	    "usage: %s [-dU] [-L cmd] [-o mountopt] [-S socket] node\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char c, *p, *noncanon_mp, *cmd, *path_env, dir[PATH_MAX];
	struct pscfs_args args = PSCFS_ARGS_INIT(0, NULL);
	struct psc_dynarray startup_cmds = DYNARRAY_INIT;
	const char *progpath = argv[0];
	int rc, i, unmount_first = 0;

	pfl_init();

	pscfs_addarg(&args, "");		/* progname/argv[0] */
	pscfs_addarg(&args, "-o");
	pscfs_addarg(&args, STD_MOUNT_OPTIONS);

	/* get default ctlsockfn value from environment */
	p = getenv("CTL_SOCK_FILE");
	if (p)
		ctlsockfn = p;

	while ((c = getopt(argc, argv, "dL:o:S:U")) != -1)
		switch (c) {
		case 'd':
			pscfs_addarg(&args, "-odebug");
			break;
		case 'L':
			psc_dynarray_add(&startup_cmds, optarg);
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

	pscthr_init(PFL_THRT_FSMGR, NULL, 0, "fsmgrthr");

	noncanon_mp = argv[0];
	if (unmount_first)
		unmount(noncanon_mp);

	/* canonicalize mount path */
	if (realpath(noncanon_mp, mountpoint) == NULL)
		psc_fatal("mount point %s", noncanon_mp);

	pscfs_mount(mountpoint, &args);
	pscfs_freeargs(&args);

	ctlthr_spawn();

	pfl_opstimerthr_spawn(PFL_THRT_OPSTIMER, "opstimerthr");
	pfl_workq_init(128, 1024, 1024);
	pfl_wkthr_spawn(PFL_THRT_WORKER, 4, 0, "wkthr%d");

	pscfs_entry_timeout = 8.;
	pscfs_attr_timeout = 8.;

	/*
	 * Here, $p = (directory this daemon binary resides in).
	 * Now we add the following to $PATH:
	 *
	 *   1) $p
	 *   2) $p/../wokctl (for developers)
	 */
	pfl_dirname(progpath, dir);
	p = getenv("PATH");
	rc = pfl_asprintf(&path_env, "%s:%s/../wokctl%s%s", dir, dir,
	    p ? ":" : "", p ? p : "");
	psc_assert(rc != -1);
	setenv("PATH", path_env, 1);

	/*
 	 * If wokctl (see file wokctl.c) misbehaves because it is given 
 	 * a wrong arugment, it is hard to debug from our end because 
 	 * we won't be receiving anything useful via the socket. This 
 	 * should be changed to a native call someday.
 	 *
 	 * If the client does not come up, double/triple checkout 
 	 * the name of your slash2 shared library. I wish I can
 	 * add more verbose debugging information.
 	 */
	DYNARRAY_FOREACH(cmd, i, &startup_cmds)
		pfl_systemf("wokctl -S %s %s", ctlsockfn, cmd);

	exit(pscfs_main(32, ""));
}
