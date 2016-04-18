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

struct io_toolbox;

enum sym_types {
	FIO_FUNCTION		= 1,
	FIO_VARIABLE		= 2,
	FIO_METATAG		= 4,
	FIO_FLAG		= 8
};

enum sym_parameter_types {
	FIOT_STRING,
	FIOT_INT,
	FIOT_BOOL,
	FIOT_FLOAT,
	FIOT_SIZET,
	FIOT_TYPE_RECIPE,
	FIOT_TYPE_GROUP,
	FIOT_NONE
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
