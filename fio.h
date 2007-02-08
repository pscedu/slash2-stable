#define _GNU_SOURCE
#include <getopt.h>

#include "fio_list.h"

//#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <sys/queue.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>
#include <math.h>

#ifdef PTHREADS
#include <pthread.h>
#include "fio_pthread_barrier.h"
barrier_t  barrier;

#elif  MPI
//#include <mpi/mpi.h>
#include <mpi.h>
#endif

#define strtoull strtoul

#ifdef CATAMOUNT
#include <catamount/cnos_mpi_os.h>
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0200000
#endif

#define MAXROUTINE 16

//#define strtoull strtoul

#define FIO_DIR_PREFIX  "fio_d."
#define FIO_FILE_PREFIX "fio_f."

#ifdef OS64
#define TIMET  "%u" 
#define UTIMET "%06u"
#define SIZET  "%lu"
#define OFFT   "%lu" 
#define INOT   "%u" 

#elif QK
#define TIMET  "%lu"
#define UTIMET "%06lu"
#define SIZET  "%lu"
#define OFFT   "%llu"
#define INOT   "%lu"

#else
#define TIMET  "%lu"
#define UTIMET "%06lu"
#define SIZET  "%u"
#define OFFT   "%llu"
#define INOT   "%llu" 
#endif

int   TOTAL_PES;
int   fio_global_debug, fio_lexyacc_debug;
int   stderr_redirect;
char *stderr_fnam_prefix;
/*
 *  Test type bits
 */
enum TEST_OPTIONS { 
  FIO_SAMEDIR            = 1 << 1,  // all nsops happen in samedir   
  FIO_BLOCK_BARRIER      = 1 << 2,  // time each block (read/write)
  FIO_BARRIER            = 1 << 3,  // barrier stuff with mpi
  FIO_BARRDBG            = 1 << 4,
  FIO_SAMEFILE           = 1 << 5,  // io goes to same file.. 
  FIO_STAGGER            = 1 << 6,  // each process runs independently of other
  //FIO_PAROPEN            = 1 << 7,  // processes write to the same file 
  FIO_VERIFY             = 1 << 8,  // perform blocklevel checksumming
  FIO_WRITE              = 1 << 9,   
  FIO_READ               = 1 << 10,
  FIO_SEEKOFF            = 1 << 11,
  FIO_TIME_BARRIER       = 1 << 12,
  FIO_THRASH_LOCK        = 1 << 13, 
  FIO_TIME_BLOCK         = 1 << 14,
  FIO_FSYNC_BLOCK        = 1 << 15,
  FIO_INTERSPERSE        = 1 << 16,
  FIO_APP_BARRIER        = 1 << 17
};

#define ACTIVETYPE(t) ((t & mygroup->test_opts) ? 1 : 0)

#define GETSKEW (ACTIVETYPE(FIO_THRASH_LOCK) ? 1 : 0)

/*
 * This construct is used to provide protection
 *   of global memory regions for the pthreads port.
 *   MPI doesn't need this since every mpi process
 *   has its own heap
 */
#ifdef  MPI
#define WORKPE 1
#elif   PTHREADS
#define WORKPE ((iot->mytest_pe == mygroup->work_pe) ? 1 : 0)
#endif

enum CLOCKS { 
  WRITE_clk    = 0,
  READ_clk     = 2,
  CREAT_clk    = 4,
  RDOPEN_clk   = 6,
  UNLINK_clk   = 8,
  RENAME_clk   = 10,
  LINK_clk     = 12,
  BLOCK_clk    = 14, // time each block action
  STAT_clk     = 16,
  FSTAT_clk    = 18,
  CLOSE_clk    = 20,
  WROPEN_clk   = 22,
  APPOPEN_clk  = 24,
  BARRIER_clk  = 26,
  RDWROPEN_clk = 28,
  TRUNC_clk    = 30,
  MKDIR_clk    = 32,
  FSYNC_clk    = 34
};
#define NUMCLOCKS 36

