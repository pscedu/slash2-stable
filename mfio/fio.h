/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _FIO_H_
#define _FIO_H_

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/fcntl.h"
#include "pfl/types.h"
#include "pfl/list.h"
#include "pfl/alloc.h"

#ifdef HAVE_LIBPTHREAD
#include <pthread.h>

#include "pfl/pthrutil.h"

extern pthread_barrier_t barrier;

#elif defined(HAVE_MPI)
# include <mpi.h>
#endif

#ifdef CATAMOUNT
# include <catamount/cnos_mpi_os.h>
#endif

#define MAXROUTINE	16

#define FIO_DIR_PREFIX	"fio_d."
#define FIO_FILE_PREFIX	"fio_f."

extern int   TOTAL_PES;
extern int   fio_global_debug, fio_lexyacc_debug;
extern int   stderr_redirect;
extern char *stderr_fnam_prefix;

/*
 *  Test type bits
 */
enum TEST_OPTIONS {
	FIO_SAMEDIR		= 1 << 0,  // all fsops happen in samedir
	FIO_BLOCK_BARRIER	= 1 << 1,  // time each block (read/write)
	FIO_BARRIER		= 1 << 2,  // barrier stuff with mpi
	FIO_BARRDBG		= 1 << 3,
	FIO_SAMEFILE		= 1 << 4,  // io goes to same file..
	FIO_STAGGER		= 1 << 5,  // each process runs independently of other
	FIO_RANDOM		= 1 << 6,
	FIO_VERIFY		= 1 << 7,  // perform blocklevel checksumming
	FIO_WRITE		= 1 << 8,
	FIO_READ		= 1 << 9,
	FIO_SEEKOFF		= 1 << 10,
	FIO_TIME_BARRIER	= 1 << 11,
	FIO_THRASH_LOCK		= 1 << 12,
	FIO_TIME_BLOCK		= 1 << 13,
	FIO_FSYNC_BLOCK		= 1 << 14,
	FIOT_INTERSPERSE	= 1 << 15,
	FIO_APP_BARRIER		= 1 << 16
};

#define ACTIVETYPE(t)		(((t) & mygroup->test_opts) ? 1 : 0)

#define GETSKEW			(ACTIVETYPE(FIO_THRASH_LOCK) ? 1 : 0)

/*
 * This construct is used to provide protection
 *   of global memory regions for the pthreads port.
 *   MPI doesn't need this since every mpi process
 *   has its own heap
 */
#ifdef  HAVE_MPI
#  define WORKPE 1
#elif defined(HAVE_LIBPTHREAD)
#  define WORKPE (iot->mytest_pe == mygroup->work_pe)
#endif

enum CLOCKS {
	WRITE_clk	= 0,
	READ_clk	= 2,
	CREAT_clk	= 4,
	RDOPEN_clk	= 6,
	UNLINK_clk	= 8,
	RENAME_clk	= 10,
	LINK_clk	= 12,
	BLOCK_clk	= 14, // time each block action
	STAT_clk	= 16,
	FSTAT_clk	= 18,
	CLOSE_clk	= 20,
	WROPEN_clk	= 22,
	APPOPEN_clk	= 24,
	BARRIER_clk	= 26,
	RDWROPEN_clk	= 28,
	TRUNC_clk	= 30,
	MKDIR_clk	= 32,
	FSYNC_clk	= 34
};
#define NUMCLOCKS	36

enum path_depth {
	KEEP_DIR	= 1,
	CHANGE_DIR	= 2,
	ASCEND_DIR	= 3
};

enum debug_channels {
	FIO_DBG_BLOCK	= 1 << 1,
	FIO_DBG_MEMORY	= 1 << 2,
	FIO_DBG_BARRIER	= 1 << 3,
	FIO_DBG_DTREE	= 1 << 4,
	FIO_DBG_SYMTBL	= 1 << 5,
	FIO_DBG_CONF	= 1 << 6,
	FIO_DBG_OUTPUT	= 1 << 7,
	FIO_DBG_IOFUNC	= 1 << 8,
	FIO_DBG_BUFFER	= 1 << 9
};

