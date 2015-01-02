/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2014, Pittsburgh Supercomputing Center (PSC).
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

#include "fio.h"
#include "sym.h"

/* declare and initialize the global table */
struct symtable sym_table[] = {
 { "app_barrier",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_APP_BARRIER },
 { "barrier",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_BARRIER },
 { "block_barrier",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_BLOCK_BARRIER },
 { "block_freq",	FIO_VARIABLE,	FIOT_FLOAT,	NULL,		offsetof(GROUP_t, block_freq),		PATH_MAX },
 { "block_size",	FIO_VARIABLE,	FIOT_SIZET,	NULL,		offsetof(GROUP_t, block_size),		4 },
 { "close",		FIO_FUNCTION,	FIOT_NONE,	do_close,	0,					0 },
 { "creat",		FIO_FUNCTION,	FIOT_NONE,	do_creat,	0,					0 },
 { "create",		FIO_FUNCTION,	FIOT_NONE,	do_creat,	0,					0 },
 { "debug_barrier",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_BARRIER },
 { "debug_block",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_BLOCK },
 { "debug_buffer",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_BUFFER },
 { "debug_conf",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_CONF },
 { "debug_dtree",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_DTREE },
 { "debug_iofunc",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_IOFUNC },
 { "debug_memory",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_MEMORY },
 { "debug_output",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_OUTPUT },
 { "debug_symtable",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, debug_flags),		FIO_DBG_SYMTBL },
 { "file_size",		FIO_VARIABLE,	FIOT_SIZET,	NULL,		offsetof(GROUP_t, file_size),		4 },
 { "filename",		FIO_VARIABLE,	FIOT_STRING,	NULL,		offsetof(GROUP_t, test_filename),	PATH_MAX },
 { "files_per_dir",	FIO_VARIABLE,	FIOT_INT,	NULL,		offsetof(GROUP_t, files_per_dir),	1 },
 { "files_per_pe",	FIO_VARIABLE,	FIOT_INT,	NULL,		offsetof(GROUP_t, files_per_pe),	1 },
 { "fstat",		FIO_FUNCTION,	FIOT_NONE,	do_fstat,	0,					0 },
 { "fsync_block",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_FSYNC_BLOCK },
 { "ftrunc",		FIO_FUNCTION,	FIOT_NONE,	do_trunc,	0,					0 },
 //{ "group",		FIO_,		FIOT_TYPE_GROUP,	NULL,		0,					0 },
 { "intersperse",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIOT_INTERSPERSE },
 { "iterations",	FIO_VARIABLE,	FIOT_INT,	NULL,		offsetof(GROUP_t, iterations),		1 },
 { "link",		FIO_FUNCTION,	FIOT_NONE,	do_link,	0,					0 },
 { "openap",		FIO_FUNCTION,	FIOT_NONE,	do_open,	0,					O_APPEND | O_WRONLY },
 { "openrd",		FIO_FUNCTION,	FIOT_NONE,	do_open,	0,					O_RDONLY },
 { "openrw",		FIO_FUNCTION,	FIOT_NONE,	do_open,	0,					O_RDWR   },
 { "openwr",		FIO_FUNCTION,	FIOT_NONE,	do_open,	0,					O_WRONLY },
 { "output_path",	FIO_VARIABLE,	FIOT_STRING,	NULL,		offsetof(GROUP_t, output_path),		PATH_MAX },
 { "path",		FIO_VARIABLE,	FIOT_STRING,	NULL,		offsetof(GROUP_t, test_path),		PATH_MAX },
 { "pes",		FIO_VARIABLE,	FIOT_INT,	NULL,		offsetof(GROUP_t, num_pes),		4 },
 { "random",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_RANDOM },
 { "read",		FIO_FUNCTION,	FIOT_NONE,	do_read,	0,					0 },
 { "rename",		FIO_FUNCTION,	FIOT_NONE,	do_rename,	0,					0 },
 { "samedir",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_SAMEDIR },
 { "samefile",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_SAMEFILE },
 { "seekoff",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_SEEKOFF },
 { "stagger",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_STAGGER },
 { "stat",		FIO_FUNCTION,	FIOT_NONE,	do_stat,	0,					0 },
 { "test_freq",		FIO_VARIABLE,	FIOT_FLOAT,	NULL,		offsetof(GROUP_t, test_freq),		PATH_MAX },
// { "test_recipe",	FIO_,		FIOT_TYPE_RECIPE, NULL,		0,					0 },
 { "thrash_lock",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_THRASH_LOCK },
 { "time_barrier",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_TIME_BARRIER },
 { "time_block",	FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_TIME_BLOCK },
 { "tree_depth",	FIO_VARIABLE,	FIOT_INT,	NULL,		offsetof(GROUP_t, tree_depth),		1 },
 { "tree_width",	FIO_VARIABLE,	FIOT_INT,	NULL,		offsetof(GROUP_t, tree_width),		1 },
 { "trunc",		FIO_FUNCTION,	FIOT_NONE,	do_trunc,	0,					0 },
 { "unlink",		FIO_FUNCTION,	FIOT_NONE,	do_unlink,	0,					0 },
 { "verify",		FIO_FLAG,	FIOT_BOOL,	NULL,		offsetof(GROUP_t, test_opts),		FIO_VERIFY },
 { "write",		FIO_FUNCTION,	FIOT_NONE,	do_write,	0,					0 },
 { NULL,		0,		0,		NULL,		0,					0 }
};