enum path_depth { 
  KEEP_DIR    = 1,
  CHANGE_DIR  = 2,
  ASCEND_DIR  = 3
};

enum bool_t { 
  NO  = 0,
  YES = 1
};

enum debug_channels { 
  FIO_DBG_BLOCK   = 1 << 1,
  FIO_DBG_MEMORY  = 1 << 2,
  FIO_DBG_BARRIER = 1 << 3,
  FIO_DBG_DTREE   = 1 << 4,
  FIO_DBG_SYMTBL  = 1 << 5,
  FIO_DBG_CONF    = 1 << 6,
  FIO_DBG_OUTPUT  = 1 << 7,
  FIO_DBG_IOFUNC  = 1 << 8,
  FIO_DBG_BUFFER  = 1 << 9
};

enum debug_channels_short { 
  D_BLOCK   = 1 << 1,
  D_MEMORY  = 1 << 2,
  D_BARRIER = 1 << 3,
  D_DTREE   = 1 << 4,
  D_SYMTBL  = 1 << 5,
  D_CONF    = 1 << 6,
  D_OUTPUT  = 1 << 7,
  D_IOFUNC  = 1 << 8,
  D_BUFFER  = 1 << 9
};

#define MAXTESTS            16
#define TEST_GROUP_NAME_MAX PATH_MAX

#define LONGSZ sizeof(long int)

typedef unsigned long long u64;

struct op_log_t { 
  int    oplog_magic;
  enum   CLOCKS    oplog_type;
  enum   bool_t    oplog_used;
  enum   bool_t    oplog_checksum_ok;
  int    oplog_subcnt;
  float  oplog_time;
  float  oplog_barrier_time;
  union { 
    struct op_log_t *oplog_sublog;
    off_t  oplog_offset;          
  } sub;
};
typedef struct op_log_t OPLOG_t;
#define OPLOGSZ sizeof(OPLOG_t)
#define OPLOG_MAGIC 0x6e1eafe9

struct iotest_log_t { 
  char     iolog_tname[TEST_GROUP_NAME_MAX];
  int      iolog_pe;
  int      iolog_iteration;
  OPLOG_t *iolog_oplog;
  int      iolog_oplog_cnt;
  size_t   iolog_size;
};
typedef struct iotest_log_t IOTESTLOG_t;
#define IOTESTLOGSZ sizeof(IOTESTLOG_t)

struct buffer_t { 
  void  *buffer;
  size_t buffer_size;
  size_t data_size;
  off_t  current_offset;
  int    block_number;
  struct drand48_data rand_data;
};
typedef struct buffer_t BUF_t;


struct io_toolbox { 
  void  *mygroup;                        /* pointer back to my group */
  struct buffer_t      bdesc;             
  struct buffer_t      rd_bdesc;
  struct timeval       times[NUMCLOCKS];
  struct stat          stb;
  struct stat          stb_unlink;
  IOTESTLOG_t          io_log;
  OPLOG_t             *op_log; 
  u64    mysize;
  char   mypath[PATH_MAX];
  char  *myfnam;
  char  *logBuf; 
  char  *logBufPtr;
  int    micro_iterations;               /* how many times..    */
  int    macro_iterations;   
  int    myfd;
  int    mype;
  int    mytest_pe;
  int    param;
  int    unlink; 
  int    num_blocks;
  int    path_len;
  int    path_isdir;
  enum   path_depth depth;
  int    filenum;
  int    current_depth;
  int    current_width;
  int    current_test;
  int    barrier_cnt;
  int    debug_flags;
};
typedef struct io_toolbox IOT_t;

struct io_routine_t { 
  int   num_routines;
  char  io_testname[TEST_GROUP_NAME_MAX];
  char *io_routine[MAXROUTINE];
};
typedef struct io_routine_t IOROUTINE_t;

struct io_thread_t { 
#ifdef PTHREADS
  pthread_t thread_id;
#endif
  int       mype;                    /* mype is the absolute pe */
  int       rc;
};
typedef struct io_thread_t THREAD_t;


