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

%{
#include <err.h>
#include <stdarg.h>

#include "pfl/str.h"
#include "pfl/alloc.h"
#include "pfl/log.h"

#include "fio.h"
#include "sym.h"

int yylex(void);
int yyerror(const char *, ...);
int yyparse(void);

int lineno;
int errors;

struct psclist_head groupList = PSCLIST_HEAD_INIT(groupList);
GROUP_t         *currentGroup;
int              numGroups;
%}

%start group_blocks

%token END
%token EQ

%token IOTESTS

%token GROUP
%token GROUP_START
%token GROUP_END

%token SUB
%token SUBSECT_START
%token SUBSECT_END

%token RECIPE_SEP
%token RECIPE_END
%token RECIPE_START

%token <string> NUM
%token <string> STR
%token <string> BOOL
%token <string> SIZEVAL
%token <string> FLOATVAL

%union {
	char *string;
	void *voidp;
};

%%

group_blocks		: /* NULL */
			| group_blocks group_block;

group_block		: group_start statements GROUP_END {
				ASSERT(currentGroup->num_pes);

#ifdef HAVE_LIBPTHREAD
				BDEBUG("Allocating %d Thread Structs\n",
				    currentGroup->num_pes);

				currentGroup->threads = psc_calloc(sizeof(THREAD_t),
				   currentGroup->num_pes, 0);
#endif

				BDEBUG("Group '%s' processing complete\n",
				    currentGroup->test_name);
			}
			;

group_start		: GROUP STR GROUP_START {
				GROUP_t        *group;

				numGroups++;

				BDEBUG("malloc'ing %zu bytes for testGroup inc_ptr %p\n",
				    sizeof(GROUP_t), $2);

				BDEBUG("\nGroup Name = '%s'\n", $2);

				group = PSCALLOC(sizeof(GROUP_t));
				INIT_PSC_LISTENTRY(&group->group_lentry);
				psclist_add_tail(&group->group_lentry, &groupList);
				strlcpy(group->test_name, $2, sizeof(group->test_name));
				currentGroup = group;

				BDEBUG("New Group Declared: Name '%s' Addr %p\n",
				    group->test_name, group);
				free($2);
			}
			;

statements		: /* NULL */
			| statement statements
			| subsect_block statements
			;

statement		: val_stmt
			| bool_stmt
			| num_stmt
			| size_stmt
			| float_stmt
			;

val_stmt		: STR EQ STR END {
				BDEBUG("Found Name/Val Statement: Tok '%s' Val '%s'\n",
				    $1, $3);
				store_tok_val($1, $3);
				free($1);
				free($3);
			}
			;

num_stmt		: STR EQ NUM END {
				BDEBUG("Found Num Statement: Tok '%s' Val '%s'\n",
				    $1, $3);
				store_tok_val($1, $3);
				free($1);
				free($3);
			}
			;

bool_stmt		: STR EQ BOOL END {
				BDEBUG("Found Bool Statement: Tok '%s' Val '%s'\n",
				    $1, $3);
				store_tok_val($1, $3);
				free($1);
				free($3);
			}
			;

size_stmt		: STR EQ SIZEVAL END {
				BDEBUG("Found Sizeval Statement: Tok '%s' Val '%s'\n",
				    $1, $3);
				store_tok_val($1, $3);
				free($1);
				free($3);
			}
			;

float_stmt		: STR EQ FLOATVAL END {
				BDEBUG("Found Float Statement: Tok '%s' Val '%s'\n",
				    $1, $3);
				store_tok_val($1, $3);
				free($1);
				free($3);
			}
			;

subsect_block		: subsect_begin statements SUBSECT_END
			| iotests SUBSECT_END;

subsect_begin		: SUB STR SUBSECT_START {
				struct symtable *sym = get_symbol($2);

				if (sym == NULL)
					err(1, "invalid config entry '%s'", $2);
				free($2);
			}
			;

iotests			: IOTESTS SUBSECT_START test_recipes;

test_recipes		: /* NULL */
			| test_recipe test_recipes;

test_recipe		: test_recipe_decl test_recipe_comps RECIPE_END {
				currentGroup->num_iotests++;
			}
			;

test_recipe_decl	: STR RECIPE_START {
				int num_iotests        =  currentGroup->num_iotests;
				struct io_routine *ior = &currentGroup->iotests[num_iotests];

				if (num_iotests >= MAXTESTS)
					errx(1, "num_iotests (%d) >= MAXTESTS (%d)",
					    num_iotests, MAXTESTS);

				strlcpy(ior->io_testname, $1, sizeof(ior->io_testname));
				BDEBUG("Got test name '%s'\n", ior->io_testname);
				free($1);
			}
			;

test_recipe_comps	: /* NULL */
			| test_recipe_comp test_recipe_comps
			| test_recipe_scomp test_recipe_comps;

test_recipe_scomp	: STR {
				store_func($1);
				free($1);
			}
			;

test_recipe_comp:	STR RECIPE_SEP {
				store_func($1);
				free($1);
			}
			;

%%

int
run_yacc(void)
{
	yyparse();
	if (errors)
		errx(1, "errors encountered");
	return 0;
}

int
yyerror(const char *msg, ...)
{
	va_list ap;

	printf("line %d: error encountered: ", lineno);

	va_start(ap, msg);
	vprintf(msg, ap);
	va_end(ap);

	printf("\n");

	errors = 1;
	return 0;
}
