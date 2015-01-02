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

#include <stdio.h>
#include <string.h>

#include "pfl/str.h"

#include "fio.h"
#include "parse.h"

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
	yylval.string = pfl_strdup(yytext);
	return BOOL;
}

no {
	CDEBUG("BOOL %s\n", yytext);
	yylval.string = pfl_strdup(yytext);
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
	yylval.string = pfl_strdup(yytext);
	return SIZEVAL;
}

[0-9]+ {
	CDEBUG("NUM %s\n", yytext);
	yylval.string = pfl_strdup(yytext);
	return NUM;
}

[0-9]*\.?[0-9]+ {
	CDEBUG("FLOATVAL %s\n", yytext);
	yylval.string = pfl_strdup(yytext);
	return FLOATVAL;
}

[0-9A-Za-z_/.-]+ {
	CDEBUG("STR %s\n", yytext);
	yylval.string = pfl_strdup(yytext);
	return STR;
}

\"[^"]*\" {
	char *p;

	CDEBUG("STR %s\n", yytext);
	yylval.string = pfl_strdup(yytext + 1);
	p = strrchr(yylval.string, '"');
	*p = '\0';
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
