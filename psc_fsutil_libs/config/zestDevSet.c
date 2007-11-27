/* $Id: zestDevSet.c 2183 2007-11-06 19:44:14Z yanovich $ */

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "zestAlloc.h"
#include "zestAssert.h"
#include "zestConfig.h"
#include "zestDevSet.h"
#include "zestList.h"
#include "zestLock.h"
#include "zestLog.h"
#include "zestParitySvc.h"
#include "zestReadWrite.h"
#include "zestSuperBlock.h"
#include "zestTypes.h"
#include "myrandom.h"
#include "vbitmap.h"

/* Pointer to parity disk. */
zdisk_t *parityDsk;
struct hash_table zdiskHash;

znode_t      *zestNodeInfo;
znode_prof_t *zestNodeProfile;
char         *zestHostname;

/**
 * load_dev_set - open each device matched by the profile glob, also
 *	allocates superblock and file des arrays.
 * @z:   zest node structure
 * Returns: 0 on success, -1 on failure
 */
int
load_dev_set(znode_t *z)
{
	znode_prof_t   *p = z->znode_profile;
	zdisk_t        *d;
	unsigned        failed_disks = 0;
	int             rc, i, loglevel;
	char           *dev;
	int             prty_dev = 0;
	char		*sb;
	struct zdio	zdio;
	struct stat     stb;

	ENTRY_MARKER;

	INIT_ZLIST_HEAD(&z->znode_disks);
	INIT_ZLIST_HEAD(&z->znode_twin_disks);

	/*
	 * Initialize the zdisk lookup hash
	 */
	init_hash_table(&zdiskHash, ZDISK_HASH_SZ, "disks");

	if (get_devs(z) <= 0) {
		zerrorx("get_devs() failed to retrieve devices");
		return -1;
	}
	ztrace("get_devs returned %zu potential devices",
	    z->znode_ndisks_found);

	sb = palloc(ZSUPERBLOCKSZ);

	for (i=0; i < (z->znode_ndisks_found + 1); i++) {
		if (i == z->znode_ndisks_found) {
			/*
			 * Check for parity device on the last
			 *   iteration
			 */
			prty_dev = 1;

			ztrace("Looking for Parity Device: %s",
			    p->znprof_prtydev);

			if ( stat(p->znprof_prtydev, &stb) ) {
				zerror("Can't stat parity dev %s",
				    p->znprof_prtydev);
				rc = -1;
				goto done;
			}
			if ( !( S_ISREG(stb.st_mode) ||
				S_ISCHR(stb.st_mode) ||
				S_ISBLK(stb.st_mode) ) ) {
				zerrorx("Parity dev %s is the wrong type",
				    p->znprof_prtydev);
				rc = -1;
				goto done;
			}
			dev = p->znprof_prtydev;

		} else {
			dev = p->znprof_devlist[i];
			znotify("\t\tDevice%03d: %s", i, dev);
		}

		zdio_init(&zdio);
		if (z->znode_profile->znprof_virtual)
			IO_ATTR_SET(&zdio, ZEST_IO_VIRTUAL);
		else {
			if (z->znode_profile->znprof_sgio)
				IO_ATTR_SET(&zdio, ZEST_IO_SGIO);
			else
				IO_ATTR_SET(&zdio, ZEST_IO_BLOCK);

			/* Disallow direct I/O to the parity device. */
			if (z->znode_profile->znprof_dio &&
			    !prty_dev)
				IO_ATTR_SET(&zdio, ZEST_IO_DIRECT);
		}

		/*
		 * Open the device, store its fd in the array
		 */
		zest_dev_open(&zdio, dev);
		if (IO_ATTR_TEST(&zdio, ZEST_IO_SGIO))
			zdio_setsgiosz(&zdio, p->znprof_sgio_sz);

		d            = ZALLOC(sizeof(zdisk_t));
		LOCK_INIT(&d->zd_lock);
		d->zd_zdio   = zdio;
		d->zd_device = dev;
		d->zd_ciods_sz = p->znprof_bdcon_sz;

		/*
		 * Tag the parity device
		 */
		if (prty_dev) {
			d->zd_id = ZEST_PRTYDEV_ID;
			zpdio	 = zdio;
		} else
			d->zd_id = 0;

		znotify("opened dev ;%s; fd = %d id = %d",
		    d->zd_device, zdio_get_fd(&zdio), d->zd_id);

		zdisk_put(d);

		loglevel = zest_setloglevel(ZLL_WARN);
		if ( zest_pread(&zdio, sb, ZSUPERBLOCKSZ, 0) != ZSUPERBLOCKSZ ) {
			failed_disks++;
			zest_setloglevel(loglevel);
			zdio_close(&zdio);
			zdisk_del(d);
			free(d);
			znotify("Could not access device: %s", dev);
			if (prty_dev)
				zfatal("Whoops, can't access parity device");
			continue;
		}
		zest_setloglevel(loglevel);
		/*
		 * Put everything on my list, the twin's
		 *  disks will be handled later.
		 */
		zlist_add_tail(&d->zd_lentry, &z->znode_disks);
	}
	znotify("Found %zu disks, %d are unusable",
	    z->znode_ndisks_found, failed_disks);

	z->znode_ndisks_found -= failed_disks;

	EXIT_MARKER;
	if (!z->znode_ndisks_found)
		return -1;

	rc = z->znode_ndisks_found;
 done:
	free(sb);
	return (rc);
}

