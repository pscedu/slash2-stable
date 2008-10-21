/* $Id$ */

#include <err.h>

#include "fio_sym.h"
#include "fio.h"

GROUP_t *
find_group(int mype, int *start_pe)
{
	struct list_head *tmp;
	GROUP_t          *group;
	int               total = 0;

	list_for_each(tmp, &groupList) {
		group = list_entry(tmp, GROUP_t, group_list);

		if (mype < (total + group->num_pes)) {
			if (start_pe != NULL)
				*start_pe = total;
			return group;
		}
		total += group->num_pes;
	}
	WARN("Invalid PE #%d\n", mype);
	ASSERT(0);
}

/*
 * Given a group and a pe #, determine how many files
 *  this process will handle.  If files_per_pe is
 *  specified then short circuit.
 */
int
get_nfiles(IOT_t *iot, GROUP_t *mygroup)
{
	int overflow, nfiles;
	int start_pe = (mygroup->work_pe + GETSKEW) % mygroup->num_pes;
	int thispe;

	if ((mygroup->files_per_pe) &&
	    (mygroup->files_per_pe > 0))
		return mygroup->files_per_pe;

	if ( !ACTIVETYPE(FIO_SAMEFILE) ) {
		nfiles = (int)(mygroup->files_per_dir / mygroup->num_pes);
		overflow = mygroup->files_per_dir % mygroup->num_pes;

	} else {
		nfiles = mygroup->files_per_dir;
		overflow = 0;
	}

	DEBUG(D_CONF, "num_files %d overflow %d\n",
	nfiles, overflow);

	if (iot->mytest_pe == start_pe) {
		/* i'm the first pe of the group! */
		thispe = 0;

	} else if ((iot->mytest_pe - start_pe) < 0) {
		/* a pe before the the worker_pe */
		thispe = mygroup->num_pes - start_pe + iot->mytest_pe;

	} else {
		thispe = iot->mytest_pe - start_pe;
	}

	if (overflow && (thispe <= overflow))
		nfiles++;

	return nfiles;
}

void
init_log_buffers(IOT_t *iot)
{
	int          num_log_ops, i, tree_size;
	size_t       oplog_size;
	GROUP_t     *mygroup = iot->mygroup;
	IOTESTLOG_t *iolog   = &iot->io_log;

	bzero(iolog, sizeof(IOTESTLOG_t));


	for (i=mygroup->tree_depth, tree_size=0; i > -1; i--) {
		tree_size += pow(mygroup->tree_width, i);
	}
	/* multiply by the number of operations
	 *  per file, number of routines, and divide
	 *  by the number of pes.  Lastly add in the
	 *  entries for directory ops and a 'fudge factor'
	 *  to handle rounding.
	 */
	num_log_ops  = tree_size;
	num_log_ops *= mygroup->files_per_dir;
	num_log_ops *= mygroup->current_iotest->num_routines;
	num_log_ops /= mygroup->num_pes;
	num_log_ops += tree_size + 10;

	DEBUG(D_OUTPUT, "num_routines %d num_log_ops %d files_per_dir %d\n",
	    mygroup->current_iotest->num_routines,
	    num_log_ops, mygroup->files_per_dir);

	oplog_size             = num_log_ops * OPLOGSZ;
	iolog->iolog_oplog_cnt = num_log_ops;
	iolog->iolog_oplog     = iolog_alloc(iolog, oplog_size);

	DEBUG(D_OUTPUT, "iolog %p sz = %zu num_log_ops = %d size %zu\n",
	    iolog->iolog_oplog, iolog->iolog_size, num_log_ops, oplog_size);

	bzero(iolog->iolog_oplog, oplog_size);

	iot->op_log = iolog->iolog_oplog;

	return;
}

/*
 * this macro should return the maximum number of
 *  files needed to handle group 'g'
 */
#define GETMAXFILES(g) get_nfiles(0, g)