struct dirnode_t { 
  struct list_head dir_stack;
  int   depth;
  int   child_cnt;
  int   test_done;
  char *path_comp;
};
typedef struct dirnode_t DIR_t;


struct test_group_t { 
  struct list_head group_list;
  struct io_routine_t  iotests[MAXTESTS];
  struct io_routine_t *current_iotest;
  
  int    num_iotests;

  struct timeval      test_freq;         /* how often io occurs */
  struct timeval      block_freq;        /* sleep in between block */
  char   test_name[TEST_GROUP_NAME_MAX]; /* name of the group   */
  char   test_path[PATH_MAX];            /* starting path       */
  char   output_path[PATH_MAX];          /* starting path       */
  char   test_filename[PATH_MAX];        
  u64    block_size;                     /* chunk size          */
  u64    file_size;                      /* file size           */
  int    num_pes;                        /* num of processes    */
  int    work_pe;                        /* pe next up for work */
  int    iterations;
  int    test_opts;                      /* what this test does */
  int    files_per_pe;
  int    files_per_dir;
  int    thrash_lock;  
  int    tree_depth;
  int    tree_width;
  int    depth_change;
  int    debug_flags;
#ifdef PTHREADS
  barrier_t  group_barrier;
  THREAD_t  *threads; 
#elif  MPI
  MPI_Group  group;
  MPI_Comm   group_barrier;
#endif
  DIR_t  *dirRoot;
  struct list_head dirStack;
};
typedef struct test_group_t GROUP_t;

struct list_head groupList;
GROUP_t         *currentGroup;
int              numGroups;

#define DEBUG(chan, format, args...) do {                             \
  struct timeval tv;                                                  \
  gettimeofday(&tv, NULL);                                            \
  if (chan & iot->debug_flags)                                        \
      fprintf(stderr,                                                 \
	      TIMET"."UTIMET " %s PE_%05d %s() %s, %d :: "format,     \
   	      tv.tv_sec, tv.tv_usec, get_dbg_prefix(chan), iot->mype, \
	      __FUNCTION__, __FILE__, __LINE__, ##args);              \
  } while (0) 