enum debug_channels_short {
	D_BLOCK		= 1 << 1,
	D_MEMORY	= 1 << 2,
	D_BARRIER	= 1 << 3,
	D_DTREE		= 1 << 4,
	D_SYMTBL	= 1 << 5,
	D_CONF		= 1 << 6,
	D_OUTPUT	= 1 << 7,
	D_IOFUNC	= 1 << 8,
	D_BUFFER	= 1 << 9
};

#define MAXTESTS	16

#define LONGSZ		sizeof(long)

struct op_log {
	int		oplog_magic;
	enum CLOCKS	oplog_type;
	bool		oplog_used;
	bool		oplog_checksum_ok;
	int		oplog_subcnt;
	float		oplog_time;
	float		oplog_barrier_time;
	union {
		struct op_log *oplog_sublog;
		off_t	oplog_offset;
	} sub;
};
typedef struct op_log OPLOG_t;

#define OPLOGSZ		sizeof(OPLOG_t)
#define OPLOG_MAGIC	0x6e1eafe9

struct iotest_log {
	char		 iolog_tname[PATH_MAX];
	int		 iolog_pe;
	int		 iolog_iteration;
	OPLOG_t		*iolog_oplog;
	int		 iolog_oplog_cnt;
	size_t		 iolog_size;
};
typedef struct iotest_log IOTESTLOG_t;

#define IOTESTLOGSZ	sizeof(IOTESTLOG_t)

struct buffer {
	void		*buffer;
	size_t		 buffer_size;
	size_t		 data_size;
	off_t		 current_offset;
	int		 block_number;
};
typedef struct buffer BUF_t;

struct io_toolbox {
	void		*mygroup;				/* pointer back to my group */
	struct buffer	 bdesc;
	struct buffer	 rd_bdesc;
	struct timeval	 times[NUMCLOCKS];
	struct stat	 stb;
	struct stat	 stb_unlink;
	IOTESTLOG_t	 io_log;
	OPLOG_t		*op_log;
	uint64_t	 mysize;
	char		 mypath[PATH_MAX];
	char		*myfnam;
	char		*logBuf;
	char		*logBufPtr;
	int		 micro_iterations;			/* how many times..    */
	int		 macro_iterations;
	int		 myfd;
	int		 mype;
	int		 mytest_pe;
	int		 param;
	int		 unlink;
	int		 num_blocks;
	int		 path_len;
	int		 path_isdir;
	enum path_depth	 depth;
	int		 filenum;
	int		 current_depth;
	int		 current_width;
	int		 current_test;
	int		 barrier_cnt;
	int		 debug_flags;
};
typedef struct io_toolbox IOT_t;

struct io_routine {
	int		 num_routines;
	char		 io_testname[PATH_MAX];
	char		*io_routine[MAXROUTINE];
};
typedef struct io_routine IOROUTINE_t;

struct io_thread {
#ifdef HAVE_LIBPTHREAD
	pthread_t	 thread_id;
#endif
	int		 mype;				/* mype is the absolute pe */
	int		 rc;
};
typedef struct io_thread THREAD_t;

struct dirnode {
	struct psclist_head dir_stack_lentry;
	int		 depth;
	int		 child_cnt;
	int		 test_done;
	char		*path_comp;
};
typedef struct dirnode DIR_t;

struct test_group {
	struct psclist_head group_lentry;
	struct io_routine  iotests[MAXTESTS];
	struct io_routine *current_iotest;

	int		 num_iotests;

	struct timeval	 test_freq;			/* how often io occurs	*/
	struct timeval	 block_freq;			/* sleep in between block */
	char		 test_name[PATH_MAX];		/* name of the group	*/
	char		 test_path[PATH_MAX];		/* starting path	*/
	char		 output_path[PATH_MAX];		/* starting path	*/
	char		 test_filename[PATH_MAX];
	uint64_t	 block_size;			/* chunk size		*/
	uint64_t	 file_size;			/* file size		*/
	int		 num_pes;			/* num of processes	*/
	int		 work_pe;			/* pe next up for work	*/
	int		 iterations;
	int		 test_opts;			/* what this test does	*/
	int		 files_per_pe;
	int		 files_per_dir;
	int		 thrash_lock;
	int		 tree_depth;
	int		 tree_width;
	int		 depth_change;
	int		 debug_flags;
#ifdef HAVE_LIBPTHREAD
	pthread_barrier_t group_barrier;
	THREAD_t	*threads;
#elif defined(HAVE_MPI)
	MPI_Group	 group;
	MPI_Comm	 group_barrier;
#endif
	DIR_t		*dirRoot;
	struct psclist_head dirStack;
};
typedef struct test_group GROUP_t;

