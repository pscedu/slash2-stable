/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/types.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/odtable.h"

static void
odtable_sync(struct odtable *odt, __unusedx size_t elem)
{
	int rc, flags;

	psc_assert(odt->odt_hdr);

	flags = (odt->odt_hdr->odth_options & ODTBL_OPT_SYNC) ?
		 MS_SYNC : MS_ASYNC;

	/* For now just sync the whole thing.
	 */
	rc = msync(odt->odt_base, ODTABLE_MAPSZ(odt), flags);
	if (rc)
		psc_error("msync error on table %p", odt);
}

struct odtable_receipt *
odtable_putitem(struct odtable *odt, void *data)
{
	struct odtable_entftr *odtf;
	struct odtable_receipt *odtr, todtr = { 0, 0 };
	psc_crc64_t crc;
	void *p;

	do {
		if (psc_vbitmap_next(odt->odt_bitmap, &todtr.odtr_elem) <= 0)
			return (NULL);
		odtf = odtable_getfooter(odt, todtr.odtr_elem);
	} while (odtable_footercheck(odtf, &todtr, 0));

	p = odtable_getitem_addr(odt, todtr.odtr_elem);
	odtf->odtf_inuse = ODTBL_INUSE;

	psc_crc64_calc(&crc, data, odt->odt_hdr->odth_elemsz);
	/* Setup the receipt.
	 */
	odtr = PSCALLOC(sizeof(*odtr));
	odtr->odtr_elem = todtr.odtr_elem;
	odtr->odtr_key = (uint64_t)crc;
	/* Write metadata into into the mmap'd footer.  For now the key and
	 *  CRC are the same.
	 */
	odtf->odtf_crc = crc;
	odtf->odtf_inuse = ODTBL_INUSE;
	memcpy(p, data, odt->odt_hdr->odth_elemsz);

	psc_trace("slot=%zd elemcrc=%"PSCPRIxCRC64, todtr.odtr_elem, crc);

	odtable_sync(odt, todtr.odtr_elem);

	return (odtr);
}

void *
odtable_getitem(struct odtable *odt, const struct odtable_receipt *odtr)
{
	void *data = odtable_getitem_addr(odt, odtr->odtr_elem);
	struct odtable_entftr *odtf = odtable_getfooter(odt, odtr->odtr_elem);

	if (odtable_footercheck(odtf, odtr, 1))
		return (NULL);

	if (odt->odt_hdr->odth_options & ODTBL_OPT_CRC) {
		psc_crc64_t crc;

		psc_crc64_calc(&crc, data, odt->odt_hdr->odth_elemsz);
		if (crc != odtf->odtf_crc) {
			odtf->odtf_inuse = ODTBL_BAD;
			psc_warnx("slot=%zd crc fail "
			    "odtfcrc=%"PSCPRIxCRC64" elemcrc=%"PSCPRIxCRC64,
			    odtr->odtr_elem, odtf->odtf_crc, crc);
		}
		return (NULL);
	}
	return (data);
 }

/**
 * odtable_freeitem - free the odtable slot which corresponds to the provided
 *   receipt.
 * Note: odtr is freed here.
 */
int
odtable_freeitem(struct odtable *odt, struct odtable_receipt *odtr)
{
	int rc;
	struct odtable_entftr *odtf = odtable_getfooter(odt, odtr->odtr_elem);

	if ((rc = odtable_footercheck(odtf, odtr, 1)))
		return (rc);

	odtf->odtf_inuse = ODTBL_FREE;

	odtable_sync(odt, odtr->odtr_elem);
	psc_vbitmap_unset(odt->odt_bitmap, odtr->odtr_elem);
	PSCFREE(odtr);

	return (0);
}

int
odtable_create(const char *f, size_t nelems, size_t elemsz)
{
	int rc = 0;
	size_t z;
	struct stat stb;
	struct odtable odt;
	struct odtable_entftr odtf = {0, ODTBL_FREE, 0, ODTBL_MAGIC};
	struct odtable_hdr odth = {nelems, elemsz, ODTBL_MAGIC, ODTBL_VERS,
				   ODTBL_OPT_CRC, ODTBL_ALIGN};

	odt.odt_hdr = &odth;

	if (!stat(f, &stb))
		return (-EEXIST);

	if (errno != ENOENT)
		return (-errno);

	odt.odt_fd = open(f, O_CREAT|O_TRUNC|O_WRONLY, 0600);
	if (odt.odt_fd < 0) {
		rc = -errno;
		goto out;
	}

	if (pwrite(odt.odt_fd, &odth, sizeof(odth), 0) != sizeof(odth)) {
		rc = -errno;
		goto out;
	}

	psc_trace("odt.odt_hdr.odth_start=%"PRIx64, odt.odt_hdr->odth_start);

	for (z=0; z < nelems; z++) {
		odtf.odtf_slotno = z;

		psc_trace("elem=%zd offset=%"PRIu64, z,
			   odtable_getoffset(&odt, z));

		if (pwrite(odt.odt_fd, &odtf, sizeof(odtf),
			   (elemsz + odtable_getoffset(&odt, z))) !=
		    sizeof(odtf)) {
			rc = -errno;
			goto out;
		}
	}
 out:
	close(odt.odt_fd);
	odt.odt_fd = -1;
	return (rc);
}