#define PRINT(format, args...)                                        \
   fprintf(stderr, "%s() %s, %d :: "format,                           \
	__FUNCTION__, __FILE__, __LINE__, ##args);

#define TPRINT(tv, format, args...)                                   \
   fprintf(stderr, TIMET"."UTIMET" PE_%05d %s() :: "format,           \
      	   tv.tv_sec, tv.tv_usec, iot->mype, __FUNCTION__, ##args); 

#define TPRINTPE TPRINT

#define BDEBUG(format, args...)                                       \
  if (fio_global_debug)                                               \
      fprintf(stderr, "%s() %s, %d :: "format,                        \
   	      __FUNCTION__, __FILE__, __LINE__, ##args);

#define BDEBUGPE(format, args...) do {                                \
  struct timeval tv;                                                  \
  gettimeofday(&tv, NULL);                                            \
  if (fio_global_debug)                                               \
      fprintf(stderr,                                                 \
	      TIMET"."UTIMET "GDBG %s() PE_%05d %s, %d :: "format,    \
   	      tv.tv_sec, tv.tv_usec, __FUNCTION__, iot->mype,         \
	      __FILE__, __LINE__, ##args);                            \
  } while (0)

#define CDEBUG(format, args...)                                       \
  if (fio_lexyacc_debug)                                              \
      fprintf(stderr, "%s() %s, %d :: "format,                        \
   	      __FUNCTION__, __FILE__, __LINE__, ##args);

#define ASSERT(cond) 				                      \
  if (!(cond)) {				                      \
    fprintf(stderr, "ASSERT %s() %s, %d %s\n",    	              \
	    __FUNCTION__, __FILE__, __LINE__, strerror(errno));	      \
    exit(1);					                      \
  }

#define ASSERTPE(cond) 				                      \
  if (!(cond)) {				                      \
    fprintf(stderr, "ASSERT PE_%d %s() %s, %d %s\n",    	      \
	    iot->mype, __FUNCTION__, __FILE__,                        \
            __LINE__, strerror(errno));				      \
    exit(1);					                      \
  }

#define ASSERTMSG(cond, format, args...)                              \
  if (!(cond)) {				                      \
    fprintf(stderr, "ASSERT %s() %s, %d msg:"format,    	      \
	    __FUNCTION__, __FILE__, __LINE__, ##args);		      \
    exit(1);					                      \
  }

#define WARN(format, args...)			                      \
  fprintf(stderr, "WARNING %s() %s, %d :: :: "format,                 \
	  __FUNCTION__, __FILE__, __LINE__, ##args)

static inline char * get_dbg_prefix(int dbg_channel) 
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

#define DUMP_BUFFER(b)                                                        \
  DEBUG(D_BUFFER, "buffer_t %p: buffer %p, offset %llu, block_num %d, block_size %i\n", \
	b, b->buffer, (off_t)b->current_offset,                               \
	b->block_number, b->buffer_size)


#define DUMP_GROUP(g) do {	                		    \
    int i,j;							    \
    BDEBUG("Group '%s' %p\n\tnum_pes %d\n\tnum_tests %d\n\tpath '%s'\n\tfile_size %llu\n\tblock_size %llu\n\titeratons %d\n\ttest_opts %d\n\tfile_per_pe %d\n\tfile_per_dir %d\n\ttree_depth %d\ttree_width %d\n\ttest_freq "TIMET TIMET"\n", \
	    g->test_name,                                           \
	    g,                                                      \
            g->num_pes,                                             \
	    g->num_iotests,                                         \
	    g->test_path,                                           \
	    (u64)g->file_size,					    \
	    (u64)g->block_size,					    \
	    g->iterations,                                          \
	    g->test_opts,                                           \
	    g->files_per_pe,                                        \
	    g->files_per_dir,                                       \
	    g->tree_depth,                                          \
	    g->tree_width,                                          \
	    g->test_freq.tv_sec, g->test_freq.tv_usec);	            \
                                                                    \
    for (i=0; i < g->num_iotests; i++) {                            \
      BDEBUG("\t%s:\n",						    \
              g->iotests[i].io_testname);                           \
      for (j=0; j < g->iotests[i].num_routines; j++) {		    \
	BDEBUG("\t\t%s %p:\n",					    \
	      (char *)g->iotests[i].io_routine[j],		    \
	      g->iotests[i].io_routine[j]);			    \
      }                                                             \
    }                                                               \
} while (0)

#define STARTWATCH(test) gettimeofday(&(iot->times[test]),   NULL)
#define STOPWATCH(test)                                               \
  gettimeofday(&(iot->times[test+1]), NULL);	                      \
  log_op(test, iot);

static inline double calc_run_time(struct timeval *tv1, 
				   struct timeval *tv2) { 
  float t;

  t = ( ((tv2->tv_usec/1000000.0) + tv2->tv_sec) - 
	((tv1->tv_usec/1000000.0) + tv1->tv_sec) );

  return t;
}

static inline char * clock_2_str(int clock) { 
  switch (clock) {
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
 * Barrier Macros provide the same interface 
 *  regardless of the underlying syncronization 
 *  method.
 */

static inline void _BARRIER(GROUP_t *mygroup, struct io_toolbox *iot) { 
#ifdef PTHREADS 
  barrier_wait(&mygroup->group_barrier);
#elif MPI
  MPI_Barrier(mygroup->group_barrier);                    
#else
  WARN("No barrier support..\n");
#endif        
  iot->barrier_cnt++;
  DEBUG(D_BARRIER, "barrier_cnt %d\n", 
	iot->barrier_cnt);
  //  sleep(1);
  return;
}


#ifdef CATAMOUNT
#define YY_BARRIER cnos_barrier()
#endif

#define __BARRIER  ( ACTIVETYPE(FIO_BARRIER) && \
		  !ACTIVETYPE(FIO_STAGGER) ? _BARRIER(mygroup, iot) : 0)

#define SBARRIER ( ACTIVETYPE(FIO_BARRIER) ? _BARRIER(mygroup) : 0)

#define BARRIER __BARRIER
#define APP_BARRIER if (ACTIVETYPE(FIO_APP_BARRIER)) BARRIER

// figure out how long we waited at this barrier      
#define LOG_BARRIER_WAIT(fn,i) do {                              \
  STARTWATCH(BARRIER_clk);                                       \
  BARRIER;                                                       \
  STOPWATCH(BARRIER_clk);                                        \
  LOG1(iot, "BW%s\t%#09x %d %.4f\n",                             \
       clock_2_str(BARRIER_clk), fn, i,                          \
       calc_run_time(times[BARRIER_clk], times[BARRIER_clk+1])); \
} while (0)                                                     

#define CREAT do {                                               \
  APP_BARRIER;                                                   \
  STARTWATCH(CREAT_clk);                                         \
  ASSERT( (iot->myfd = creat(iot->mypath, 0644)) >= 0);		 \
  STOPWATCH(CREAT_clk);                                          \
  ASSERT( !close(iot->myfd) );                                   \
} while (0)

#define TRUNC do {                                               \
  APP_BARRIER;                                                       \
  STARTWATCH(TRUNC_clk);                                         \
  ASSERT(!ftruncate(iot->myfd, iot->stb.st_size));               \
  STOPWATCH(TRUNC_clk);                                          \
} while (0) 

#define SEEKOFF do {                                             \
  iot->mysize = (mygroup->file_size / mygroup->num_pes);         \
  off_t seekv =  iot->mysize * iot->mype;                        \
  DEBUG(D_BLOCK, "PE %d, seeking "OFFT" bytes\n",                \
	iot->mype, (off_t)seekv);                                \
  APP_BARRIER;                                                       \
  ASSERT(lseek(iot->myfd, seekv, SEEK_SET) >= 0 );               \
} while (0)

#define INTERSPERSE_SEEK do {                                    \
  off_t seekv;                                                   \
  if (!iot->bdesc.block_number)                                  \
     seekv = (iot->bdesc.buffer_size * 	iot->mype);              \
  else seekv = (iot->bdesc.buffer_size * mygroup->num_pes);      \
  DEBUG(D_BLOCK, "PE %d, seeking "OFFT" bytes\n",                \
	iot->mype, (off_t)seekv);                                \
  ASSERT(lseek(iot->myfd, seekv, SEEK_CUR) >= 0 );               \
  DEBUG(D_BLOCK, "PE %d, curr off "OFFT"\n",                     \
	iot->mype, (off_t)lseek(iot->myfd, (off_t)0, SEEK_CUR)); \
} while (0)

#define RDOPEN do {                                              \
  APP_BARRIER;                                                       \
  STARTWATCH(RDOPEN_clk);                                        \
  DEBUG(D_DTREE, "about to open %s\n", iot->mypath);             \
  ASSERT( (iot->myfd = open(iot->mypath, O_RDONLY)) >= 0 );      \
  STOPWATCH(RDOPEN_clk);                                         \
} while ( 0 )

