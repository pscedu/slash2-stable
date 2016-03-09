/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2014-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <string.h>

#include "pfl/acl.h"
#include "pfl/cdefs.h"
#include "pfl/endian.h"
#include "pfl/log.h"

#define ACL_EA_VERSION		2
#define ACL_EA_ACCESS		"system.posix_acl_access"

struct acl_ea_entry {
	u_int16_t		tag;
	u_int16_t		perm;
	u_int32_t		id;
} __packed;

struct acl_ea_header {
	u_int32_t		version;
	struct acl_ea_entry	entries[0];
} __packed;

acl_t
pfl_acl_from_xattr(const void *buf, size_t size)
{
	int i, entries;
	const struct acl_ea_header *h = buf;
	const struct acl_ea_entry *xe = PSC_AGP(h + 1, 0);
	unsigned int xperms;
	acl_permset_t permset;
	acl_entry_t e;
	acl_tag_t tag;
	acl_t a;

	if (size < sizeof(*h)) {
		errno = EINVAL;
		return (NULL);
	}
	if (le32toh(h->version) != ACL_EA_VERSION) {
		errno = EINVAL;
		return (NULL);
	}
	size -= sizeof(*h);
	if (size % sizeof(*xe)) {
		errno = EINVAL;
		return (NULL);
	}
	entries = size / sizeof(*xe);

	a = acl_init(entries);
	if (a == NULL)
		return (NULL);
	for (i = 0; i < entries; i++, xe++) {
		acl_create_entry(&a, &e);
		if (acl_get_permset(e, &permset) == -1)
			psclog_error("get_permset");
		acl_clear_perms(permset);

		xperms = le16toh(xe->perm);

		if (xperms & ACL_READ)
			acl_add_perm(permset, ACL_READ);
		if (xperms & ACL_WRITE)
			acl_add_perm(permset, ACL_WRITE);
		if (xperms & ACL_EXECUTE)
			acl_add_perm(permset, ACL_EXECUTE);
		if (acl_set_permset(e, permset) == -1)
			psclog_error("set_permset");

		acl_set_tag_type(e, tag = le16toh(xe->tag));

		switch (tag) {
		case ACL_USER: {
			uid_t uid = le32toh(xe->id);

			acl_set_qualifier(e, &uid);
			break;
		    }
		case ACL_GROUP: {
			gid_t gid = le32toh(xe->id);

			acl_set_qualifier(e, &gid);
			break;
		    }
		}
	}
	return (a);
}
