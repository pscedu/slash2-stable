#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "psc_types.h"
#include "psc_util/log.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
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

size_t
odtable_putitem(struct odtable *odt, void *data)
{
	size_t elem, rc = 0;
	void *p;
	struct odtable_entftr *odtf;

	do { 
		if (vbitmap_next(odt->odt_bitmap, &elem) <= 0)
			return (ODTBL_ERR);
		odtf = odtable_getfooter(odt, elem);
		odtable_footercheck(odtf, elem, 0, rc);
	} while (rc);
	
	p = odtable_getitem_addr(odt, elem);
	odtf->odtf_inuse = ODTBL_INUSE;
	
	if (odt->odt_hdr->odth_options & ODTBL_OPT_CRC) {	
		psc_crc_t crc;
		
		psc_crc_calc(&crc, p, odt->odt_hdr->odth_elemsz);
		odtf->odtf_crc = crc;
		psc_warnx("slot=%"_P_U64"d crc  odtfcrc=%"_P_U64"x \
elemcrc=%"_P_U64"x", elem, odtf->odtf_crc, crc);

		memcpy(p, data, odt->odt_hdr->odth_elemsz);
	}
	odtable_sync(odt, elem);

	return (elem);
}

void *
odtable_getitem(struct odtable *odt, size_t elem)
{
	void *data = odtable_getitem_addr(odt, elem);
	struct odtable_entftr *odtf = odtable_getfooter(odt, elem);
	size_t rc;
	
	odtable_footercheck(odtf, elem, 1, rc);
	if (rc)
		return (NULL);

	if (odt->odt_hdr->odth_options & ODTBL_OPT_CRC) {	
		psc_crc_t crc;
		
		psc_crc_calc(&crc, data, odt->odt_hdr->odth_elemsz);
		if (crc != odtf->odtf_crc) {
			//odtf->odtf_inuse = ODTBL_BAD;
			psc_warnx("slot=%"_P_U64"d crc fail odtfcrc=%"_P_U64"x elemcrc=%"_P_U64"x", elem, odtf->odtf_crc, crc);
		}
		return (NULL);
	}	
	return (data);	
}

void 
odtable_freeitem(struct odtable *odt, size_t elem)
{
	size_t rc;
	struct odtable_entftr *odtf = odtable_getfooter(odt, elem);

	odtable_footercheck(odtf, elem, 1, rc);
	if (rc)
		return;

	odtf->odtf_inuse = ODTBL_FREE;

	odtable_sync(odt, elem);
	vbitmap_unset(odt->odt_bitmap, elem);

	return;
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

	odt.odt_fd = open(f, O_CREAT|O_TRUNC|O_WRONLY, 0700);
	if (odt.odt_fd < 0) {
		rc = -errno;
		goto out;
	}
	
	if (pwrite(odt.odt_fd, &odth, sizeof(odth), 0) != sizeof(odth)) {
		rc = -errno;
                goto out;
	}

	psc_errorx("odt.odt_hdr.odth_start=%"_P_U64"x", 
		   odt.odt_hdr->odth_start);

	for (z=0; z < nelems; z++) {
		odtf.odtf_slotno = z;

		psc_errorx("elem=%"_P_U64"d offset=%"_P_U64"u ", z, odtable_getoffset(&odt, z));

		if (pwrite(odt.odt_fd, &odtf, sizeof(odtf),
			   (elemsz + odtable_getoffset(&odt, z))) != 
		    sizeof(odtf)) {
			rc = -errno;
			goto out;
		}			
	}
 out:
	close(odt.odt_fd);
	return (rc);
}

int
odtable_load(const char *f, struct odtable **t)
{
	int rc = 0;	
	size_t z, frc;
	void *p;
	struct odtable *odt = PSCALLOC(sizeof(struct odtable));
	struct odtable_entftr *odtf;
	struct odtable_hdr *odth;;

	psc_assert(t);
	*t = NULL;

	odt->odt_fd = open(f, O_RDWR, 0700);
	if (odt->odt_fd < 0)
		return (-errno);

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

	odt->odt_bitmap = vbitmap_new(odt->odt_hdr->odth_nelems);
	if (!odt->odt_bitmap) {
		rc = -ENOMEM;
		goto out_unmap;
	}

	for (z=0; z < odt->odt_hdr->odth_nelems; z++) {
		p = odtable_getitem_addr(odt, z);
		odtf = odtable_getfooter(odt, z);

		odtable_footercheck(odtf, z, -1, frc);
		/* Sanity checks for debugging.
		 */
		psc_assert(frc != ODTBL_MAGIC_ERR);	
		psc_assert(frc != ODTBL_SLOT_ERR);

		if (odtf->odtf_inuse == ODTBL_FREE)
			vbitmap_unset(odt->odt_bitmap, z);

		else if (odtf->odtf_inuse == ODTBL_INUSE) {
			if (odth->odth_options & ODTBL_OPT_CRC) {
				psc_crc_t crc;
				
				psc_crc_calc(&crc, p, odt->odt_hdr->odth_elemsz);
				if (crc != odtf->odtf_crc) {
					//odtf->odtf_inuse = ODTBL_BAD;
					vbitmap_set(odt->odt_bitmap, z);
					psc_warnx("slot=%"_P_U64"d crc fail odtfcrc=%"_P_U64"x elemcrc=%"_P_U64"x", z, odtf->odtf_crc, crc);
				}				
			} else
				vbitmap_set(odt->odt_bitmap, z);	

		} else {
			vbitmap_set(odt->odt_bitmap, z);
			psc_warnx("slot=%"_P_U64"d ignoring, bad inuse value"
				  "inuse=0x%"_P_U64"x", 
				  z, odtf->odtf_inuse);
		}			
	}

	psc_notify("odtable=%p has %d/%"_P_U64"d slots available",	   
		   odt, vbitmap_nfree(odt->odt_bitmap), odth->odth_nelems);

	*t = odt;
 out:
	close(odt->odt_fd);
	return (rc);
	
 out_unmap:
	odtable_freemap(odt);
	goto out;
}

int 
odtable_release(struct odtable *odt) {
	PSCFREE(odt->odt_hdr);
	vbitmap_free(odt->odt_bitmap);
	
	if (odtable_freemap(odt))
		psc_fatal("odtable_freemap() failed on %p", odt);	

	return (close(odt->odt_fd));
}

void
odtable_scan(struct odtable *odt, void (*odt_handler)(void *))
{
	size_t z, rc;
	
	for (z=0; z < odt->odt_hdr->odth_nelems; z++) {
		if (!vbitmap_get(odt->odt_bitmap, z))
			continue;
		
		odtable_footercheck(odtable_getfooter(odt, z), z, 1, rc);
		psc_assert(rc != ODTBL_FREE_ERR);

		if (rc) {
			psc_warnx("slot=%"_P_U64"d marked bad, skipping", z);
			continue;
		}		
		(odt_handler)(odtable_getitem(odt, z));		
	}
}

