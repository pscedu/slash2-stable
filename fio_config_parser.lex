%{
#ifdef NEED_YYLVAL
#define YYSTYPE char *
extern char * yylval;
#endif
#include "fio.h"
#include "fio_config_parser.tab.h"
#include <stdio.h>
#include <string.h>
%}

eq   [=]
sep  [:]
endl [;]

num      [0-9]+
char     [A-Za-z]
name     [0-9A-Za-z_]+
float    [0-9]*\.?[0-9]+

sizeval  [0-9]+[KMGTBkmgtb]
pathname [a-zA-Z0-9\/\-\.\_]+

%%

[ \n\t]+ ;

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
  yylval=strdup(yytext);
  return BOOL; 
}

no { 
  CDEBUG("BOOL %s\n", yytext);
  yylval=strdup(yytext);
  return BOOL; 
}

{num} { 
  CDEBUG("NUM %s\n", yytext);
  yylval=strdup(yytext);
  return NAME; 
}

{eq} { 
  CDEBUG("EQ %s\n", yytext);
  return EQ; 
}

{endl} { 
  CDEBUG("END %s\n", yytext);
  return END;
}

{sizeval} { 
  CDEBUG("SIZEVAL %s\n", yytext);
  yylval=strdup(yytext);
  return SIZEVAL; 
}

{float} { 
  CDEBUG("FLOATVAL %s\n", yytext);
  yylval=strdup(yytext);
  return FLOATVAL; 
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

"[" { 
  CDEBUG("RECIPE_START %s\n", yytext); 
  return RECIPE_START;
}

"]" { 
  CDEBUG("RECIPE_END %s\n", yytext);
  return RECIPE_END;
}

{name} { 
  CDEBUG("NAME %s\n", yytext);
  yylval=strdup(yytext);
  CDEBUG("NAME1 %s %s %p\n", yytext, yylval, (void *)yylval);
  return NAME; 
}

{pathname} { 
  CDEBUG("PATHNAME %s\n", yytext);
  yylval=strdup(yytext);
  return PATHNAME; 
}

. WARN("Unrecognized character: %s\n", yytext);

%%


int yywrap()
{
  return 1;
}
