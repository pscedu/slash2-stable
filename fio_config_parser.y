/* $Id$ */

%{
#include <err.h>
#include <stdarg.h>

#include "fio.h"
#include "fio_sym.h"

int yylex(void);
int yyerror(const char *, ...);
int yyparse(void);

int lineno;
int errors;

extern GROUP_t *currentGroup;
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

group_blocks:	/* NULL */ |
		group_blocks group_block {};

group_block:	group_start statements GROUP_END {
	ASSERT(currentGroup->num_pes);

#ifdef PTHREADS
	BDEBUG("Allocating %d Thread Structs\n",
	 currentGroup->num_pes);

	currentGroup->threads = malloc(sizeof(THREAD_t) *
				 currentGroup->num_pes);
#endif

	BDEBUG("Group '%s' processing complete\n",
	currentGroup->test_name);
};

group_start:	GROUP STR GROUP_START {
	GROUP_t        *group;

	if (!numGroups) {
		BDEBUG("initialize groupList\n");
		INIT_LIST_HEAD(&groupList);
	}
	numGroups++;

	BDEBUG("malloc'ing %zu bytes for testGroup inc_ptr %p\n",
	    sizeof(GROUP_t), $2);

	BDEBUG("\nGroup Name = '%s'\n", $2);

	group = malloc(sizeof(GROUP_t));
	ASSERT(group != NULL);
	bzero(group, sizeof(GROUP_t));
	INIT_LIST_HEAD(&group->group_list);
	list_add_tail(&group->group_list, &groupList);
	strncpy(group->test_name, $2, TEST_GROUP_NAME_MAX);
	currentGroup = group;

	BDEBUG("New Group Declared: Name '%s' Addr %p\n",
	    group->test_name, group);
	free($2);
};

statements:	/* NULL */ |
		statement statements |
		subsect_block statements ;

statement:	val_stmt |
		bool_stmt  |
		num_stmt  |
		size_stmt  |
		float_stmt ;

val_stmt:	STR EQ STR END {
	BDEBUG("Found Name/Val Statement: Tok '%s' Val '%s'\n",
	    $1, $3);
	store_tok_val($1, $3);
	free($1);
	free($3);
};

num_stmt:	STR EQ NUM END {
	BDEBUG("Found Name/Val Statement: Tok '%s' Val '%s'\n",
	    $1, $3);
	store_tok_val($1, $3);
	free($1);
	free($3);
};

bool_stmt:	STR EQ BOOL END {
	BDEBUG("Found Bool Statement: Tok '%s' Val '%s'\n",
	    $1, $3);
	store_tok_val($1, $3);
	free($1);
	free($3);
};

size_stmt:	STR EQ SIZEVAL END {
	BDEBUG("Found Sizeval Statement: Tok '%s' Val '%s'\n",
	    $1, $3);
	store_tok_val($1, $3);
	free($1);
	free($3);
};

float_stmt:	STR EQ FLOATVAL END {
	BDEBUG("Found Float Statement: Tok '%s' Val '%s'\n",
	    $1, $3);
	store_tok_val($1, $3);
	free($1);
	free($3);
};

subsect_block:	subsect_begin statements SUBSECT_END |
		iotests SUBSECT_END {};

subsect_begin:	SUB STR SUBSECT_START {
	struct symtable_t *sym = get_symbol($2);

	if (sym == NULL)
		err(1, "invalid config entry '%s'", $2);
	free($2);
};

iotests:	IOTESTS SUBSECT_START test_recipes {};

test_recipes:	/* NULL */ |
		test_recipe test_recipes {};

test_recipe:	test_recipe_decl test_recipe_comps RECIPE_END {
	currentGroup->num_iotests++;
};


test_recipe_decl: STR RECIPE_START {
	int num_iotests          =  currentGroup->num_iotests;
	struct io_routine_t *ior = &currentGroup->iotests[num_iotests];

	if (num_iotests >= MAXTESTS)
		errx(1, "num_iotests (%d) >= MAXTESTS (%d)",
		    num_iotests, MAXTESTS);

	strncpy(ior->io_testname, $1, TEST_GROUP_NAME_MAX);
	BDEBUG("Got test name '%s'\n", ior->io_testname);
	free($1);
};

test_recipe_comps:	/* NULL */ |
			test_recipe_comp test_recipe_comps |
			test_recipe_scomp test_recipe_comps {};

test_recipe_scomp:	STR {
	store_func($1);
	free($1);
};

test_recipe_comp:	STR RECIPE_SEP {
	store_func($1);
	free($1);
};

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
