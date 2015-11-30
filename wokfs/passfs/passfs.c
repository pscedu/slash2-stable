/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "pfl/fs.h"
#include "pfl/fsmod.h"
#include "pfl/atomic.h"

psc_atomic64_t	passfs_inum = PSC_ATOMIC64_INIT(1);

#define SZ_100G	(1024 * UINT64_C(1024) * 1024 * 100)

#define ENTR_TIMEOUT	8
#define ATTR_TIMEOUT	8

void
passfsop_lookup(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	struct stat stb;

	(void)pinum;
	(void)name;

	memset(&stb, 0, sizeof(stb));
	stb.st_ino = psc_atomic64_inc_getnew(&passfs_inum);
	stb.st_mode = S_IFREG | 0644;
	stb.st_nlink = 1;
	stb.st_blksize = 512;
	stb.st_size = SZ_100G;
	pscfs_reply_lookup(pfr, stb.st_ino, 0, ENTR_TIMEOUT, &stb,
	    ATTR_TIMEOUT, 0);
}

void
passfsop_getattr(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	struct stat stb;

	memset(&stb, 0, sizeof(stb));
	stb.st_ino = inum;
	stb.st_mode = S_IFREG | 0644;
	stb.st_nlink = 1;
	stb.st_blksize = 512;
	stb.st_size = SZ_100G;
	pscfs_reply_getattr(pfr, &stb, ATTR_TIMEOUT, 0);
}

void
passfsop_setattr(struct pscfs_req *pfr, pscfs_inum_t inum,
    struct stat *stb, int to_set, void *data)

{
	(void)inum;
	(void)stb;
	(void)to_set;
	(void)data;

	stb->st_ino = psc_atomic64_inc_getnew(&passfs_inum);
	stb->st_mode = S_IFREG | 0644;
	stb->st_nlink = 1;
	stb->st_blksize = 512;
	stb->st_size = SZ_100G;

	pscfs_reply_getattr(pfr, stb, ATTR_TIMEOUT, 0);
}

void
passfsop_open(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags)
{
	(void)inum;
	(void)oflags;

	pscfs_reply_open(pfr, NULL, PSCFS_OPENF_DIO, 0);
}

void
passfsop_read(struct pscfs_req *pfr, size_t size, off_t off, void *data)
{
	static struct psc_spinlock lock = SPINLOCK_INIT;
	static size_t buflen;
	static void *buf;

	struct iovec iov;

	(void)data;
	(void)off;

	if (size > buflen) {
		spinlock(&lock);
		if (size > buflen) {
			buf = realloc(buf, size);
			if (buf == NULL)
				err(1, NULL);
			buflen = size;
		}
		freelock(&lock);
	}

	iov.iov_base = buf;
	iov.iov_len = size;

	pscfs_reply_read(pfr, &iov, 1, 0);
}

void
passfsop_write(struct pscfs_req *pfr, const void *buf, size_t size,
    off_t off, void *data)
{
	(void)buf;
	(void)data;
	(void)off;

	pscfs_reply_write(pfr, size, 0);
}

void
pscfs_module_load(struct pscfs *m)
{
	m->pf_handle_lookup = passfsop_lookup;
	m->pf_handle_getattr = passfsop_getattr;
	m->pf_handle_setattr = passfsop_setattr;
	m->pf_handle_open = passfsop_open;
	m->pf_handle_read = passfsop_read;
	m->pf_handle_write = passfsop_write;
}