/**
 * assemble_dev_set - determine if the devices associated with the fd's
 *	and sb's are affiliated with each other and this node.
 * @z:   zest node profile
 * Returns: 0 on success, -1 on failure
 */
int assemble_dev_set(znode_t *z)
{
	znode_prof_t     *p = z->znode_profile;
	zdisk_t          *d = NULL, **disks = NULL;
	zsuper_block_t   *s;
	struct zlist_head *l, *m;
	int               ret = 0, n = 0, i = 0, have_pdev=0;
	size_t            save_nblks = 0, save_blksz = 0;

	z->znode_ndisks_found = load_dev_set(z);

	znotify("found ndisks %zu, would like %zu",
	    z->znode_ndisks_found, p->znprof_ndisks);

	if (z->znode_ndisks_found <= 0)
		zfatalx("Can't build set, no devices available");

	zlist_for_each_safe(l, m, &z->znode_disks) {
		/*
		 * Cast the disk structure and
		 *   point to the superblock storage
		 */
		d = zlist_entry(l, zdisk_t, zd_lentry);

		if (d->zd_id == ZEST_PRTYDEV_ID) {
			znotify("%s is a parity device",
			    d->zd_device);
			zest_assert_msg( (!have_pdev),
			    "Can't have multiple parity devices"
			    "%s", d->zd_device);
			have_pdev = 1;
			zlist_del(l);
			/*
			 * Try to load the parity superblock
			 */
			if (zpsuper_load(z) == NULL) {
				zerrorx("Parity Dev Load Failed, %s",
				    p->znprof_prtydev);
				ret = -1;
				goto done;
			}
			parityDsk = d;
			continue;
		}
		/*
		 * It's not the parity device
		 */
		s = zsuper_load(zdio_get_fd(&d->zd_zdio));
		if (s == NULL) {
			znotify("Got null super from dev ;%s;", d->zd_device);
			goto fail_disk;
		}
		/*
		 * Does this disk belong to our twin?
		 */
		if (s->zsuper_node_id == z->znode_twin_id) {
			znotify("Dev is owned by our twin ;%s;", d->zd_device);
			z->znode_ndisks_found--;
			zlist_del(l);
			zlist_add(l, &z->znode_twin_disks);

		} else if (s->zsuper_node_id == z->znode_id) {
			znotify("Dev is owned by us ;%s;", d->zd_device);

			if (s->zsuper_set_uuid == z->znode_set_uuid) {
				znotify("Dev ;%s; matches set_uuid %"ZLPX64,
				    d->zd_device, z->znode_set_uuid);

				if (disks == NULL) {
					n          = s->zsuper_ndisks;
					disks      = ZALLOC(sizeof(zdisk_t *) * n);
					save_blksz = s->zsuper_blksz;
					save_nblks = s->zsuper_nblks;
				}
				/*
				 * Store the disk struct
				 */
				disks[s->zsuper_disk_id] = d;
			} else {
				znotify("Dev ;%s; has wrong UUID %"
				    ZLPX64" %"ZLPX64,
				    d->zd_device, s->zsuper_set_uuid,
				    z->znode_set_uuid);
				goto fail_disk;
			}
		} else {
			znotify("Dev ;%s; is owned by %hx",
			    d->zd_device,
			    (zest_node_id_t)s->zsuper_node_id);
			goto fail_disk;
		}
		continue;

	fail_disk:
		z->znode_ndisks_found--;
		zlist_del(&d->zd_lentry);
		free(d);
	}

	if (disks == NULL) {
		zerrorx("Found no devices belonging to set %"ZLPX64,
		    z->znode_set_uuid);
		ret = -1;
		goto done;
	}

	for (i=0; i < n; i++) {
		if (disks[i] == NULL) {
			/*
			 * Handle Missing Drives
			 */
			zerrorx("Missing Zest Drive %d", i);
			d = ZALLOC(sizeof(zdisk_t));
			d->zd_id                 = i;
			d->zd_failed             = 1;
			d->zd_device             = NULL;
			d->zd_usedtab            = NULL;
			d->zd_sb.zsuper_set_uuid = z->znode_set_uuid;
			d->zd_sb.zsuper_magic    = ZEST_SUPER_MAGIC;
			d->zd_sb.zsuper_node_id  = z->znode_id;
			d->zd_sb.zsuper_disk_id  = i;
			/*
			 * Make an educated guess.. maybe later this
			 *   stuff will go into a the parity service.
			 * These values are needed for recovery..
			 *  I guess it means that the number of blocks should
			 *  be consistent across devices.
			 */
			d->zd_nblks   = d->zd_sb.zsuper_nblks = save_nblks;
			d->zd_blksiz  = d->zd_sb.zsuper_blksz = save_blksz;
			d->zd_usedtab = NULL;
			/*
			 * Put the failed-disk pointer in place and
			 *   add to the list
			 */
			disks[i] = d;
			zlist_add_tail(&d->zd_lentry, &z->znode_disks);
		} else {
			d = disks[i];
			/*
			 * A good disk
			 */
			//zerrorx("%p superblock", s);
			d->zd_nblks   = d->zd_sb.zsuper_nblks;
			d->zd_blksiz  = d->zd_sb.zsuper_blksz;
			d->zd_usedtab = vbitmap_new(d->zd_nblks);
			if (d->zd_usedtab == NULL)
				zfatal("vbitmap_new");

			iostats_init(&d->zd_stats.zdstat_rd,
			    "disk-%02d-rd", d->zd_id);
			iostats_init(&d->zd_stats.zdstat_wr,
			    "disk-%02d-wr", d->zd_id);
		}
	}
done:
	free(disks);
	return (ret);
}

