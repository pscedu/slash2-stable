/* $Id: zestConfig.h 2189 2007-11-07 22:18:18Z yanovich $ */

#ifndef HAVE_ZCONFIG_INC
#define HAVE_ZCONFIG_INC

#define ZNPROF_NAME_MAX 64
#define ZNODE_NAME_MAX  MAXHOSTNAMELEN

#define DEVNAMEMAX 128

#include <sys/param.h>

#include "zestList.h"
#include "zestLog.h"
#include "zestSuperBlock.h"
#include "zestTypes.h"

struct zest_node_profile {
	char   znprof_name[ZNPROF_NAME_MAX];
	char   znprof_objroot[PATH_MAX];           /* Object ID Namespc */
	char   znprof_devices[DEVNAMEMAX];         /* Device Glob Str   */
	char   znprof_prtydev[DEVNAMEMAX];         /* Parity Device     */
	size_t znprof_block_sz;                    /* block size        */
	size_t znprof_bdcon_sz;                    /* Size of the bdesc */
	size_t znprof_nblocks;                     /* Num blocks        */
	size_t znprof_sect_sz;                     /* Sector Size       */
	int    znprof_sgio;                        /* Use SCSI generic  */
	size_t znprof_sgio_sz;                     /* SG block size     */
	int    znprof_dio;                         /* Use direct I/O    */
	int    znprof_virtual;                     /* Disk is a file    */
	size_t znprof_ndisks;                      /* Number of disks needed */
	char **znprof_devlist;			   /* List of glob matches */
	u64    znprof_setuuid;
};
typedef struct zest_node_profile znode_prof_t;

struct zest_node {
	char             znode_name[ZNODE_NAME_MAX];  /* gethostname()      */
	char             znode_prof[ZNPROF_NAME_MAX]; /* My profile's name  */
	u64              znode_set_uuid;
	u64              znode_twin_set_uuid;
	ssize_t          znode_ndisks_found;          /* # of disks found   */
	zest_node_id_t   znode_id;                    /* My numeric ID      */
	zest_node_id_t   znode_twin_id;	              /* Failover twin's ID */
	znode_prof_t    *znode_profile;               /* Pointer to my prof */
	struct zlist_head znode_disks;                 /* list of avail dsks */
	struct zlist_head znode_twin_disks;            /* list of twins dsks */
};
typedef struct zest_node znode_t;

extern znode_t      *zestNodeInfo;
extern znode_prof_t *zestNodeProfile;
extern char         *zestHostname;

static inline void
znode_profile_dump(void)
{
	znode_prof_t *z = zestNodeProfile;
	int i;

	zwarnx("\nNode Profile: ;%s;\n\tDeviceGlob ;%s;",
	       z->znprof_name, z->znprof_devices);

	for (i=0; i < zestNodeInfo->znode_ndisks_found; i++)
		zwarnx("\n\t\tDevice%03d: %s",
		       i, z->znprof_devlist[i]);
	zwarnx("\n\tBlockSize %zu\n"
	    "\tSectorSize %zu\n"
	    "\tBdescConSize %zu\n"
	    "\tNumBlocks %zu\n"
	    "\tSGIO %d",
	       z->znprof_block_sz,
	       z->znprof_sect_sz,
	       z->znprof_bdcon_sz,
	       z->znprof_nblocks,
	       z->znprof_sgio);
}

/*
 * Macro for obtaining the configuration
 */

int run_yacc(const char *config_file, const char *prof);

#define zestGetConfig run_yacc

#endif /* HAVE_ZCONFIG_INC */