void
dump_groups(void)
{
	struct list_head *tmp;
	GROUP_t          *group;

	list_for_each(tmp, &groupList) {
		group = list_entry(tmp, GROUP_t, group_list);
		DUMP_GROUP(group);
	}
}

void
write_output(IOT_t *iot)
{
	char outpath[PATH_MAX];
	int fd;

	enum bool_t  hole_detect, hole_found;

	int          num_log_ops, i;
	off_t        offset, end;
	GROUP_t     *mygroup = iot->mygroup;
	IOTESTLOG_t *iolog   = &iot->io_log;
	OPLOG_t     *oplog   = iolog->iolog_oplog;

	snprintf(outpath, PATH_MAX,
		 "%s/%s_%s_PE%d.fioout.%d",
		 mygroup->output_path,
		 mygroup->test_name,
		 mygroup->current_iotest->io_testname,
		 iot->mype,
		 iot->macro_iterations);

	if ( (fd = open(outpath, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0 ) {
		fprintf(stderr, "open() %s %s\n", outpath, strerror(errno));
		exit(1);
	}

	for (i=0, hole_detect=hole_found=NO, num_log_ops=0;
	    i < iolog->iolog_oplog_cnt; i++, oplog++) {

		if (oplog->oplog_used) {
			/* make sure nothing goofy happened */
			ASSERT(oplog->oplog_magic == OPLOG_MAGIC);

			if (hole_detect) {
				hole_found = YES;
				WARN("Found a hole at %p\n", oplog);
			}
			num_log_ops++;
			DUMP_OPLOG_ENTRY(oplog);
		} else {
			hole_detect =YES;
		}
	}
	end = num_log_ops * OPLOGSZ;

	ASSERT( write(fd, iolog, IOTESTLOGSZ) == IOTESTLOGSZ );

	oplog = iolog->iolog_oplog;
	for (i=0; i < num_log_ops; i++, oplog++) {
		/*
		 * write out the sublog if one exists
		 */
		if (oplog->sub.oplog_sublog != NULL) {
			/* save the current offset */
			offset = lseek(fd, 0, SEEK_CUR);
			ASSERT(offset >= 0);

			/* go to the end.. */
			lseek(fd, end, SEEK_SET);

			ASSERT( write(fd, oplog->sub.oplog_sublog,
				(OPLOGSZ*oplog->oplog_subcnt))
				== (OPLOGSZ*oplog->oplog_subcnt) );

			free(oplog->sub.oplog_sublog);
			/* after using the oplog pointer,
			 *   overwrite it with the offset in the file
			 *   where the sublog data now resides
			 */
			oplog->sub.oplog_offset = end;

			/* get the new 'end' of file */
			end = lseek(fd, 0, SEEK_END);

			/* go back to where it was before the
			 *  sublog processing
			 */
			lseek(fd, offset, SEEK_SET);
		}

		ASSERT( write(fd, oplog, OPLOGSZ) == OPLOGSZ );
	}

	close(fd);
	return;
}

void
print_pe_map(struct io_toolbox *iot)
{
	// if i'm the first process then print the map
#ifdef CATAMOUNT
	if ( !iot->mype ) {
		DEBUG("i'm pe 0!\n");
		int i, pid;
		int npes = cnos_get_size();
		// get the nidpid map for this process..
		DEBUG("before malloc\n");
		cnos_nidpid_map_t *map ;// = malloc( sizeof(cnos_nidpid_map_t) * npes);
		DEBUG("after malloc\n");
		(int)cnos_get_nidpid_map( &map );
		DEBUG("after getnidpi\n");

		for ( i=0; i < npes; i++ ) {
			DEBUG("PE %d = NID 0x%x\n", i, map[i].nid);
		}
	}
#endif
	return;
}

IOT_t *
init_pe(int mype)
{
	IOT_t       *iot;
	GROUP_t     *mygroup;
	struct stat  stb;
	int          fd;

	iot = malloc(sizeof(IOT_t));
	ASSERT(iot != NULL);

	bzero(iot, sizeof(IOT_t));

	mygroup = find_group(mype, NULL);
	ASSERT(mygroup != NULL);

	iot->mygroup = (GROUP_t *)mygroup;

	/* absolute pe # */
	iot->mype        = mype;
	iot->debug_flags = mygroup->debug_flags;

	/* relative pe # within the group */
	iot->mytest_pe  = iot->mype % mygroup->num_pes;

	iot->num_blocks = (mygroup->file_size) / (mygroup->block_size);

	if ( ACTIVETYPE(FIO_SAMEFILE) &&
	    (ACTIVETYPE(FIO_SEEKOFF) || ACTIVETYPE(FIO_INTERSPERSE)) )
		iot->num_blocks /= mygroup->num_pes;

	DEBUG(D_CONF, "rank = %d testpe %d num_blocks %d\n",
	    iot->mype, iot->mytest_pe, iot->num_blocks);

#ifdef CATAMOUNT
	(void)print_pe_map();
#endif

	if ( stat(mygroup->test_path, &stb) == -1)
		err(1, "stat %s", mygroup->test_path);

	if (S_ISDIR(stb.st_mode)) {
		if (chdir(mygroup->test_path) == -1)
			err(1, "chdir %s", mygroup->test_path);

		// is !SAMEDIR go into a PE specific dir
#if 0
		if ( !ACTIVETYPE(FIO_SAMEDIR) ) {
			char buf[16];

			bzero(buf, 16);
			snprintf(buf, 16, "%d", FIO_DIR_PREFIX, iot->mype);

			if ( mkdir(buf, 0755) && (errno != EEXIST) ) {
				fprintf(stderr, "mkdir() ;%s; %s",
				    mygroup->test_path, strerror(errno));
				exit(1);
			}
			if ( chdir(buf) ) {
				fprintf(stderr, "chdir() ;%s; %s",
				    mygroup->test_path, strerror(errno));
				exit(1);
			}
		}
#endif

	} else {
		errno = ENOTDIR;
		err(1, "%s", mygroup->test_path);
	}

	/*
	 * Initialize the read and write buffers - both are
	 *  needed to do read checksumming..
	 */
	if (mygroup->block_size) {
		iot->bdesc.buffer_size = iot->rd_bdesc.buffer_size = mygroup->block_size;
		iot->bdesc.buffer      = malloc(iot->bdesc.buffer_size);
		iot->rd_bdesc.buffer   = malloc(iot->bdesc.buffer_size);

		ASSERT( (iot->bdesc.buffer    != NULL) &&
			(iot->rd_bdesc.buffer != NULL) );

		init_buffer(&iot->bdesc,    iot->mype);
		init_buffer(&iot->rd_bdesc, iot->mype);
		compare_buffer(&iot->bdesc, &iot->rd_bdesc);
	}

	if (stderr_redirect) {
		char tf[PATH_MAX];

		snprintf(tf, PATH_MAX, "%s.%d",
			 stderr_fnam_prefix, iot->mype);

		fd = open(tf, O_TRUNC | O_CREAT | O_WRONLY, 0666);

		ASSERTPE(fd >= 0);
		ASSERTPE(dup2(fd, 2) == 2);
		DEBUG(D_OUTPUT, "Opened stderr file %s\n", tf);
	}

	return iot;
}

/*
 * this call takes on 2 forms.  when called
 *  via mpi, each cpu runs this to create it's
 *  local communication group.  when called
 *  from pthreads - the master pe does all the
 *  work.
 */
void
init_barriers(int mype)
{
#ifdef MPI
	GROUP_t *mygroup;
	MPI_Group group_world;
	int *iptr, start_pe, i, rc;
	mygroup = find_group(mype, &start_pe);
	ASSERT(mygroup != NULL);

	iptr = malloc(sizeof(int) * mygroup->num_pes);
	ASSERT(iptr != NULL);

	for (i=0; i < mygroup->num_pes; i++)
		iptr[i] = start_pe + i;

	rc = MPI_Comm_group (MPI_COMM_WORLD, &group_world);

	ASSERTMSG(rc == MPI_SUCCESS,
			"MPI_Group_incl() failed rc = %d\n", rc);

	rc = MPI_Group_incl(group_world, mygroup->num_pes,
					iptr, &mygroup->group);

	ASSERTMSG(rc == MPI_SUCCESS,
			"MPI_Group_incl() failed rc = %d\n", rc);

	rc = MPI_Comm_create(MPI_COMM_WORLD, mygroup->group,
					 &mygroup->group_barrier);

	ASSERTMSG(rc == MPI_SUCCESS,
			"MPI_Comm_create() failed rc = %d\n", rc);

	free(iptr);

#elif PTHREADS
	struct list_head *tmp;
	GROUP_t          *group;

	list_for_each(tmp, &groupList) {
		group = list_entry(tmp, GROUP_t, group_list);
		barrier_init(&group->group_barrier, group->num_pes);
	}
#endif
	return;
}

void
destroy_barriers(void)
{
	struct list_head *tmp;
	GROUP_t          *group;

	list_for_each(tmp, &groupList) {
		group = list_entry(tmp, GROUP_t, group_list);
#ifdef PTHREADS
		barrier_destroy(&group->group_barrier);
#elif MPI
		WARN("Mpi implementation not complete\n");
#endif
	}
	return;
}

/*
 * Create a buffer which is the 'seeded' for
 *   this PE
 */
void
init_buffer(struct buffer_t *bdesc, int id)
{
	long int *buf_long_ints = (long int *)bdesc->buffer;
	size_t    i             = bdesc->buffer_size / LONGSZ;
	int t;

	bzero(bdesc->buffer, bdesc->buffer_size);

	/* seed the random number generator */
	srand48_r(id, &bdesc->rand_data);

	for (t=0; t < i; t++){
		lrand48_r(&bdesc->rand_data, &buf_long_ints[t]);
		//BDEBUG("PE_%d %d %lu\n", id, t,  buf_long_ints[t]);
	}
	return;
}

int
do_creat(struct io_toolbox *iot)
{
	GROUP_t *mygroup = iot->mygroup;
	OPLOG_t *op_tmp  = iot->op_log;

	if (ACTIVETYPE(FIO_SAMEFILE)) {
		// first node creates the file
		if (iot->mype == 0) {
			DEBUG(D_DTREE, "node 0 is creating ;%s;\n",
			    iot->mypath);
			CREAT;

			TPRINT(iot->times[CREAT_clk + 1],
			 "cr_op %011.6f '%s'\n",
			 op_tmp->oplog_time, iot->mypath);

		} else
			BARRIER;
	} else {
		// all nodes create unique file
		DEBUG(D_DTREE, "creat '%s'\n", iot->mypath);
		CREAT;

		TPRINT(iot->times[CREAT_clk + 1],
		 "cr_op %011.6f '%s'\n",
		 op_tmp->oplog_time, iot->mypath);
	}

	APP_BARRIER;
	return 0;
}

int
do_open(struct io_toolbox *iot)
{
	GROUP_t *mygroup = iot->mygroup;
	OPLOG_t *op_tmp  = iot->op_log;
	enum CLOCKS clk;

	if (iot->param & O_RDWR) {
		RDWROPEN;
		clk = RDWROPEN_clk;

	} else if (iot->param & O_WRONLY) {
		WROPEN;
		clk = WROPEN_clk;

	} else if (iot->param & O_APPEND) {
		APPOPEN;
		clk = APPOPEN_clk;

	} else if (iot->param == O_RDONLY) {
		RDOPEN;
		clk = RDOPEN_clk;

	} else ASSERT(1);

	DEBUG(D_DTREE, "opened ;%s;\n",
	iot->mypath);

	if ( ACTIVETYPE(FIO_SEEKOFF) ) {
		SEEKOFF;
	}

	TPRINT(iot->times[clk + 1],
	 "op_op %011.6f '%s'\n",
	 op_tmp->oplog_time, iot->mypath);

	BARRIER;
	return 0;
}

int
do_unlink(struct io_toolbox *iot)
{
	struct test_group_t *mygroup = iot->mygroup;

	OPLOG_t *op_tmp = iot->op_log;
	iot->unlink = 1;
#if 0
	FSTAT;
	if (iot->stb.st_ino != iot->stb_unlink.st_ino) {
		WARN("unlink failed.. inodes don't match!\n");
		return -1;
	}
#endif
	BARRIER;
	STARTWATCH(UNLINK_clk);
	ASSERT(!unlink(iot->mypath));
	STOPWATCH(UNLINK_clk);
	TPRINT(iot->times[UNLINK_clk + 1],
	 "ul_op %011.6f '%s'\n",
	 op_tmp->oplog_time, iot->mypath);
	return 0;
}

int
do_io(IOT_t *iot, int op)
{
	GROUP_t     *mygroup = iot->mygroup;
	OPLOG_t     *op_log_save, *sublog, *op_tmp;
	IOTESTLOG_t *iolog;
	size_t       logsz = iot->num_blocks * OPLOGSZ;
	int          rc = 0;
	enum bool_t  checksum_ok = YES;
	enum CLOCKS  clk_type;

	op_tmp = op_log_save = sublog = NULL;

	if (op)
		clk_type = WRITE_clk;
	else
		clk_type = READ_clk;

	DEBUG(D_BLOCK, "Entry: do_io(%d) blocks %d\n",
	iot->myfd, iot->num_blocks);

	BARRIER;
	/*
	 * set the op log to write the sub log
	 *   for this write op if we're interested
	 *   in timing each block io.
	 */
	op_log_save = iot->op_log;

	if ( ACTIVETYPE(FIO_TIME_BLOCK) ) {
		//sublog = iot->op_log->sub.oplog_sublog;
		iolog  = &iot->io_log;

		sublog = iolog_alloc(iolog, logsz);

		bzero(sublog, logsz);

		DEBUG(D_MEMORY, "malloc ok to %p iologsz %zu\n",
		    sublog, iolog->iolog_size);

		/* save this sublog pointer to the
		 *  correct spot in the current oplog
		 */
		op_log_save->sub.oplog_sublog = sublog;

		/* change the current oplog to the sublog
		 *   for the duration of the next loop
		 */
		iot->op_log = sublog;

		DEBUG(D_MEMORY, "malloc'ed %zu, to sub iot->op_log %p old_log %p\n",
		    logsz, iot->op_log, op_log_save);
	}

	STARTWATCH(clk_type);

	for ( iot->bdesc.block_number = 0;
	    iot->bdesc.block_number < iot->num_blocks;
	    iot->bdesc.block_number++ ) {

		DEBUG(D_BLOCK, "Block #%d\n", iot->bdesc.block_number);

		if ( ACTIVETYPE(FIO_INTERSPERSE) )
			INTERSPERSE_SEEK;

		if ( ACTIVETYPE(FIO_TIME_BLOCK) ) {
			STARTWATCH(BLOCK_clk);
		}

		if (clk_type == WRITE_clk)
			SWABBUFFER(&iot->bdesc);

		/*
		 * need to handle all the return code
		 *  cases here (ie partial reads)
		 */
		if (clk_type == WRITE_clk)
			rc = write(iot->myfd, iot->bdesc.buffer,
			    iot->bdesc.buffer_size);

		else
			rc = read(iot->myfd, iot->bdesc.buffer,
			    iot->bdesc.buffer_size);

		DEBUG(D_BLOCK, "IO to fd %d rc == %d\n",
		iot->myfd, rc);

		ASSERTPE(rc >= 0);

		if ( (clk_type == WRITE_clk) &&
		    ACTIVETYPE(FIO_FSYNC_BLOCK) ) {
			ASSERT( !fsync(iot->myfd) );
		}

		if ( ACTIVETYPE(FIO_VERIFY) ) {
			CLEANBUFFER(&iot->bdesc);

			if (clk_type == READ_clk) {
				if (compare_buffer(&iot->bdesc,
				    &iot->rd_bdesc) != 0)
					checksum_ok = NO;
			}
		}
		/*
		 * STOPWATCH is going to barrier if TIME_BLOCK_BARRIER
		 *  is set.  If it's not set then BARRIER
		 */
		if ( ACTIVETYPE(FIO_TIME_BLOCK) ) {
			char *op_type = "bl_rd";
			op_tmp = iot->op_log;

			STOPWATCH(BLOCK_clk);

			if (clk_type == WRITE_clk)
				op_type = "bl_wr";

			TPRINT(iot->times[BLOCK_clk + 1],
			 "%s %011.6f MB/s %011.6f block# %d bwait %09.6f\n",
			 op_type, op_tmp->oplog_time,
			 (float)(((mygroup->block_size)/op_tmp->oplog_time)/1048576),
			 iot->bdesc.block_number,
			 op_tmp->oplog_barrier_time);

		} else if ( ACTIVETYPE(FIO_BLOCK_BARRIER) ) {
			BARRIER;
		}
		/*
		 * Manage block_freq here
		 */
		if (mygroup->block_freq.tv_sec)
			sleep(mygroup->block_freq.tv_sec);

		if (mygroup->block_freq.tv_usec)
			usleep(mygroup->block_freq.tv_usec);
	}
	/* stopwatch increments iot->op_log,
	 *  we'd like to save the checksum status
	 *  to this oplog AFTER we've stopped the
	 *  clock which is why the oplog ptr
	 *  needs to be saved
	 */
	if ( ACTIVETYPE(FIO_TIME_BLOCK) ) {
		/*
		 * block io is done, replace the 'real'
		 *  oplog pointer
		 */
		op_tmp = iot->op_log = op_log_save;

	} else {
		/* these pointers had better be the same */
		ASSERT(op_log_save == iot->op_log);
		op_tmp = op_log_save;
	}

	STOPWATCH(clk_type);

	if (clk_type == READ_clk) {
		TPRINT(iot->times[READ_clk + 1],
		 "rd_op %011.6f MB/s %011.6f '%s' chksum_ok %d\n",
		 op_tmp->oplog_time,
		 (float)(((mygroup->block_size*iot->bdesc.block_number)/op_tmp->oplog_time)/1048576),
		 iot->mypath, checksum_ok);

		op_tmp->oplog_checksum_ok = checksum_ok;

		if (checksum_ok == NO)
			WARN("Bad Checksum File '%s'\n",
			    iot->mypath);
		else
			DEBUG(D_BLOCK, "Good Checksum File '%s'\n",
			    iot->mypath);

	} else
		TPRINT(iot->times[WRITE_clk + 1],
		 "wr_op %011.6f MB/s %011.6f '%s'\n",
		 op_tmp->oplog_time,
		 (float)(((mygroup->block_size*iot->bdesc.block_number)/op_tmp->oplog_time)/1048576),
		 iot->mypath);

	return 0;
}

int
do_write(IOT_t *iot)
{
	return (do_io(iot, 1));
}

int
do_read(IOT_t *iot)
{
	return (do_io(iot, 0));
}

int
do_trunc(IOT_t *iot)
{
	GROUP_t *mygroup = iot->mygroup;

	ASSERTPE(!fstat(iot->myfd, &iot->stb));

	TRUNC;
	return 0;
}

int
do_close(IOT_t *iot)
{
	struct test_group_t *mygroup = iot->mygroup;
	OPLOG_t *op_tmp = iot->op_log;

	CLOSE;

	TPRINT(iot->times[CLOSE_clk + 1],
	 "cl_op %011.6f '%s'\n",
	 op_tmp->oplog_time, iot->mypath);

	return 0;
}

int
do_rename(IOT_t *iot)
{
	//struct test_group_t *mygroup = iot->mygroup;
	return 0;
}

int
do_link(IOT_t *iot)
{
	//struct test_group_t *mygroup = iot->mygroup;
	return 0;
}

int
do_stat(IOT_t *iot)
{
	struct test_group_t *mygroup = iot->mygroup;
	OPLOG_t *op_tmp = iot->op_log;

	STAT;

	TPRINT(iot->times[STAT_clk + 1],
	 "st_op %011.6f '%s'\n",
	 op_tmp->oplog_time, iot->mypath);

	return 0;
}

int
do_fstat(IOT_t *iot)
{
	struct test_group_t *mygroup = iot->mygroup;
	OPLOG_t *op_tmp = iot->op_log;

	FSTAT;

	TPRINT(iot->times[FSTAT_clk + 1],
	 "fs_op %011.6f '%s'\n",
	 op_tmp->oplog_time, iot->mypath);

	return 0;
}

void *
worker(void *arg)
{
	int f, i, j, k, l, m, num_files, rc = 0;
	struct symtable_t   *e;
	int                 *mype        = (int*)arg;
	struct io_toolbox   *iot         = init_pe(*mype);
	GROUP_t             *mygroup     = iot->mygroup;
	struct io_routine_t *io_routines = mygroup->iotests;
	struct list_head    *tmp, *tmp1;
	DIR_t               *current_dir = NULL;

	ASSERT(iot != NULL);
	ASSERT(io_routines != NULL);

	for (i=0, j=0, k=0; i < mygroup->iterations; i++) {

		iot->macro_iterations = i;

		num_files = get_nfiles(iot, mygroup);

		for (l=0; l < mygroup->num_iotests; l++) {
			/* this barrier is important because
			 *  it protects  against some pe's
			 *  loop wrapping before the others are ready
			 */
			BARRIER;

			mygroup->current_iotest = &io_routines[l];

			iot->current_test       = l;
			iot->current_width      = 0;
			iot->current_depth      = 0;
			iot->micro_iterations   = 0;

			init_log_buffers(iot);

			/* create the initial path */
			make_abs_pathname(iot);

			if (WORKPE) {
				/* initialize the phantom directory tree */
				mygroup->dirRoot = malloc(sizeof(DIR_t));
				bzero(mygroup->dirRoot, sizeof(DIR_t));
				INIT_LIST_HEAD(&mygroup->dirRoot->dir_stack);
				INIT_LIST_HEAD(&mygroup->dirStack);
				list_add(&mygroup->dirRoot->dir_stack, &mygroup->dirStack);
				MKDIR;
			}
			/* everyone wait here for the creation of the
			 *  root test dir
			 */
			BARRIER;

			if ( !ACTIVETYPE(FIO_SAMEDIR) ) {
				make_pe_specific_dir(iot);
				MKDIR;
				BARRIER;
			}

			DEBUG(D_IOFUNC, "BEGIN Test '%s' mypath '%s' nfiles %d\n",
			io_routines[l].io_testname, iot->mypath, num_files);

			do {
				list_for_each_safe(tmp, tmp1, &mygroup->dirStack) {
					current_dir = list_entry(tmp, DIR_t, dir_stack);
					break;
				}

				if (current_dir == NULL) {
					WARN("Null current_dir\n");
					break;
				}

				BARRIER;

				/* get the parent dir */
				DEBUG(D_DTREE, "current_dir %p root %p path %s test_done %d\n",
					current_dir, mygroup->dirRoot,
					iot->mypath, current_dir->test_done);
				BARRIER;

				if (!current_dir->test_done) {

					for (f=0; f < num_files; f++) {
						iot->micro_iterations = f;

						(void)make_fnam(iot);

						for (m=0; m < io_routines[l].num_routines ; m++) {
							e = get_symbol(io_routines[l].io_routine[m]);

							DEBUG(D_IOFUNC, "name '%s' path '%s'\n",
							    e->name, iot->mypath);

							iot->param = e->param;
							rc = e->io_func(iot);
							if (rc)
								WARN("Error rc = %d, %s\n",
							    	    rc, strerror(errno));
						}
						(void)clear_fnam(iot);
					}

					BARRIER;
					if (WORKPE) {
						current_dir->test_done = 1;
					}
					BARRIER;

					/*
					 * Manage test_freq here
					 */
					if (mygroup->test_freq.tv_sec)
						sleep(mygroup->test_freq.tv_sec);

					if (mygroup->test_freq.tv_usec)
						usleep(mygroup->test_freq.tv_usec);

				} else {
					DEBUG(D_DTREE, "current_dir %p has been run\n",
					current_dir);
				}
				/* does this node have all of it's children
				 *  or is it at max height?
				 */
				if ((iot->current_depth >= mygroup->tree_depth)) {

					(void)pop_dirstack(iot);

				} else {
					/* descend to a lower part of the tree */
					iot->current_width = current_dir->child_cnt;

					BARRIER;
					if (WORKPE) {
						current_dir->child_cnt++;
					}
					BARRIER;

					(void)push_dirstack(iot);
				}
			} while (!list_empty(&mygroup->dirStack));

			write_output(iot);

		} /* for num_iotests */
	}   /* for iterations  */

	if (rc)
		WARN("%d tests FAILED\n", rc);

	return NULL;
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: fio [-Dd] [-i conf] [-o errlog]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
#ifdef PTHREADS
	struct list_head *tmp;
	GROUP_t          *group = NULL;
	int               rc, i, c, fd;

#elif  MPI
	int  mype = 0;
	(int)MPI_Init(&argc, &argv);
#endif

	TOTAL_PES = 0;

	while (((c = getopt(argc, argv, "dDi:o:")) != -1))
		switch (c) {
		case 'i':
			fd = open(optarg, O_RDONLY);
			if (fd < 0)
				err(1, "%s", optarg);
			if (dup2(fd, STDIN_FILENO) == -1)
				err(1, "dup2");
			break;
		case 'o':
			stderr_redirect     = 1;
			stderr_fnam_prefix  = strdup(optarg);
			break;
		case 'd':
			fio_global_debug = 1;
			break;
		case 'D':
			fio_lexyacc_debug = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	run_yacc();
	dump_groups();

#ifdef MPI
	/* get the total number of pes */
	MPI_Comm_size(MPI_COMM_WORLD, &TOTAL_PES);
	MPI_Comm_rank(MPI_COMM_WORLD, &mype);
	/* no master pe.. everyone runs the worker */
	init_barriers(mype);
	worker(&mype);
	MPI_Finalize();

#elif PTHREADS
	init_barriers(0);
	list_for_each(tmp, &groupList) {
		group = list_entry(tmp, GROUP_t, group_list);
		/*
		 * note that every group has a pe 0... the mpi port
		 *  is going to have to handle this a bit differently
		 *  since pe assignment is implicit
		 */
		for (i=0; i < group->num_pes; i++) {
			group->threads[i].mype = i + TOTAL_PES;

			BDEBUG("about to create %d\n", group->threads[i].mype);

			rc = pthread_create(&group->threads[i].thread_id, NULL,
				worker, &group->threads[i].mype);
			ASSERT(!rc);
		}

		TOTAL_PES += group->num_pes;
	}

	for (i=0; i < TOTAL_PES; i++) {
		rc = pthread_join(group->threads[i].thread_id, NULL);
	}
#endif

	exit(0);
}