/**
 * init_dev_set - used during formatting, initializes superblocks for
 *	each device handled by the specified node configuration.
 * @z:   zest node profile
 * Returns: 0 on success, -1 on failure
 */
int init_dev_set(znode_t *z)
{
	znode_prof_t     *p   = z->znode_profile;
	zsuper_block_t   *s   = NULL;
	zpsuper_block_t  *psb = NULL;
	size_t            spb, nblks;
	unsigned int      sectsz, disks_used = 0;
	int               sb_sects = (ZSUPERBLOCKSZ >> 9) + 1;
	int               have_pdev = 0;
	struct zlist_head *l, *m;
	zdisk_t          *d;

	ENTRY_MARKER;

	if (!z->znode_set_uuid)
		z->znode_set_uuid = p->znprof_setuuid = myrandom64();

	z->znode_ndisks_found = load_dev_set(z);
	if (z->znode_ndisks_found < (ssize_t)p->znprof_ndisks) {
		zfatalx("Not enough available disks (%zd) or bad parity dev",
		    z->znode_ndisks_found);
		return -1;
	}

	zwarnx("found ndisks %zu, need %zu (set uuid = %"ZLPX64")",
		z->znode_ndisks_found, p->znprof_ndisks, z->znode_set_uuid);

	sectsz = 0;
	zlist_for_each_safe(l, m, &z->znode_disks) {
		d = zlist_entry(l, zdisk_t, zd_lentry);

		ztrace("disks_used = %d", disks_used);

		/*
		 * Check the sect sz and num_sects, sector sizes must be
		 *  consistent across the set
		 */
		zest_dev_getsize(&d->zd_zdio, &d->zd_nsect, &d->zd_sectsz);

		znotify("Dev %s: SectorSize %d NumSectors %d",
		    d->zd_device, d->zd_sectsz, d->zd_nsect);

		if (sectsz) {
			if (p->znprof_sect_sz)
				d->zd_sectsz = sectsz;
			else if (sectsz != d->zd_sectsz)
				zwarnx("%s has invalid sector size %u",
				    d->zd_device, d->zd_sectsz);
		} else {
			/*
			 * First disk: handle set disk sector size.
			 * If the user specified a custom sectsz,
			 * it is specified in p->znprof_sect_sz.
			 */
			if (p->znprof_sect_sz) {
				if (p->znprof_sect_sz != d->zd_sectsz)
					zwarnx("Overriding dev sector size %u with %zu",
					    d->zd_sectsz, p->znprof_sect_sz);
				d->zd_sectsz = sectsz = p->znprof_sect_sz;
			} else
				sectsz = d->zd_sectsz;
		}

		/*
		 * Ensure we have a non-zero sector size and verify
		 *  that the sizes are sane
		 */
		zest_assert_msg( (d->zd_sectsz),
		    "Sector size is '%d', please specify -s",
		    d->zd_sectsz);

		if (p->znprof_block_sz % d->zd_sectsz)
			zfatalx("Block size %zu not a multiple"
			    " of sectsz %d",
			    p->znprof_block_sz, d->zd_sectsz);

		if (p->znprof_bdcon_sz % d->zd_sectsz)
			zfatalx("Bdcon size %zu not a multiple"
			    " of sectsz %d",
			    p->znprof_bdcon_sz,d->zd_sectsz);

		/*
		 * Calculate the number of blocks based on the
		 *  sector size - sectors per block, take into
		 *  account the container sz
		 */
		spb = (p->znprof_block_sz / d->zd_sectsz) +
			(p->znprof_bdcon_sz / d->zd_sectsz);

		if (!p->znprof_nblocks)
			/*
			 * Save nblocks to the superblock for this disk
			 */
			nblks = (d->zd_nsect-sb_sects) / spb;
		else {
			/*
			 * User specified the number of blocks,
			 *  verify nblks
			 */
			size_t b = (d->zd_nsect-sb_sects) / spb;
			if (p->znprof_nblocks > b) {
				znotify("Dev %s: SectorSize %d NumSectors %d Spb %zu",
					d->zd_device, d->zd_sectsz, d->zd_nsect, spb);
				zfatalx("nblocks val too small (max %zu)", b);
				goto fail_disk;
			}
			nblks = p->znprof_nblocks;
		}

		if (d->zd_id == ZEST_PRTYDEV_ID) {
			znotify("%s is a parity device", d->zd_device);
			psb = &d->zd_psb;
			parityDsk = d;
			zlist_del(l);

			zest_assert(!have_pdev);
			have_pdev = 1;

			psb->zpsuper_set_uuid = z->znode_set_uuid;
			psb->zpsuper_uuid     = myrandom64();
			psb->zpsuper_node_id  = z->znode_id;
			psb->zpsuper_nblks    = nblks;
			psb->zpsuper_sectsz   = d->zd_sectsz;

			zpsuper_crc_get(psb);
			zpsuper_update(psb);
		} else {
			if (disks_used == p->znprof_ndisks) {
				ztrace("Freeing device %s %p",
				    d->zd_device, d);
				zlist_del(&d->zd_lentry);
				free(d);
				continue;
			}

			// Check for exising superblock here
			// if valid sb exists goto fail_disk??
			s = &d->zd_sb;
			s->zsuper_magic      = ZEST_SUPER_MAGIC;
			s->zsuper_set_uuid   = z->znode_set_uuid;
			s->zsuper_uuid       = myrandom64();
			s->zsuper_ndisks     = p->znprof_ndisks;
			s->zsuper_blksz      = p->znprof_block_sz;
			s->zsuper_bdconsz    = p->znprof_bdcon_sz;
			s->zsuper_nblks      = nblks;
			s->zsuper_sectsz     = d->zd_sectsz;
			s->zsuper_disk_id    = disks_used;
			s->zsuper_node_id    = z->znode_id;
			s->zsuper_badblk_cnt = 0;

			d->zd_id = s->zsuper_disk_id;

			disks_used++;
		}
		continue;

	fail_disk:
		z->znode_ndisks_found--;
		zlist_del(&d->zd_lentry);
		free(d);
		if (z->znode_ndisks_found < (ssize_t)p->znprof_ndisks) {
			zfatalx("Not enough available disks!");
			return -1;
		}
	}
	zest_assert(have_pdev);

	zlist_for_each_safe(l, m, &z->znode_disks) {
		d = zlist_entry(l, zdisk_t, zd_lentry);
		s = &d->zd_sb;

		/*
		 * Subtract some blocks from the beginning of the
		 *  first device so that the parity dev sb doesn't
		 *  get overwritten
		 */
		if ( d->zd_id == 0) {
			zerrorx("parityDsk->zd_sectsz %u", parityDsk->zd_sectsz);
			s->zsuper_skip_blks = 1 + (ZPTYSUPERBLOCKSZ/parityDsk->zd_sectsz);
			ztrace("ID%hu Blk Skip %zu",
			    s->zsuper_disk_id,
			    s->zsuper_skip_blks);
		} else
			s->zsuper_skip_blks = 0;

		zsuper_update(d, s);

		znotify("New SuperBlock Data %s", d->zd_device);

		zsuper_dump(s);
	}
	return 0;
}

