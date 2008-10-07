/* $Id$ */

%{
#include <stdio.h>
#include <string.h>

#include "fio.h"
#include "fio_config_parser.tab.h"
%}

eq		[=]
sep		[:]
endl		[;]

num		[0-9]+
name		[0-9A-Za-z_]+
float		[0-9]*\.?[0-9]+

sizeval		[0-9]+[BKMGTbkmgt]
pathname	[a-zA-Z0-9/._-]+

%%

\n		{ lineno++; }
[ \t]+		;

group {
	CDEBUG("GROUP %s\n", yytext);
	return GROUP;
}

iotests {
	CDEBUG("IOTESTS %s\n", yytext);
	return IOTESTS;
}

sub {
	CDEBUG("SUB %s\n", yytext);
	return SUB;
}

yes {
	CDEBUG("BOOL %s\n", yytext);
	yylval.string=strdup(yytext);
	return BOOL;
}

no {
	CDEBUG("BOOL %s\n", yytext);
	yylval.string=strdup(yytext);
	return BOOL;
}

"{" {
	CDEBUG("GROUP_START TAG %s\n", yytext);
	return GROUP_START;
}

"}" {
	CDEBUG("GROUP_END TAG %s\n", yytext);
	return GROUP_END;
}

"(" {
	CDEBUG("SUBSECT_START TAG %s\n", yytext);
	return SUBSECT_START;
}

")" {
	CDEBUG("SUBSECT_END TAG %s\n", yytext);
	return SUBSECT_END;
}

{sep} {
	CDEBUG("RECIPE_SEP %s\n", yytext);
	return RECIPE_SEP;
}

{eq} {
	CDEBUG("EQ %s\n", yytext);
	return EQ;
}

{endl} {
	CDEBUG("END %s\n", yytext);
	return END;
}

"[" {
	CDEBUG("RECIPE_START %s\n", yytext);
	return RECIPE_START;
}

"]" {
	CDEBUG("RECIPE_END %s\n", yytext);
	return RECIPE_END;
}

{sizeval} {
	CDEBUG("SIZEVAL %s\n", yytext);
	yylval.string=strdup(yytext);
	return SIZEVAL;
}

{num} {
	CDEBUG("NUM %s\n", yytext);
	yylval.string=strdup(yytext);
	return NUM;
}

{float} {
	CDEBUG("FLOATVAL %s\n", yytext);
	yylval.string=strdup(yytext);
	return FLOATVAL;
}

{name} {
	CDEBUG("NAME %s\n", yytext);
	yylval.string=strdup(yytext);
	CDEBUG("NAME1 %s %s %p\n", yytext, yylval.string, yylval.voidp);
	return NAME;
}

{pathname} {
	CDEBUG("PATHNAME %s\n", yytext);
	yylval.string=strdup(yytext);
	return PATHNAME;
}

.	WARN("Unrecognized character: %s\n", yytext);

%%

int
yywrap(void)
{
	return 1;
}
