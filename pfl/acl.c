/* $Id$ */
/* %PSC_COPYRIGHT% */

#include <endian.h>
#include <string.h>

#include "pfl/acl.h"
#include "pfl/log.h"
#include "pfl/cdefs.h"

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
	unsigned int xperm;
	acl_permset_t perm;
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

		xperm = le16toh(xe->perm);
		memset(&perm, 0, sizeof(perm));
		acl_clear_perms(perm);
		if (xperm & ACL_READ)
			acl_add_perm(perm, ACL_READ);
		if (xperm & ACL_WRITE)
			acl_add_perm(perm, ACL_WRITE);
		if (xperm & ACL_EXECUTE)
			acl_add_perm(perm, ACL_EXECUTE);
		acl_set_permset(e, perm);

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
