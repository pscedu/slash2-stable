/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

%{
#include <stdio.h>
#include <string.h>

#include "fio.h"
#include "fio_config_parser.h"

int yyerror(const char *, ...);
%}

%%

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

= {
	CDEBUG("EQ %s\n", yytext);
	return EQ;
}

: {
	CDEBUG("RECIPE_SEP %s\n", yytext);
	return RECIPE_SEP;
}

; {
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

[0-9]+[BKMGTbkmgt] {
	CDEBUG("SIZEVAL %s\n", yytext);
	yylval.string=strdup(yytext);
	return SIZEVAL;
}

[0-9]+ {
	CDEBUG("NUM %s\n", yytext);
	yylval.string=strdup(yytext);
	return NUM;
}

[0-9]*\.?[0-9]+ {
	CDEBUG("FLOATVAL %s\n", yytext);
	yylval.string=strdup(yytext);
	return FLOATVAL;
}

[0-9A-Za-z_/.-]+ {
	CDEBUG("STR %s\n", yytext);
	yylval.string=strdup(yytext);
	return STR;
}

\"[^"]*\" {
	char *p;

	CDEBUG("STR %s\n", yytext);
	p = strrchr(yytext, '"');
	*p = '\0';
	yylval.string=strdup(yytext + 1);
	return STR;
}

[ \t]*#.*	;
[ \t]+		;
\n		{ lineno++; }

.		yyerror("Unrecognized character: %s\n", yytext);

%%

int
yywrap(void)
{
	return 1;
}