extern struct psclist_head	 groupList;
extern GROUP_t		*currentGroup;

#define DEBUG(chan, format, ...) do {					\
	if (iot->debug_flags & (chan)) {				\
		struct timeval tv;					\
									\
		gettimeofday(&tv, NULL);				\
		fprintf(stderr,	PSCPRI_TIMEVAL				\
		    " %s PE_%05d %s() %s, %d :: "format,		\
		    PSCPRI_TIMEVAL_ARGS(&tv), get_dbg_prefix(chan),	\
		    iot->mype, __func__, __FILE__, __LINE__,		\
		    ##__VA_ARGS__);					\
	}								\
} while (0)

#define PRINT(format, ...)						\
	fprintf(stderr, "%s() %s, %d :: "format,			\
	    __func__, __FILE__, __LINE__, ##__VA_ARGS__)

#define TPRINT(tv, format, ...)						\
	fprintf(stderr, PSCPRI_TIMEVAL " PE_%05d %12s() :: " format,	\
	    PSCPRI_TIMEVAL_ARGS(tv), iot->mype, __func__, ##__VA_ARGS__)

#define TPRINTPE TPRINT

#define BDEBUG(format, ...) do {					\
	if (fio_global_debug)						\
		fprintf(stderr, "%s() %s, %d :: "format,		\
		    __func__, __FILE__, __LINE__, ##__VA_ARGS__);	\
} while (0)

#define BDEBUGPE(format, ...) do {					\
	if (fio_global_debug) {						\
		struct timeval tv;					\
									\
		gettimeofday(&tv, NULL);				\
		fprintf(stderr, PSCPRI_TIMEVAL				\
		    " GDBG %s() PE_%05d %s, %d :: "format,		\
		    PSCPRI_TIMEVAL_ARGS(&tv), __func__, iot->mype,	\
		    __FILE__, __LINE__, ##__VA_ARGS__);			\
	}								\
} while (0)

#define CDEBUG(format, ...) do {					\
	if (fio_lexyacc_debug)						\
		fprintf(stderr, "%s() %s, %d :: "format,		\
		    __func__, __FILE__, __LINE__, ##__VA_ARGS__);	\
} while (0)

#define ASSERT(cond) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "ASSERT %s() %s, %d %s\n",		\
		    __func__, __FILE__, __LINE__, strerror(errno));	\
		exit(1);						\
	}								\
} while (0)

#define ASSERTPE(cond) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "ASSERT PE_%d %s() %s, %d %s\n",	\
		    iot->mype, __func__, __FILE__, __LINE__,		\
		    strerror(errno));					\
		exit(1);						\
	}								\
} while (0)

#define ASSERTMSG(cond, format, ...) do {				\
	if (!(cond)) {							\
		fprintf(stderr, "ASSERT %s() %s, %d msg:"format,	\
		    __func__, __FILE__, __LINE__, ##__VA_ARGS__);	\
		exit(1);						\
	}								\
} while (0)

