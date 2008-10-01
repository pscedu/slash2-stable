/* $Id$ */

struct io_toolbox;

enum sym_types { 
  FIO_FUNCTION = 1,
  FIO_VARIABLE = 2,
  FIO_METATAG  = 4,
  FIO_FLAG     = 8
};

enum sym_parameter_types { 
  FIO_STRING          = 0,
  FIO_INT             = 1,   
  FIO_BOOL            = 2,
  FIO_FLOAT           = 3,
  FIO_SIZET           = 4,
  FIO_TYPE_RECIPE     = 5,
  FIO_TYPE_GROUP      = 6,
  FIO_TYPE_RECURSE    = 7,
  FIO_NONE            = 8
};
  
enum config_heirarchies { 
  FIO_GROUP_TAG       = 001,
  FIO_RECURSE_TAG     = 002,
  FIO_TEST_RECIPE_TAG = 004
};

/*
 * Symbol tables for our reserved words
 */
#define FUNCNAME_MAX 64
#define OPTNAME_MAX  64


struct symtable_t { 
  char *name;
  enum  sym_types sym_type;
  enum  sym_parameter_types sym_param_type;
  int (*io_func)(struct io_toolbox *);
  int  offset;
  int  param;
};

struct symtable_t * get_symbol(const char *);
void store_tok_val(const char *, char *);
void store_func(const char *);
