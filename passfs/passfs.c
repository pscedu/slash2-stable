/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pfl/fs.h"

void
passfsop_open(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags)
{
	pscfs_reply_open();
}

void
passfsop_read(struct pscfs_req *pfr, size_t size, off_t off, void *data)
{
	pscfs_reply_read();
}

void
passfsop_write(struct pscfs_req *pfr, const void *buf, size_t size,
    off_t off, void *data)
{
	pscfs_reply_write();
}

void
pscfs_module_load(struct pscfs *m)
{
	m->pf_handle_open = passfsop_open;
	m->pf_handle_read = passfsop_read;
	m->pf_handle_write = passfsop_write;
}