#define WARN(format, ...)						\
	fprintf(stderr, "WARNING %s() %s, %d :: "format,		\
	    __func__, __FILE__, __LINE__, ##__VA_ARGS__)

static inline char *
get_dbg_prefix(int dbg_channel)
{
	switch (dbg_channel) {
	case FIO_DBG_BLOCK:
		return "D_BLOCK";
	case FIO_DBG_MEMORY:
		return "D_MEM";
	case FIO_DBG_BARRIER:
		return "D_BARR";
	case FIO_DBG_DTREE:
		return "D_DTREE";
	case FIO_DBG_SYMTBL:
		return "D_SYMTBL";
	case FIO_DBG_CONF:
		return "D_DCONF";
	case FIO_DBG_OUTPUT:
		return "D_OUT";
	case FIO_DBG_IOFUNC:
		return "D_IOFUNC";
	case FIO_DBG_BUFFER:
		return "D_BUFF";
	}
	return NULL;
}

#define DUMP_BUFFER(b)							\
    DEBUG(D_BUFFER, "buffer_t %p: buffer %p, offset %llu, "		\
	"block_num %d, block_size %i\n",				\
	(b), (b)->buffer, (off_t)(b)->current_offset,			\
	(b)->block_number, (b)->buffer_size)

#define DUMP_GROUP(g) do {						\
	int _i, _j;							\
									\
	BDEBUG("Group '%s' %p\n"					\
	    "\tnum_pes %d\n"						\
	    "\tnum_tests %d\n"						\
	    "\tpath '%s'\n"						\
	    "\tfile_size %"PRIu64"\n"					\
	    "\tblock_size %"PRIu64"\n"					\
	    "\titeratons %d\n"						\
	    "\ttest_opts %d\n"						\
	    "\tfile_per_pe %d\n"					\
	    "\tfile_per_dir %d\n"					\
	    "\ttree_depth %d\n"						\
	    "\ttree_width %d\n"						\
	    "\ttest_freq "PSCPRI_TIMEVAL "\n",				\
	    (g)->test_name,						\
	    (g),							\
	    (g)->num_pes,						\
	    (g)->num_iotests,						\
	    (g)->test_path,						\
	    (g)->file_size,						\
	    (g)->block_size,						\
	    (g)->iterations,						\
	    (g)->test_opts,						\
	    (g)->files_per_pe,						\
	    (g)->files_per_dir,						\
	    (g)->tree_depth,						\
	    (g)->tree_width,						\
	    PSCPRI_TIMEVAL_ARGS(&(g)->test_freq));			\
									\
	for (_i = 0; _i < (g)->num_iotests; _i++) {			\
		BDEBUG("\t%s:\n",					\
		    (g)->iotests[_i].io_testname);			\
		for (_j = 0; _j < (g)->iotests[_i].num_routines; _j++) {\
			BDEBUG("\t\t%s %p:\n",				\
			    (char *)(g)->iotests[_i].io_routine[_j],	\
			    (g)->iotests[_i].io_routine[_j]);		\
		}							\
	}								\
} while (0)

#define STARTWATCH(test) gettimeofday(&(iot->times[test]), NULL)
#define STOPWATCH(test)							\
	do {								\
		gettimeofday(&(iot->times[(test) + 1]), NULL);		\
		log_op((test), iot);					\
	} while (0)

static inline double
calc_run_time(struct timeval *tv1, struct timeval *tv2)
{
	float t;

	t = ( ((tv2->tv_usec/1000000.0) + tv2->tv_sec) -
	    ((tv1->tv_usec/1000000.0) + tv1->tv_sec) );

	return t;
}

static inline char *
clock_2_str(int clck)
{
	switch (clck) {
	case WRITE_clk:   return "WRITE";
	case READ_clk:    return "READ";
	case CREAT_clk:   return "CREAT";
	case RDOPEN_clk:  return "RDOPEN";
	case WROPEN_clk:  return "WROPEN";
	case APPOPEN_clk: return "APPOPEN";
	case UNLINK_clk:  return "UNLINK";
	case RENAME_clk:  return "RENAME";
	case STAT_clk:    return "STAT";
	case FSTAT_clk:   return "FSTAT";
	case BLOCK_clk:   return "BLOCK";
	case CLOSE_clk:   return "CLOSE";
	case BARRIER_clk: return "BARRIER";
	}
	return NULL;
}

/*
 * Barrier macros provide the same interface
 *  regardless of the underlying syncronization
 *  method.
 */

static inline void
_BARRIER(GROUP_t *mygroup, struct io_toolbox *iot)
{
#ifdef HAVE_LIBPTHREAD
	pthread_barrier_wait(&mygroup->group_barrier);
#elif defined(HAVE_MPI)
	MPI_Barrier(mygroup->group_barrier);
#else
	WARN("No barrier support..\n");
#endif
	iot->barrier_cnt++;
	DEBUG(D_BARRIER, "barrier_cnt %d\n", iot->barrier_cnt);
	//  sleep(1);
}