/**
 * get_devs - run glob() to gather the device list.
 * @z: our node configuration
 * Return:  the number of devices found by glob
 */
int get_devs(znode_t *z)
{
	struct zest_profile_device {
		char			*zpd_dev;
		struct zlist_head	 zpd_link;
	} *zpd;
	struct zlist_head *ent, *next, devlist;
	znode_prof_t *p = z->znode_profile;
	char *s, *t, *devglob;
	glob_t gl;
	size_t i;
	int rc;

	ENTRY_MARKER;

	INIT_ZLIST_HEAD(&devlist);
	devglob = strdup(p->znprof_devices);
	if (devglob == NULL)
		zfatal("strdup");

	for (s = devglob; s != NULL; s = t) {
		if ((t = strchr(s, '|')) != NULL)
			*t++ = '\0';

		rc = glob(s, GLOB_MARK, NULL, &gl);

		switch (rc) {
		case 0:
			for (i = 0; i < gl.gl_pathc; i++) {
				zpd = ZALLOC(sizeof(*zpd));
				INIT_ZLIST_ENTRY(&zpd->zpd_link);
				if ((zpd->zpd_dev =
				    strdup(gl.gl_pathv[i])) == NULL)
					zfatal("strdup");
				zlist_add_tail(&zpd->zpd_link,
				    &devlist);
			}
			z->znode_ndisks_found += gl.gl_pathc;
			break;

		case GLOB_NOMATCH:
			break;

		default:
			zwarn("glob(%s)", s);
			break;
		}
	}

	p->znprof_devlist = ZALLOC(z->znode_ndisks_found *
	    sizeof(*p->znprof_devlist));
	i = 0;
	zlist_for_each_safe(ent, next, &devlist) {
		zpd = zlist_entry(ent, struct zest_profile_device,
		    zpd_link);
		p->znprof_devlist[i++] = zpd->zpd_dev;
		free(zpd);
	}

	free(devglob);

	EXIT_MARKER;
	return (z->znode_ndisks_found);
}