#define WROPEN do {                                              \
  APP_BARRIER;                                                       \
  STARTWATCH(WROPEN_clk);                                        \
  ASSERT((iot->myfd =                                            \
	  open(iot->mypath, O_CREAT| O_WRONLY|O_TRUNC, 0644)) >= 0 );	         \
  DEBUG(D_DTREE, "opened %s fd = %d\n", iot->mypath, iot->myfd);	 \
  STOPWATCH(WROPEN_clk);                                         \
} while ( 0 )
  
#define APPOPEN do {                                             \
  APP_BARRIER;                                                       \
  STARTWATCH(APPOPEN_clk);                                       \
  ASSERT((iot->myfd =                                            \
	  open(iot->mypath, O_WRONLY|O_APPEND)) >= 0 );	         \
  STOPWATCH(APPOPEN_clk);                                        \
} while ( 0 )

#define RDWROPEN do {                                            \
  APP_BARRIER;                                                       \
  DEBUG(D_DTREE, "rdwr open path ;%s;\n", iot->mypath);           \
  if ( !ACTIVETYPE(FIO_STAGGER) ) BARRIER;                       \
  STARTWATCH(RDWROPEN_clk);                                      \
  ASSERT( (iot->myfd = open(iot->mypath, O_RDWR)) >= 0 );        \
  STOPWATCH(RDWROPEN_clk);                                       \
} while ( 0 )