#ifdef CATAMOUNT
# define YY_BARRIER	cnos_barrier()
#endif

#define __BARRIER()	(ACTIVETYPE(FIO_BARRIER) &&			\
			!ACTIVETYPE(FIO_STAGGER) ? _BARRIER(mygroup, iot) : 0)

#define SBARRIER()	(ACTIVETYPE(FIO_BARRIER) ? _BARRIER(mygroup) : 0)

#define BARRIER()	__BARRIER()
#define APP_BARRIER()							\
	do {								\
		if (ACTIVETYPE(FIO_APP_BARRIER))			\
			BARRIER();					\
	} while (0)

// figure out how long we waited at this barrier
#define LOG_BARRIER_WAIT(fn, i) do {					\
	STARTWATCH(BARRIER_clk);					\
	BARRIER;							\
	STOPWATCH(BARRIER_clk);						\
	LOG1(iot, "BW%s\t%#09x %d %.4f\n",				\
	    clock_2_str(BARRIER_clk), fn, i,				\
	    calc_run_time(times[BARRIER_clk], times[BARRIER_clk+1]));	\
} while (0)

#define CREAT do {							\
	APP_BARRIER();							\
	STARTWATCH(CREAT_clk);						\
	iot->myfd = creat(iot->mypath, 0644);				\
	if (iot->myfd == -1) {						\
		WARN("creat %s: %s\n", iot->mypath, strerror(errno));	\
		ASSERTPE(0);						\
	}								\
	STOPWATCH(CREAT_clk);						\
	ASSERT( !close(iot->myfd) );					\
} while (0)

#define TRUNC do {							\
	APP_BARRIER();							\
	STARTWATCH(TRUNC_clk);						\
	ASSERT(!ftruncate(iot->myfd, iot->stb.st_size));		\
	STOPWATCH(TRUNC_clk);						\
} while (0)

#define SEEKOFF do {							\
	iot->mysize = (mygroup->file_size / mygroup->num_pes);		\
	off_t seekv =  iot->mysize * iot->mype;				\
	DEBUG(D_BLOCK, "PE %d, seeking %"PSCPRIdOFFT" bytes\n",		\
	    iot->mype, seekv);						\
	APP_BARRIER();							\
	ASSERT(lseek(iot->myfd, seekv, SEEK_SET) >= 0);			\
} while (0)

#define INTERSPERSE_SEEK do {						\
	off_t seekv;							\
	if (!iot->bdesc.block_number)					\
		seekv = (iot->bdesc.buffer_size * iot->mype);		\
	else								\
		seekv = (iot->bdesc.buffer_size * mygroup->num_pes);	\
	DEBUG(D_BLOCK, "PE %d, seeking %"PSCPRIdOFFT" bytes\n",		\
	    iot->mype, seekv);						\
	ASSERT(lseek(iot->myfd, seekv, SEEK_CUR) >= 0);			\
	DEBUG(D_BLOCK, "PE %d, curr off %"PSCPRIdOFFT"\n",		\
	    iot->mype, lseek(iot->myfd, (off_t)0, SEEK_CUR));		\
} while (0)

#define RDOPEN do {							\
	APP_BARRIER();							\
	STARTWATCH(RDOPEN_clk);						\
	DEBUG(D_DTREE, "about to open %s\n", iot->mypath);		\
	ASSERT( (iot->myfd = open(iot->mypath, O_RDONLY)) >= 0);	\
	STOPWATCH(RDOPEN_clk);						\
} while (0)

#define WROPEN do {							\
	APP_BARRIER();							\
	STARTWATCH(WROPEN_clk);						\
	ASSERT((iot->myfd =						\
	    open(iot->mypath, O_CREAT| O_WRONLY, 0644)) >= 0);		\
	DEBUG(D_DTREE, "opened %s fd = %d\n", iot->mypath, iot->myfd);	\
	STOPWATCH(WROPEN_clk);						\
} while (0)

#define APPOPEN do {							\
	APP_BARRIER();							\
	STARTWATCH(APPOPEN_clk);					\
	ASSERT((iot->myfd =						\
	    open(iot->mypath, O_WRONLY|O_APPEND)) >= 0);		\
	STOPWATCH(APPOPEN_clk);						\
} while (0)

