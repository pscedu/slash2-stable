/* $Id$ */

%{
#define YYSTYPE char *

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void		 yyerror(const char *, ...);
int		 yylex(void);
int		 yyparse(void);
%}

%start config

%%

config		:
		;

%%

int
yylex(void)
{
	return (0);
}

int
main(int argc, char *argv[])
{
	yyparse();
	(void)argc;
	(void)argv;
	exit(0);
}

void
yyerror(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(1, fmt, ap);
	va_end(ap);
}
