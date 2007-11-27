/* $Id: zestLexConfig.l 2041 2007-10-29 21:17:11Z pauln $ */

%{
#define YYSTYPE char *

#include <stdio.h>
#include <string.h>

#include "zestLog.h"
#include "zestYaccConfig.h"

extern int cfg_lineno;
extern char *yylval;
%}

eq   [=]
sep  [:]
nsep [,]
endl [;]

num      [0-9]+
hexnum   0x[0-9a-f]+
char     [A-Za-z]
name     [A-Za-z][0-9A-Za-z_.-]+
floatv   [0-9]*\.?[0-9]+

sizeval  [0-9]+[KMGTBkmgtb]
pathname [/][a-zA-Z0-9/._-]+
globpath [/][a-zA-Z0-9|/.\,_\[\]+?-]+

%%

none {
        ztrace("NONE");
	return NONE;
}

NONE {
        ztrace("NONE");
	return NONE;
}

";" {
	return END;
}

":" {
	return SEP;
}

"," {
	return NSEP;
}

"=" {
	return EQ;
}

zestion_nodes {
	ztrace("ZNODES %s", yytext);
	return ZNODES;
}

zestion_node_profile {
	ztrace("ZNODE_PROFILE %s", yytext);
        return ZNODE_PROFILE;
}

"{" {
	ztrace("SUBSECT_START TAG %s", yytext);
	return SUBSECT_START;
}

"}" {
	ztrace("SUBSECT_END TAG %s", yytext);
	return SUBSECT_END;
}

yes {
	ztrace("BOOL %s", yytext);
	yylval = strdup(yytext);
	return BOOL;
}

no {
	ztrace("BOOL %s", yytext);
	yylval = strdup(yytext);
	return BOOL;
}

{num} {
	ztrace("NUM %s", yytext);
	yylval = strdup(yytext);
	return NUM;
}

{hexnum} {
	ztrace("HEXNUM %s", yytext);
	yylval = strdup(yytext);
	return HEXNUM;
}

{name} {
	ztrace("NAME %s", yytext);
	yylval = strdup(yytext);
	return NAME;
}

{pathname} {
	ztrace("PATHNAME %s", yytext);
	yylval = strdup(yytext);
	return PATHNAME;
}

{globpath} {
        ztrace("GLOBPATH %s", yytext);
        yylval = strdup(yytext);
        return GLOBPATH;
}

{floatv} {
	ztrace("FLOATVAL %s", yytext);
        yylval = strdup(yytext);
        return FLOATVAL;
}

{sizeval} {
	ztrace("SIZEVAL %s", yytext);
        yylval = strdup(yytext);
        return SIZEVAL;
}

[ \t]*#.*	;
[ \t]+		;
[\n]		cfg_lineno++;

.		zwarnx("Unrecognized character: ;%s;", yytext);

%%

int yywrap(void)
{
	return 1;
}