#define RDWROPEN do {							\
	APP_BARRIER();							\
	DEBUG(D_DTREE, "rdwr open path ;%s;\n", iot->mypath);		\
	if ( !ACTIVETYPE(FIO_STAGGER) )					\
		BARRIER();						\
	STARTWATCH(RDWROPEN_clk);					\
	ASSERT( (iot->myfd = open(iot->mypath, O_RDWR)) >= 0);		\
	STOPWATCH(RDWROPEN_clk);					\
} while (0)

#define CLOSE do {							\
	APP_BARRIER();							\
	STARTWATCH(CLOSE_clk);						\
	ASSERT( !close(iot->myfd) );					\
	STOPWATCH(CLOSE_clk);						\
} while (0)

#define MKDIR() do {							\
	APP_BARRIER();							\
	STARTWATCH(MKDIR_clk);						\
	if (mkdir(iot->mypath, 0755) == -1 && errno != EEXIST) {	\
		WARN("mkdir %s failed: %s", iot->mypath,		\
		    strerror(errno));					\
		ASSERTPE(0);						\
	}								\
	STOPWATCH(MKDIR_clk);						\
} while (0)

#define FSTAT do {							\
	APP_BARRIER();							\
	STARTWATCH(FSTAT_clk);						\
	if (iot->unlink) {						\
		ASSERT(!fstat(iot->myfd, &iot->stb_unlink));		\
	} else {							\
		ASSERT(!fstat(iot->myfd, &iot->stb));			\
	}								\
	STOPWATCH(FSTAT_clk);						\
	DEBUG(D_DTREE,							\
	    "%s\t%#09x Filesize = %zu, Inode = %"PSCPRIuINOT"\n",	\
	    clock_2_str(BLOCK_clk), iot->filenum,			\
	    (size_t)iot->stb.st_size, iot->stb.st_ino);			\
} while (0)

#define STAT do {							\
	APP_BARRIER();							\
	STARTWATCH(STAT_clk);						\
	ASSERT(!stat(iot->mypath, &iot->stb));				\
	STOPWATCH(STAT_clk);						\
	DEBUG(D_DTREE,							\
	    "%s\t%#09x Filesize = %zu, Inode = %"PSCPRIuINOT"\n",	\
	    clock_2_str(STAT_clk), iot->filenum,			\
	    (size_t)iot->stb.st_size, iot->stb.st_ino);			\
} while (0)

#define DUMP_OPLOG_ENTRY(e)						\
    BDEBUGPE("Addr %p Type %d Used %d Time %f btime %f sublog %p\n",	\
	(e), (e)->oplog_type, (e)->oplog_used, (e)->oplog_time,		\
	(e)->oplog_barrier_time, (e)->sub.oplog_sublog)

static inline void
log_op(int op_type, IOT_t *iot)
{
	GROUP_t *mygroup = iot->mygroup;
	/*
	 * this seems like a good idea.. if we're going
	 *   to log barrier waits then this seems like
	 *   a good place to do it from..  not everyone
	 *   runs mkdir do don't barrier there..
	 */
	if ( ACTIVETYPE(FIO_TIME_BARRIER) &&
	    ((op_type == BLOCK_clk) ||
	     (op_type == FSYNC_clk) ||
	     (op_type == WRITE_clk) ||
	     (op_type == READ_clk)) ) {
		/*
		 * cant use startwatch here (infinite loop)
		 */
		gettimeofday(&(iot->times[BARRIER_clk]), NULL);
		BARRIER();
		gettimeofday(&(iot->times[BARRIER_clk+1]), NULL);

		iot->op_log->oplog_barrier_time =
		    calc_run_time(&iot->times[BARRIER_clk],
			&iot->times[BARRIER_clk+1]);
	}

	iot->op_log->oplog_magic = OPLOG_MAGIC;
	iot->op_log->oplog_time = calc_run_time(&iot->times[op_type],
	    &iot->times[op_type+1]);
	iot->op_log->oplog_type = op_type;
	iot->op_log->oplog_used = true;
	DUMP_OPLOG_ENTRY(iot->op_log);
	iot->op_log++;
}