#define CLOSE do {                                               \
  APP_BARRIER;                                                       \
  STARTWATCH(CLOSE_clk);                                         \
  ASSERT( !close(iot->myfd) );                                   \
  STOPWATCH(CLOSE_clk);                                          \
} while ( 0 )

#define MKDIR do {						 \
  int rc = 0;							 \
  APP_BARRIER;                                                   \
  STARTWATCH(MKDIR_clk);                                         \
  rc = mkdir(iot->mypath, 0755);				 \
  if (rc && (errno != EEXIST))					 \
    ASSERTPE(0);						 \
  STOPWATCH(MKDIR_clk);                                          \
} while ( 0 )
  
#define FSTAT do {                                               \
  APP_BARRIER;                                                       \
  STARTWATCH(FSTAT_clk);                                         \
  if (iot->unlink) { 				                 \
    ASSERT(!fstat(iot->myfd, &iot->stb_unlink));                 \
  } else { 					                 \
    ASSERT(!fstat(iot->myfd, &iot->stb));	                 \
  }                                                              \
  STOPWATCH(FSTAT_clk);                                          \
  DEBUG(D_DTREE,                                                 \
	"%s\t%#09x Filesize = "SIZET", Inode = "INOT"\n",        \
	clock_2_str(BLOCK_clk), iot->filenum,		         \
	(size_t)iot->stb.st_size, iot->stb.st_ino);		 \
} while ( 0 )
 
#define STAT do {                                                \
  APP_BARRIER;                                                   \
  STARTWATCH(STAT_clk);                                          \
  ASSERT(!stat(iot->mypath, &iot->stb));                         \
  STOPWATCH(STAT_clk);                                           \
  DEBUG(D_DTREE,                                                 \
	"%s\t%#09x Filesize = "SIZET", Inode = "INOT"\n",        \
        clock_2_str(STAT_clk), iot->filenum,	               	 \
	(size_t)iot->stb.st_size, iot->stb.st_ino);		 \
} while ( 0 )


#define DUMP_OPLOG_ENTRY(e)                                     \
 BDEBUGPE("Addr %p Type %d Used %d Time %f btime %f sublog %p\n", \
         e, e->oplog_type, e->oplog_used, e->oplog_time,        \
         e->oplog_barrier_time, e->sub.oplog_sublog);


static inline void log_op(int    op_type, 
			  IOT_t *iot) 
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
    
    BARRIER;
    
    gettimeofday(&(iot->times[BARRIER_clk+1]), NULL);
    
    iot->op_log->oplog_barrier_time = 
      calc_run_time(&iot->times[BARRIER_clk], 
		    &iot->times[BARRIER_clk+1]);
  }  
  
  iot->op_log->oplog_magic = OPLOG_MAGIC;
  iot->op_log->oplog_time = calc_run_time(&iot->times[op_type], 
					  &iot->times[op_type+1]);
  iot->op_log->oplog_type = op_type;
  iot->op_log->oplog_used = YES;
  DUMP_OPLOG_ENTRY(iot->op_log);
  iot->op_log++;
  return;
}

#define LOG(op) log_op(op, iot)


