/* $Id$ */

%{
#include <stdio.h>
#include <string.h>

#define psclog_trace(fmt, ...)
#define PFL_RETURNX()		return
#define PFL_RETURN(rc)		return (rc)
#define PFL_RETURN_LIT(s)	return (s)
#define PFL_RETURN_STR(s)	return (s)
%}

%%

.		{ }

%%

int
yywrap(void)
{
	return (1);
}

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	exit(0);
}
