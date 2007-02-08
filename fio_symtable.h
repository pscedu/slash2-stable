
/* declare and initialize the global table */
struct symtable_t sym_table[] = 
{ 
  {"creat",   FIO_FUNCTION, FIO_NONE, .io_func = do_creat}, 
  {"create",  FIO_FUNCTION, FIO_NONE, .io_func = do_creat}, 
  {"trunc",   FIO_FUNCTION, FIO_NONE, .io_func = do_trunc}, 
  {"ftrunc",  FIO_FUNCTION, FIO_NONE, .io_func = do_trunc}, 

  {"openrd",  FIO_FUNCTION, FIO_NONE, 
   .param = O_RDONLY, .io_func = do_open},

  {"openwr",  FIO_FUNCTION, FIO_NONE, 
   .param = O_WRONLY, .io_func = do_open},  

  {"openap",  FIO_FUNCTION, FIO_NONE, 
   .param = (O_APPEND | O_WRONLY), .io_func = do_open},

  {"openrw",  FIO_FUNCTION, FIO_NONE, 
   .param = O_RDWR,   .io_func = do_open},

  {"close",   FIO_FUNCTION, FIO_NONE, .io_func = do_close}, 
  {"stat",    FIO_FUNCTION, FIO_NONE, .io_func = do_stat}, 
  {"fstat",   FIO_FUNCTION, FIO_NONE, .io_func = do_fstat}, 
  {"link",    FIO_FUNCTION, FIO_NONE, .io_func = do_link}, 
  {"unlink",  FIO_FUNCTION, FIO_NONE, .io_func = do_unlink}, 
  {"rename",  FIO_FUNCTION, FIO_NONE, .io_func = do_rename},
  {"write",   FIO_FUNCTION, FIO_NONE, .io_func = do_write}, 
  {"read",    FIO_FUNCTION, FIO_NONE, .io_func = do_read},

  {"test_freq",                 
   FIO_VARIABLE, FIO_FLOAT,  
   .param = PATH_MAX, .offset = offsetof(GROUP_t, test_freq)
  },

  {"block_freq",                 
   FIO_VARIABLE, FIO_FLOAT,
   .param = PATH_MAX, .offset = offsetof(GROUP_t, block_freq)
  },

  {"path",                 
   FIO_VARIABLE, FIO_STRING,  
   .param = PATH_MAX, .offset = offsetof(GROUP_t, test_path)
  },

  {"output_path",                 
   FIO_VARIABLE, FIO_STRING,  
   .param = PATH_MAX, .offset = offsetof(GROUP_t, output_path)
  },

  {"filename",                 
   FIO_VARIABLE, FIO_STRING,  
   .param = PATH_MAX, .offset = offsetof(GROUP_t, test_filename)
  },

  {"file_size",                 
   FIO_VARIABLE, FIO_SIZET,  
   .param = 4, .offset = offsetof(GROUP_t, file_size) 
  },

  {"block_size",
   FIO_VARIABLE, FIO_SIZET,  
   .param = 4, .offset = offsetof(GROUP_t, block_size)
  },

  {"pes",                 
   FIO_VARIABLE, FIO_INT,  
   .param = 4, .offset = offsetof(GROUP_t, num_pes) 
  },

  {"files_per_pe",        
   FIO_VARIABLE, FIO_INT,  
   .param = 1, .offset = offsetof(GROUP_t, files_per_pe)
  }, 

  {"files_per_dir",        
   FIO_VARIABLE, FIO_INT,  
   .param = 1, .offset = offsetof(GROUP_t, files_per_dir) 
  }, 
  
  {"tree_depth",
   FIO_VARIABLE, FIO_INT,
   .param = 1, .offset = offsetof(GROUP_t, tree_depth)
  },
  
  {"tree_width",
   FIO_VARIABLE, FIO_INT,
   .param = 1, .offset = offsetof(GROUP_t, tree_width)
  },
  
  {"iterations",
   FIO_VARIABLE, FIO_INT,
   .param = 1, .offset = offsetof(GROUP_t, iterations)
  },

  {"samedir",
   FIO_FLAG, FIO_BOOL,
   .param = FIO_SAMEDIR, .offset = offsetof(GROUP_t, test_opts)
  }, 
  
  {"samefile",
   FIO_FLAG, FIO_BOOL,  
   .param = FIO_SAMEFILE, .offset = offsetof(GROUP_t, test_opts)  
  }, 
  
  {"seekoff", 
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_SEEKOFF, .offset = offsetof(GROUP_t, test_opts) 
  }, 
  
  {"intersperse", 
    FIO_FLAG, FIO_BOOL, 
   .param = FIO_INTERSPERSE, .offset = offsetof(GROUP_t, test_opts) 
  },

  {"stagger", 
   FIO_FLAG, FIO_BOOL,  
   .param = FIO_STAGGER, .offset = offsetof(GROUP_t, test_opts) 
  }, 
  
  {"verify", 
   FIO_FLAG, FIO_BOOL,  
   .param = FIO_VERIFY, .offset = offsetof(GROUP_t, test_opts)  
  }, 
  
  {"time_block", 
   FIO_FLAG, FIO_BOOL,  
   .param = FIO_TIME_BLOCK, 
   .offset = offsetof(GROUP_t, test_opts)  
  },
  
  {"barrier", 
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_BARRIER, .offset = offsetof(GROUP_t, test_opts) 
  },

  {"app_barrier", 
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_APP_BARRIER, .offset = offsetof(GROUP_t, test_opts) 
  },

  {"block_barrier",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_BLOCK_BARRIER, 
   .offset = offsetof(GROUP_t, test_opts) 
  },

  {"time_barrier",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_TIME_BARRIER, .offset = offsetof(GROUP_t, test_opts) 
  },

  {"thrash_lock",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_THRASH_LOCK, .offset = offsetof(GROUP_t, test_opts) 
  },

  {"fsync_block",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_FSYNC_BLOCK, .offset = offsetof(GROUP_t, test_opts) 
  },

  {"debug_block",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_BLOCK, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_memory",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_MEMORY, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_dtree",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_DTREE, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_barrier",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_BARRIER, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_symtable",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_SYMTBL, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_conf",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_CONF, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_output",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_OUTPUT, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_iofunc",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_IOFUNC, .offset = offsetof(GROUP_t, debug_flags) 
  },

  {"debug_buffer",
   FIO_FLAG, FIO_BOOL, 
   .param = FIO_DBG_BUFFER, .offset = offsetof(GROUP_t, debug_flags) 
  },
  
  {"test_recipe",  FIO_TYPE_GROUP, FIO_NONE},
  {"group",        FIO_TYPE_GROUP, FIO_NONE},
  
  {NULL}
};