static inline void * iolog_alloc(IOTESTLOG_t *iolog, size_t size) 
{ 
  BDEBUG("size "SIZET"\n", size);
  ASSERT( (size > 0) && !(size % OPLOGSZ) );
  void *p = malloc(size);  
  ASSERT(p != NULL);
  iolog->iolog_size += size;
  return p;
}
     
/* 
 * Path making macros 
 */
static inline char * trunc_path(int depth, char *p) 
{ 
  /* goto the end of the string */
  while (*p != '\0') p++; 
  /* this should skip the last char on the string */
  while (depth) { 
    //fprintf(stderr, "%c", *p);
    if (*(--p) == '/') depth--;
  }
  /* return with the final "/" intact */
  return (p+1);
}

static inline char * str_end(char *p) 
{ 
  /* goto the end of the string */
  while (*p != '\0') p++;
  /* return with the final "/" intact */
  return (p);
}


static inline void make_fnam(struct io_toolbox *iot) 
{
  GROUP_t *mygroup = iot->mygroup;
  int      tmp_pe  = iot->mytest_pe;
  
  if (ACTIVETYPE(FIO_SAMEFILE))
    tmp_pe = 999999;
  
  iot->myfnam = str_end(iot->mypath);

  iot->path_len += snprintf(iot->myfnam, 
			    (PATH_MAX - (iot->myfnam - iot->mypath)), 
			    "%spe%d.%s.%d.%d", 
			    FIO_FILE_PREFIX, 
			    tmp_pe, 
			    mygroup->test_filename, 
			    iot->macro_iterations, 
			    iot->micro_iterations);
  return;
}

static inline void clear_fnam(struct io_toolbox *iot) 
{
  char *ptr = trunc_path(1, iot->mypath);

  iot->path_len -= (int)(ptr - iot->mypath);
  
  *ptr      = '\0';
   
  return;
}


static inline void make_pe_specific_dir(struct io_toolbox *iot) { 
  iot->myfnam = str_end(iot->mypath);
  DEBUG(D_DTREE, "PATH %s\n", iot->mypath);
  
  iot->path_len += snprintf(iot->myfnam,
                            (PATH_MAX - (iot->myfnam - iot->mypath)),
                            "%d/", iot->mype);

  ASSERT(iot->path_len < PATH_MAX);

  DEBUG(D_DTREE, "PATH %s\n", iot->myfnam);

  return;
}

static inline void make_rel_pathname(struct io_toolbox *iot) 
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

  return;
}

static inline void make_abs_pathname(struct io_toolbox *iot) 
{ 
  GROUP_t *mygroup = iot->mygroup;

  iot->path_len = snprintf(iot->mypath, PATH_MAX, 
			   "%s/FIO_TEST_ROOT/", 
			   mygroup->test_path);

  iot->myfnam = str_end(iot->mypath);

  DEBUG(D_DTREE, "PATH %s\n", iot->mypath);

  return;
}

static inline void push_dirstack(IOT_t *iot) { 
  DIR_t   *new_dir;
  GROUP_t *mygroup = iot->mygroup;

  iot->current_depth++;
  make_rel_pathname(iot);

  DEBUG(D_DTREE, "CURRENT DEPTH %d PATH %s\n", 
	iot->current_depth, iot->mypath);

  /* only the workpe does this.. */    
  if (WORKPE) { 
    new_dir = malloc(sizeof(DIR_t));
    ASSERT(new_dir != NULL);
    bzero(new_dir, (sizeof(DIR_t)));
    INIT_LIST_HEAD(&new_dir->dir_stack);
    list_add(&new_dir->dir_stack, &mygroup->dirStack);    
    DEBUG(D_DTREE, "added dir %p %s\n", 
	  new_dir, iot->mypath);
    MKDIR;
  }
  BARRIER;

  return;
}