/**
 * query_dev - report device info
 * @dev_fd:  the file des for the device
 */
int query_dev(int dev_fd)
{
	zsuper_block_t  *s;
	zdisk_t         *d = zdisk_get(dev_fd);

	ENTRY_MARKER;

	if (d == NULL)
		return -1;

	if (d->zd_id == ZEST_PRTYDEV_ID) {
		zpsuper_block_t *psb = zparitySuperBlock;

		if (zparitySuperBlock == NULL) {
			fprintf(stderr,
			    "Parity Dev %s is not setup\n",
			    d->zd_device);
			return 0;
		}
		fprintf(stderr,
		    "Parity Dev %s:\tSize Info:\tnsect=%u, sectsz=0x%x,"
		    "\n\t\tUUID:\t\t%"ZLPX64
		    "\n\t\tSetUUID:\t%"ZLPX64
		    "\n\t\tCRC:\t\t0x%"ZLPX64
		    "\n\t\tNodeId:\t\t0x%hx"
		    "\n\t\tNblocks:\t%zu\n",
		    d->zd_device, d->zd_nsect, d->zd_sectsz,
		    psb->zpsuper_uuid, psb->zpsuper_set_uuid,
		    psb->zpsuper_crc,  psb->zpsuper_node_id,
		    psb->zpsuper_nblks);
	} else {
		s = zsuper_load(dev_fd);

		if (s == NULL)
			fprintf(stderr,
			    "Dev %s:\tSize Info:\tnsect=%u, sectsz=0x%x"
			    "\n\t\t**No Superblock Info**\n",
			    d->zd_device, d->zd_nsect, d->zd_sectsz);
		else
			fprintf(stderr,
			    "Dev %s:\tSize Info:\tnsect=%u, sectsz=0x%x,"
			    "\n\t\tUUID:\t\t%"ZLPX64
			    "\n\t\tSetUUID:\t%"ZLPX64
			    "\n\t\tDiskId:\t\t0x%hx"
			    "\n\t\tNodeId:\t\t0x%hx"
			    "\n\t\tBlkSz:\t\t0x%zx"
			    "\n\t\tBdconSz:\t0x%zx"
			    "\n\t\tNblocks:\t%zu"
			    "\n\t\tNdisks:\t\t%zu"
			    "\n\t\tSkipBlks:\t%zu\n",
			    d->zd_device, d->zd_nsect, d->zd_sectsz,
			    s->zsuper_uuid, s->zsuper_set_uuid,
			    s->zsuper_disk_id, s->zsuper_node_id,
			    s->zsuper_blksz, s->zsuper_bdconsz,
			    s->zsuper_nblks, s->zsuper_ndisks,
			    s->zsuper_skip_blks);
	}
	EXIT_MARKER;
	return 0;
}