#define LOG(op) log_op((op), iot)

static inline void *
iolog_alloc(IOTESTLOG_t *iolog, size_t size)
{
	BDEBUG("size %zu\n", size);
	ASSERT( (size > 0) && !(size % OPLOGSZ) );
	void *p = PSCALLOC(size);
	iolog->iolog_size += size;
	return (p);
}

/*
 * Path making macros
 */
static inline char *
trunc_path(int depth, char *p)
{
	/* goto the end of the string */
	while (*p != '\0')
		p++;

	/* this should skip the last char on the string */
	while (depth) {
		//fprintf(stderr, "%c", *p);
		if (*(--p) == '/')
			depth--;
	}
	/* return with the final "/" intact */
	return (p+1);
}

static inline char *
str_end(char *p)
{
	/* goto the end of the string */
	while (*p != '\0')
		p++;
	/* return with the final "/" intact */
	return (p);
}

static inline void
make_fnam(struct io_toolbox *iot)
{
	GROUP_t *mygroup = iot->mygroup;
	int      tmp_pe  = iot->mytest_pe;

	if (ACTIVETYPE(FIO_SAMEFILE)) {
		if (!ACTIVETYPE(FIO_SEEKOFF))
			tmp_pe = 0;
		else
			tmp_pe = 999999;
	}

	iot->myfnam = str_end(iot->mypath);

	iot->path_len += snprintf(iot->myfnam,
	    (PATH_MAX - (iot->myfnam - iot->mypath)),
	    "%spe%d.%s.%d.%d",
	    FIO_FILE_PREFIX,
	    tmp_pe,
	    mygroup->test_filename,
	    iot->macro_iterations,
	    iot->micro_iterations);
}

static inline void
clear_fnam(struct io_toolbox *iot)
{
	char *ptr = trunc_path(1, iot->mypath);

	iot->path_len -= (int)(ptr - iot->mypath);

	*ptr = '\0';
}

static inline void
make_pe_specific_dir(struct io_toolbox *iot)
{
	iot->myfnam = str_end(iot->mypath);
	DEBUG(D_DTREE, "PATH %s\n", iot->mypath);

	iot->path_len += snprintf(iot->myfnam,
	    (PATH_MAX - (iot->myfnam - iot->mypath)),
	    "%d/", iot->mype);

	ASSERT(iot->path_len < PATH_MAX);

	DEBUG(D_DTREE, "PATH %s\n", iot->myfnam);
}

static inline void
make_rel_pathname(struct io_toolbox *iot)
{
	//GROUP_t *mygroup = iot->mygroup;
	iot->myfnam = str_end(iot->mypath);

	DEBUG(D_DTREE, "PATH %s\n", iot->mypath);

	iot->path_len += snprintf(iot->myfnam,
	    (PATH_MAX - (iot->myfnam - iot->mypath)),
	    "%sd%d.w%d/",
	    FIO_DIR_PREFIX,
	    iot->current_depth,
	    iot->current_width);

	ASSERT(iot->path_len < PATH_MAX);

	DEBUG(D_DTREE, "PATH %s\n", iot->myfnam);

	//iot->myfnam = iot->path_len;

	DEBUG(D_DTREE, "mypath PATH %s\n", iot->mypath);
}

static inline void
make_abs_pathname(struct io_toolbox *iot)
{
	GROUP_t *mygroup = iot->mygroup;

	iot->path_len = snprintf(iot->mypath, PATH_MAX,
	    "%s/FIO_TEST_ROOT/",
	    mygroup->test_path);

	iot->myfnam = str_end(iot->mypath);

	DEBUG(D_DTREE, "PATH %s\n", iot->mypath);
}

static inline void
push_dirstack(IOT_t *iot)
{
	DIR_t   *new_dir;
	GROUP_t *mygroup = iot->mygroup;

	iot->current_depth++;
	make_rel_pathname(iot);

	DEBUG(D_DTREE, "CURRENT DEPTH %d PATH %s\n",
	    iot->current_depth, iot->mypath);

	/* only the workpe does this.. */
	if (WORKPE) {
		new_dir = PSCALLOC(sizeof(DIR_t));
		INIT_PSC_LISTENTRY(&new_dir->dir_stack_lentry);
		psclist_add(&new_dir->dir_stack_lentry, &mygroup->dirStack);
		DEBUG(D_DTREE, "added dir %p %s\n",
		    new_dir, iot->mypath);
		MKDIR();
	}
	BARRIER();
}

