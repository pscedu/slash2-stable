/* $Id: zestYaccConfig.y 2189 2007-11-07 22:18:18Z yanovich $ */

%{
#define YYSTYPE char *

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "zestAlloc.h"
#include "zestAssert.h"
#include "zestConfig.h"
#include "zestConfigSym.h"
#include "zestLog.h"
#include "ciod.h"

extern int  yylex(void);
extern void yyerror(const char *, ...);
extern int  yyparse(void);

extern void store_znode_decl(char *znode_name,
			     char *znode_profile_name,
			     char *znode_id,
			     char *znode_twin_id);

int have_profile;
int errors;
int cfg_lineno;
const char *cfg_filename;
%}

%start config

%token END
%token EQ

%token SEP
%token NSEP

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

%token NUM
%token HEXNUM
%token NAME
%token PATHNAME
%token GLOBPATH
%token BOOL
%token SIZEVAL
%token FLOATVAL

%token ZNODES
%token ZNODE_PROFILE

%token NONE

%%
config       : /* NULL */               |
               znode_group znode_profiles
{};

znode_group  : ZNODES SUBSECT_START znode_decls SUBSECT_END
{};

znode_decls  : /* NULL */                  |
               znode_decl NSEP znode_decls |
               znode_decl
{};

znode_decl   : NAME SEP HEXNUM SEP HEXNUM SEP NAME
                   { store_znode_decl($1, $7, $3, $5); }   |
               NAME SEP NUM    SEP NUM    SEP NAME
                   { store_znode_decl($1, $7, $3, $5); }   |
               NAME SEP HEXNUM SEP NONE   SEP NAME
                   { store_znode_decl($1, $7, $3, NULL); } |
               NAME SEP NUM    SEP NONE   SEP NAME
                   { store_znode_decl($1, $7, $3, NULL); };

znode_profiles : /* NULL */ |
                 znode_profile znode_profiles;

znode_profile  : ZNODE_PROFILE NAME SUBSECT_START statements SUBSECT_END
{
	if ( !strncmp(zestNodeInfo->znode_prof, $2, ZNPROF_NAME_MAX) ) {
		/*
		 * Found profile match
		 */
		have_profile = 1;

		znotify("Located profile ;%s; for node ;%s;",
		     $2, zestNodeInfo->znode_name);

		strncpy(zestNodeProfile->znprof_name, $2,
			ZNPROF_NAME_MAX);
		zestNodeProfile->znprof_name[ZNPROF_NAME_MAX - 1] = '\0';
	}
	free($2);
};

statements        : /* NULL */               |
                    statement statements;

statement         : path_stmt  |
                    num_stmt   |
                    bool_stmt  |
                    size_stmt  |
                    glob_stmt  |
                    hexnum_stmt|
                    float_stmt ;


path_stmt : NAME EQ PATHNAME END
{
	znotify("Found Path Statement: Tok '%s' Val '%s'",
	       $1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

glob_stmt : NAME EQ GLOBPATH END
{
        znotify("Found Glob Statement: Tok '%s' Val '%s'",
               $1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

bool_stmt : NAME EQ BOOL END
{
	znotify("Found Bool Statement: Tok '%s' Val '%s'",
	       $1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

size_stmt : NAME EQ SIZEVAL END
{
	znotify("Found Sizeval Statement: Tok '%s' Val '%s'",
	       $1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

num_stmt : NAME EQ NUM END
{
        znotify("Found Num Statement: Tok '%s' Val '%s'",
		$1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

float_stmt : NAME EQ FLOATVAL END
{
	znotify("Found Float Statement: Tok '%s' Val '%s'",
	       $1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

hexnum_stmt : NAME EQ HEXNUM END
{
	znotify("Found Hexnum Statement: Tok '%s' Val '%s'",
	       $1, $3);
	if (!have_profile)
		store_tok_val($1, $3);
	free($1);
	free($3);
};

%%

void store_znode_decl(char *znode_name,
		      char *znode_profile_name,
		      char *znode_id,
		      char *znode_twin_id)
{
	ztrace("znode_decl hostname ;%s; ;%s;",
		zestHostname, znode_name);
	/*
	 * Look at znode_name for a match
	 */
	if ( !strncmp(zestHostname, znode_name, MAXHOSTNAMELEN) ) {

		strncpy(zestNodeInfo->znode_name,
			znode_name, ZNODE_NAME_MAX);
		zestNodeInfo->znode_name[ZNODE_NAME_MAX - 1] = '\0';

		if (zestNodeInfo->znode_prof[0] == '\0') {
			strncpy(zestNodeInfo->znode_prof,
			    znode_profile_name, ZNPROF_NAME_MAX);
			zestNodeInfo->znode_prof[ZNPROF_NAME_MAX - 1] = '\0';
		}

		zestNodeInfo->znode_id = strtol(znode_id, NULL, 0);

		if (znode_twin_id != NULL)
			zestNodeInfo->znode_twin_id = strtol(znode_twin_id, NULL, 0);

		znotify("Node ;%s; : Id 0x%hx : Twin 0x%hx : Profile ;%s;\n",
			zestNodeInfo->znode_name,
			zestNodeInfo->znode_id,
			zestNodeInfo->znode_twin_id,
			zestNodeInfo->znode_prof);
	}
	free(znode_name);
	free(znode_profile_name);
	free(znode_id);
	free(znode_twin_id);
}

int run_yacc(const char *config_file, const char *prof)
{
	extern FILE *yyin;

	have_profile = 0;

	yyin = fopen(config_file, "r");
	if (yyin == NULL)
		zfatal("open() failed ;%s;", config_file);

	/*
	 * Setup our node structure
	 */
	zestHostname = ZALLOC(MAXHOSTNAMELEN);
	zestNodeInfo = ZALLOC(sizeof(znode_t));
	zestNodeProfile = ZALLOC(sizeof(znode_prof_t));

	if (prof)
		snprintf(zestNodeInfo->znode_prof,
		    sizeof(zestNodeInfo->znode_prof), "%s", prof);

	zest_assert( (!gethostname(zestHostname, MAXHOSTNAMELEN)) );
	znotify("got hostname ;%s;", zestHostname);

	zestNodeInfo->znode_profile = zestNodeProfile;

	cfg_filename = config_file;
	yyparse();

	fclose(yyin);

	if (errors)
		zfatalx("%d error(s) encountered", errors);

	/* Sanity checks */
	if (zestNodeProfile->znprof_bdcon_sz < sizeof(struct ciod_store))
		zfatalx("Fatal: znprof_bdcon_sz (%zu) container size is "
			"less than ciod_store sz (%zu)",
			zestNodeProfile->znprof_bdcon_sz,
			sizeof(struct ciod_store));

	return 0;
}

void
yyerror(const char *fmt, ...)
{
	char buf[LINE_MAX];
	va_list ap;

	errors++;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	zerrorx("%s:%d: %s", cfg_filename, cfg_lineno, buf);
}