static inline void pop_dirstack(IOT_t *iot) 
{ 
  struct list_head *tmp, *tmp1;
  GROUP_t          *mygroup = iot->mygroup;
  DIR_t            *current_dir;
  char             *p;
  int               del_cnt = 0;
  
  DEBUG(D_DTREE, "ENTER\n");

  /* only the workpe does this.. */    
  if (WORKPE) { 
    list_for_each_safe(tmp, tmp1, &mygroup->dirStack) { 

      current_dir = list_entry(tmp, DIR_t, dir_stack);
      
      DEBUG(D_DTREE, "list_del_check current_dir %p root %p %d\n", 
	    current_dir, &mygroup->dirStack, del_cnt);
      
      if (!del_cnt ||
	  current_dir->child_cnt >= mygroup->tree_width) { 
	DEBUG(D_DTREE, "about to list_del %p\n",
	      current_dir);

	list_del(&current_dir->dir_stack);

	DEBUG(D_DTREE, "did list_del %p\n",
	      current_dir);

	del_cnt++;
	//free(current_dir);
	
      } else break;
    }
    mygroup->depth_change = del_cnt;
  }

  BARRIER;
  
  tmp = NULL;
  
  /*
   * 
   */
  list_for_each(tmp, &mygroup->dirStack) { 
    DEBUG(D_DTREE, "list_for_each() %p empty? %d\n", 
	    tmp, list_empty(&mygroup->dirStack));
  }

  DEBUG(D_DTREE, "list_for_each() DONE %p empty? %d\n", 
	  tmp, list_empty(&mygroup->dirStack));

  BARRIER;
  iot->current_depth -= mygroup->depth_change;

  DEBUG(D_DTREE, "CURRENT DEPTH %d\n", iot->current_depth);
  /* nuke this directory component */
  p  = trunc_path(mygroup->depth_change + 1, iot->mypath);
  *p = '\0';
  
  DEBUG(D_DTREE, "NEWPATH %s\n", iot->mypath);
  return;
}

/*
 * Buffer Related functions and macros
 */
static inline void xor_buffer(struct buffer_t *bdesc){ 
  int t = 0;
  long int *buf_long_ints = (long int *)bdesc->buffer;

  for(t = 0; t < bdesc->buffer_size / sizeof(long int); t++){
    buf_long_ints[t] ^= bdesc->block_number;
  }
  return;
}

static inline int compare_buffer(const struct buffer_t *bdesc_a, 
				 const struct buffer_t *bdesc_b) {
  int    t = 0;
  size_t i = bdesc_a->buffer_size / LONGSZ;

  unsigned long *a, *b;

  a = (unsigned long*)bdesc_a->buffer;
  b = (unsigned long*)bdesc_b->buffer;

  ASSERT(bdesc_a->buffer_size <= bdesc_b->buffer_size);

  for(t=0; t < i; t++) {

    if( a[t] != b[t] ){
      WARN("checksum failed at t=%d a 0x%lx addr %p b 0x%lx addr %p\n", 
	   t, a[t], (void *)&a[t], b[t], (void *)&b[t]);
      return -1;
    }
  }
  return 0;
}

#define SWABBUFFER(bdesc) \
   if ( ACTIVETYPE(FIO_VERIFY) ) xor_buffer(bdesc)

#define CLEANBUFFER SWABBUFFER

#define COMPAREBUFFERS(a, b) \
   if ( ACTIVETYPE(FIO_VERIFY) ) compare_buffer(a, b)

int do_unlink(struct io_toolbox *);
int do_rdopen(struct io_toolbox *);
int do_rename(struct io_toolbox *);
int do_trunc(struct io_toolbox *);
int do_fstat(struct io_toolbox *);
int do_write(struct io_toolbox *);
int do_creat(struct io_toolbox *);
int do_trunc(struct io_toolbox *);
int do_close(struct io_toolbox *);
int do_stat(struct io_toolbox *);
int do_open(struct io_toolbox *);
int do_read(struct io_toolbox *);
int do_link(struct io_toolbox *);
int do_null(struct io_toolbox *);
void init_buffer(struct buffer_t *, int);

int run_yacc();

int getOptions(int,  char **); 
void printHelp(void);