static inline void
pop_dirstack(IOT_t *iot)
{
	GROUP_t          *mygroup = iot->mygroup;
	DIR_t            *current_dir, *tmp;
	char             *p;
	int               del_cnt = 0;

	DEBUG(D_DTREE, "ENTER\n");

	/* only the workpe does this.. */
	if (WORKPE) {
		psclist_for_each_entry_safe(current_dir, tmp,
		    &mygroup->dirStack, dir_stack_lentry) {

			DEBUG(D_DTREE, "list_del_check current_dir %p root %p %d\n",
			    current_dir, &mygroup->dirStack, del_cnt);

			if (!del_cnt ||
			    current_dir->child_cnt >= mygroup->tree_width) {
				DEBUG(D_DTREE, "about to list_del %p\n",
				    current_dir);

				psclist_del(&current_dir->dir_stack_lentry,
				    &mygroup->dirStack);

				DEBUG(D_DTREE, "did list_del %p\n",
				    current_dir);

				del_cnt++;
				//free(current_dir);

			} else
				break;
		}
		mygroup->depth_change = del_cnt;
	}

	BARRIER();

	psclist_for_each_entry(current_dir, &mygroup->dirStack, dir_stack_lentry)
		DEBUG(D_DTREE, "list empty? no, item: %p\n", current_dir);

	BARRIER();
	iot->current_depth -= mygroup->depth_change;

	DEBUG(D_DTREE, "CURRENT DEPTH %d\n", iot->current_depth);
	/* nuke this directory component */
	p  = trunc_path(mygroup->depth_change + 1, iot->mypath);
	*p = '\0';

	DEBUG(D_DTREE, "NEWPATH %s\n", iot->mypath);
}

/*
 * Buffer Related functions and macros
 */
static inline void
xor_buffer(struct buffer *bdesc)
{
	size_t t = 0;
	long *buf_long_ints = bdesc->buffer;

	for(t = 0; t < bdesc->buffer_size / sizeof(long); t++)
		buf_long_ints[t] ^= bdesc->block_number;
}

static inline int
compare_buffer(const struct buffer *bdesc_a, const struct buffer *bdesc_b)
{
	size_t t = 0;
	size_t i = bdesc_a->buffer_size / LONGSZ;
	const unsigned long *a = bdesc_a->buffer, *b = bdesc_b->buffer;

	ASSERT(bdesc_a->buffer_size <= bdesc_b->buffer_size);

	for (t = 0; t < i; t++) {
		if (a[t] != b[t]) {
			WARN("checksum failed at t=%zd/%zd a %#lx addr %p b %#lx addr %p\n",
			    t, i, a[t], &a[t], b[t], &b[t]);
			return -1;
		}
	}
	return 0;
}

#define SWABBUFFER(bdesc)							\
	do {									\
		if (ACTIVETYPE(FIO_VERIFY))					\
			xor_buffer(bdesc);					\
	} while (0)

#define CLEANBUFFER SWABBUFFER

#define COMPAREBUFFERS(a, b)							\
	do {									\
		if (ACTIVETYPE(FIO_VERIFY))					\
			compare_buffer((a), (b));				\
	} while (0)

int do_close(struct io_toolbox *);
int do_creat(struct io_toolbox *);
int do_fstat(struct io_toolbox *);
int do_link(struct io_toolbox *);
int do_null(struct io_toolbox *);
int do_open(struct io_toolbox *);
int do_rdopen(struct io_toolbox *);
int do_read(struct io_toolbox *);
int do_rename(struct io_toolbox *);
int do_stat(struct io_toolbox *);
int do_trunc(struct io_toolbox *);
int do_unlink(struct io_toolbox *);
int do_write(struct io_toolbox *);

void init_buffer(struct buffer *, int);

int run_yacc(void);

extern int lineno;

#endif /* _FIO_H_ */
