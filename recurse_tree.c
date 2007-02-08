/*

PEs        = 8
WIDTH      = 4
DEPTH      = 4
FPD        = 7
KEEP_CLOSE = 0


#dir_end(a) : (a + (((a+WIDTH)%PEs)-1))

TIME 0 // create the directories
  tmp = WIDTH
  do 

      I{start:dir_end()} ( P(I)->mkdir(I) )
      
  tmp -= WIDTH
  while (tmp)


TIME 1 // create the files
  tmp = FPD
  do
     if tmp <  then a = tmp 
<<<<<<< recurse_tree.c

struct recurse_t { 
  int    files_per_dir;
  int    thrash_lock;  
  int    tree_depth;
  int    tree_width;
  int    current_depth;
  int    current_width;
};

#define TEST_GROUP_NAME_MAX 64

struct test_group_t { 
  struct timeval   test_freq;            /* how often io occurs */
  struct recurse_t tree_params;          /* store tree dimensions */
  char   test_name[TEST_GROUP_NAME_MAX]; /* name of the group   */
  char   test_path[PATH_MAX]             /* starting path       */
  size_t block_size;                     /* chunk size          */
  size_t file_size;                      /* file size           */
  int    num_pes;                        /* num of processes    */
  int    iteration;
  int    test_type;                      /* what this test does */
#ifdef PTHREADS
  barrier_t  group_barrier;
#endif
};



group 8PE_RW_RECURSE { 

  files_per_dir = 4;
  thrash_lock   = yes;
  tree_depth    = 1;
  tree_width    = 8;
  pes          = 8;
  freq         = 1.0001; // secs.usecs for app simulation
  path         = /scratch/lustre/fs;
  samedir      = yes;
  file_size    = 16g;
  block_size   = 64k;
  //num_files    = 128 // not needed with recurse
  verify       = yes;
  barrier      = yes;
  barrier_time = yes;  
  iterations   = 4;
  
  // this entire block will happen 4 times (iterations)
  iotests ( 
	   // write the tree's files then go back and read them
	   WT1 [creat:trunc:openwr:write:fstat:fsync:close];
	   //  for a re-initialization here..
	   RT1 [stat:openrd:read:fstat:close:unlink];
  )
}

group 1024PE_WRITE {
  pes          = 1024
  freq         = 0  // no usleep between ops
  path         = /scratch/lustre/fs
  samedir      = no // all io to separate dir
  file_size    = 1g
  block_size   = 8m
  files_per_pe = 1
  verify       = no
  barrier      = yes
  barrier_time = no  
  iterations   = 1
  iotests (
    WRT_FSYNC   [creat:openwr:write:fstat:fsync:close]
    UNLINK      [unlink]
    WRT_NOFSYNC [creat:openwr:write:fstat:close]
    UNLINK      [unlink]
  )
}

runtest "1024 Blaster with Read" 1024PE_WRITE, 8PE_RW_RECURSE


recurserecurse

/*
  operations { 
    // read as files are created
    recurse=test1 { 
      creat:trunc:openwr:write:fstat:fsync:close:openrd:read:close;
    }
  }
*/
=======
*/

int do_recursive_ops(struct io_toolbox *iot) {
  int rc = 0;

  return rc;
}
>>>>>>> 1.2


worker_pe + SKEW + FILES_PER_DIR


Path Factor Variables
---------------------
iteration No., depth, width

so should the paths be pre-computed?
