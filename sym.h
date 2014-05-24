/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

struct io_toolbox;

enum sym_types {
	FIO_FUNCTION		= 1,
	FIO_VARIABLE		= 2,
	FIO_METATAG		= 4,
	FIO_FLAG		= 8
};

enum sym_parameter_types {
	FIO_STRING,
	FIO_INT,
	FIO_BOOL,
	FIO_FLOAT,
	FIO_SIZET,
	FIO_TYPE_RECIPE,
	FIO_TYPE_GROUP,
	FIO_NONE
};

#define	FIO_GROUP_TAG		001
#define	FIO_RECURSE_TAG		002
#define	FIO_TEST_RECIPE_TAG	004

/*
 * Symbol tables for our reserved words
 */
#define FUNCNAME_MAX		64
#define OPTNAME_MAX		64

struct symtable {
	char				 *name;
	enum sym_types			  sym_type;
	enum sym_parameter_types	  sym_param_type;
	int				(*io_func)(struct io_toolbox *);
	int				  offset;
	int				  param;
};

struct symtable *get_symbol(const char *);
void store_tok_val(const char *, char *);
void store_func(const char *);

extern struct symtable sym_table[];