int
odtable_load(const char *f, struct odtable **t)
{
	int rc = 0, frc;
	size_t z;
	void *p;
	struct odtable *odt = PSCALLOC(sizeof(struct odtable));
	struct odtable_entftr *odtf;
	struct odtable_hdr *odth;;
	struct odtable_receipt todtr = {0, 0};

	psc_assert(t);
	*t = NULL;

	odt->odt_fd = open(f, O_RDWR, 0600);
	if (odt->odt_fd < 0) {
		psc_warnx("Fail to open bmap ownership table file %s", f);
		free(odt);
		return (-errno);
	}

	odth = odt->odt_hdr = PSCALLOC(sizeof(*odth));

	if (pread(odt->odt_fd, odth, sizeof(*odth), 0) != sizeof(*odth)) {
		rc = -errno;
		goto out;
	}

	if ((odth->odth_magic != ODTBL_MAGIC) ||
	    (odth->odth_version != ODTBL_VERS)) {
		rc = -EINVAL;
		goto out;
	}

	if ((rc = odtable_createmmap(odt)))
		goto out;

	odt->odt_bitmap = psc_vbitmap_new(odt->odt_hdr->odth_nelems);
	if (!odt->odt_bitmap) {
		rc = -ENOMEM;
		goto out_unmap;
	}

	for (z=0; z < odt->odt_hdr->odth_nelems; z++) {
		todtr.odtr_elem = z;
		p = odtable_getitem_addr(odt, z);
		odtf = odtable_getfooter(odt, z);

		frc = odtable_footercheck(odtf, &todtr, -1);
		/* Sanity checks for debugging.
		 */
		psc_assert(frc != ODTBL_MAGIC_ERR);
		psc_assert(frc != ODTBL_SLOT_ERR);

		if (odtf->odtf_inuse == ODTBL_FREE)
			psc_vbitmap_unset(odt->odt_bitmap, z);

		else if (odtf->odtf_inuse == ODTBL_INUSE) {
			psc_vbitmap_set(odt->odt_bitmap, z);

			if (odth->odth_options & ODTBL_OPT_CRC) {
				psc_crc64_t crc;

				psc_crc64_calc(&crc, p, odt->odt_hdr->odth_elemsz);
				if (crc != odtf->odtf_crc) {
					odtf->odtf_inuse = ODTBL_BAD;
					psc_warnx("slot=%zd crc fail "
					    "odtfcrc=%"PSCPRIxCRC64" elemcrc=%"PSCPRIxCRC64,
					    z, odtf->odtf_crc, crc);
				}
			}
		} else {
			psc_vbitmap_set(odt->odt_bitmap, z);
			psc_warnx("slot=%zd ignoring, bad inuse value"
			    "inuse=0x%"PRIx64,
			    z, odtf->odtf_inuse);
		}
	}

	psc_notify("odtable=%p base=%p has %d/%zd slots available"
		   " elemsz=%zd magic=%"PRIx64,
		   odt, odt->odt_base, psc_vbitmap_nfree(odt->odt_bitmap),
		   odth->odth_nelems, odth->odth_elemsz, odth->odth_magic);

	*t = odt;
 out:
	close(odt->odt_fd);
	odt->odt_fd = -1;
	if (rc) {
		if (odt->odt_base)
			munmap(odt->odt_base, ODTABLE_MAPSZ(odt));
		if (odt->odt_bitmap)
			psc_vbitmap_free(odt->odt_bitmap);
		free(odt->odt_hdr);
		free(odt);
	}
	return (rc);

 out_unmap:
	odtable_freemap(odt);
	goto out;
}

int
odtable_release(struct odtable *odt)
{
	int rc;

	psc_vbitmap_free(odt->odt_bitmap);
	odt->odt_bitmap = NULL;

	if (odtable_freemap(odt))
		psc_fatal("odtable_freemap() failed on %p", odt);

	PSCFREE(odt->odt_hdr);
	odt->odt_hdr = NULL;
	if (odt->odt_fd == -1)
		rc = 0;
	else {
		rc = close(odt->odt_fd);
		odt->odt_fd = -1;
	}
	PSCFREE(odt);
	return (rc);
}

void
odtable_scan(struct odtable *odt,
    void (*odt_handler)(void *, struct odtable_receipt *))
{
	int rc;
	struct odtable_receipt *odtr, todtr = {0, 0};
	struct odtable_entftr *odtf;

	for (todtr.odtr_elem=0; todtr.odtr_elem < odt->odt_hdr->odth_nelems;
	     todtr.odtr_elem++) {
		if (!psc_vbitmap_get(odt->odt_bitmap, todtr.odtr_elem))
			continue;

		odtf = odtable_getfooter(odt, todtr.odtr_elem);

		rc = odtable_footercheck(odtf, &todtr, 2);
		psc_assert(rc != ODTBL_FREE_ERR);
		if (rc) {
			psc_warnx("slot=%zd marked bad, skipping",
			    todtr.odtr_elem);
			continue;
		}

		odtr = PSCALLOC(sizeof(odtr));
		odtr->odtr_key  = odtf->odtf_key;
		odtr->odtr_elem = todtr.odtr_elem;

		psc_warnx("handing back key=%"PRIx64" slot=%zd odtr=%p",
		    odtr->odtr_key, odtr->odtr_elem, odtr);

		if (odt_handler)
			(odt_handler)(odtable_getitem_addr(odt,
					   todtr.odtr_elem), odtr);
	}
}
