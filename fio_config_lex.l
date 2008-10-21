/* $Id$ */

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

\n	{ lineno++; }
[ \t]+	;

.	yyerror("Unrecognized character: %s\n", yytext);

%%

int
yywrap(void)
{
	return 1;
}
