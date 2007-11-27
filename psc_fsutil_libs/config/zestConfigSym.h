/* $Id: zestConfigSym.h 2189 2007-11-07 22:18:18Z yanovich $ */

enum sym_types {
	ZEST_FUNCTION = 1,
	ZEST_VARIABLE = 2,
	ZEST_METATAG  = 4,
	ZEST_FLAG     = 8
};

enum sym_parameter_types {
	ZEST_STRING          = 0,
	ZEST_INT             = 1,
	ZEST_BOOL            = 2,
	ZEST_FLOAT           = 3,
	ZEST_SIZET           = 4,
	ZEST_TYPE_RECIPE     = 5,
	ZEST_TYPE_GROUP      = 6,
	ZEST_TYPE_RECURSE    = 7,
	ZEST_HEXU64	     = 8,
	ZEST_NONE            = 9
};

/*
 * Symbol tables for our reserved words
 */
#define FUNCNAME_MAX 64
#define OPTNAME_MAX  64

struct symtable {
	char *name;
	enum  sym_types sym_type;
	enum  sym_parameter_types sym_param_type;
	int  param;
	int  offset;
};

struct symtable * get_symbol(const char *);
void store_tok_val(const char *, char *);
