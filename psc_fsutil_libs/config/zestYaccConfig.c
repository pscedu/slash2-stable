#ifndef lint
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.28 2000/01/17 02:04:06 bde Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
static int yygrowstack();
#define YYPREFIX "yy"
#line 4 "../config/zestYaccConfig.y"
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
#line 47 "../config/zestYaccConfig.c"
#define YYERRCODE 256
#define END 257
#define EQ 258
#define SEP 259
#define NSEP 260
#define IOTESTS 261
#define GROUP 262
#define GROUP_START 263
#define GROUP_END 264
#define SUB 265
#define SUBSECT_START 266
#define SUBSECT_END 267
#define RECIPE_SEP 268
#define RECIPE_END 269
#define RECIPE_START 270
#define NUM 271
#define HEXNUM 272
#define NAME 273
#define PATHNAME 274
#define GLOBPATH 275
#define BOOL 276
#define SIZEVAL 277
#define FLOATVAL 278
#define ZNODES 279
#define ZNODE_PROFILE 280
#define NONE 281
const short yylhs[] = {                                        -1,
    0,    0,    1,    3,    3,    3,    4,    4,    4,    4,
    2,    2,    5,    6,    6,    7,    7,    7,    7,    7,
    7,    7,    8,   12,   10,   11,    9,   14,   13,
};
const short yylen[] = {                                         2,
    0,    2,    4,    0,    3,    1,    7,    7,    7,    7,
    0,    2,    5,    0,    2,    1,    1,    1,    1,    1,
    1,    1,    4,    4,    4,    4,    4,    4,    4,
};
const short yydefred[] = {                                      0,
    0,    0,    0,    0,    0,    2,    0,    0,    0,    0,
    0,   12,    0,    3,    0,    0,    0,    0,    5,    0,
    0,    0,   16,   17,   18,   19,   20,   21,   22,    0,
    0,    0,   13,   15,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   27,
   29,   23,   24,   25,   26,   28,    8,   10,    7,    9,
};
const short yydgoto[] = {                                       2,
    3,    6,    9,   10,    7,   21,   22,   23,   24,   25,
   26,   27,   28,   29,
};
const short yysindex[] = {                                   -278,
 -261,    0, -274, -266, -265,    0, -274, -250, -253, -240,
 -245,    0, -268,    0, -266, -251, -236, -235,    0, -233,
 -241, -251,    0,    0,    0,    0,    0,    0,    0, -271,
 -270, -259,    0,    0, -232, -231, -230, -229, -226, -225,
 -224, -223, -222, -221, -220, -234, -228, -227, -219,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
const short yyrindex[] = {                                     38,
    0,    0,   40, -218,    0,    0,   40,    0,    0, -217,
    0,    0,    0,    0, -218, -253,    0,    0,    0,    0,
    0, -253,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
const short yygindex[] = {                                      0,
    0,   34,   27,    0,    0,   21,    0,    0,    0,    0,
    0,    0,    0,    0,
};
#define YYTABLESIZE 54
const short yytable[] = {                                      35,
    1,   37,   17,   18,    4,    5,    8,   11,   13,   36,
   38,   39,   40,   14,   41,   42,   43,   44,   45,   15,
   16,   20,   30,   31,   32,   33,   46,   47,   48,   49,
   50,   51,   52,   53,   54,   55,   56,    1,   57,   11,
   12,   19,   34,    0,   58,   59,    0,    0,    4,    6,
    0,    0,    0,   60,
};
const short yycheck[] = {                                     271,
  279,  272,  271,  272,  266,  280,  273,  273,  259,  281,
  281,  271,  272,  267,  274,  275,  276,  277,  278,  260,
  266,  273,  259,  259,  258,  267,  259,  259,  259,  259,
  257,  257,  257,  257,  257,  257,  257,    0,  273,    0,
    7,   15,   22,   -1,  273,  273,   -1,   -1,  267,  267,
   -1,   -1,   -1,  273,
};
#define YYFINAL 2
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 281
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"END","EQ","SEP","NSEP","IOTESTS",
"GROUP","GROUP_START","GROUP_END","SUB","SUBSECT_START","SUBSECT_END",
"RECIPE_SEP","RECIPE_END","RECIPE_START","NUM","HEXNUM","NAME","PATHNAME",
"GLOBPATH","BOOL","SIZEVAL","FLOATVAL","ZNODES","ZNODE_PROFILE","NONE",
};
const char * const yyrule[] = {
"$accept : config",
"config :",
"config : znode_group znode_profiles",
"znode_group : ZNODES SUBSECT_START znode_decls SUBSECT_END",
"znode_decls :",
"znode_decls : znode_decl NSEP znode_decls",
"znode_decls : znode_decl",
"znode_decl : NAME SEP HEXNUM SEP HEXNUM SEP NAME",
"znode_decl : NAME SEP NUM SEP NUM SEP NAME",
"znode_decl : NAME SEP HEXNUM SEP NONE SEP NAME",
"znode_decl : NAME SEP NUM SEP NONE SEP NAME",
"znode_profiles :",
"znode_profiles : znode_profile znode_profiles",
"znode_profile : ZNODE_PROFILE NAME SUBSECT_START statements SUBSECT_END",
"statements :",
"statements : statement statements",
"statement : path_stmt",
"statement : num_stmt",
"statement : bool_stmt",
"statement : size_stmt",
"statement : glob_stmt",
"statement : hexnum_stmt",
"statement : float_stmt",
"path_stmt : NAME EQ PATHNAME END",
"glob_stmt : NAME EQ GLOBPATH END",
"bool_stmt : NAME EQ BOOL END",
"size_stmt : NAME EQ SIZEVAL END",
"num_stmt : NAME EQ NUM END",
"float_stmt : NAME EQ FLOATVAL END",
"hexnum_stmt : NAME EQ HEXNUM END",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#if YYDEBUG
#include <stdio.h>
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
#line 197 "../config/zestYaccConfig.y"

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
#line 313 "../config/zestYaccConfig.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 2:
#line 74 "../config/zestYaccConfig.y"
{}
break;
case 3:
#line 77 "../config/zestYaccConfig.y"
{}
break;
case 6:
#line 82 "../config/zestYaccConfig.y"
{}
break;
case 7:
#line 85 "../config/zestYaccConfig.y"
{ store_znode_decl(yyvsp[-6], yyvsp[0], yyvsp[-4], yyvsp[-2]); }
break;
case 8:
#line 87 "../config/zestYaccConfig.y"
{ store_znode_decl(yyvsp[-6], yyvsp[0], yyvsp[-4], yyvsp[-2]); }
break;
case 9:
#line 89 "../config/zestYaccConfig.y"
{ store_znode_decl(yyvsp[-6], yyvsp[0], yyvsp[-4], NULL); }
break;
case 10:
#line 91 "../config/zestYaccConfig.y"
{ store_znode_decl(yyvsp[-6], yyvsp[0], yyvsp[-4], NULL); }
break;
case 13:
#line 97 "../config/zestYaccConfig.y"
{
	if ( !strncmp(zestNodeInfo->znode_prof, yyvsp[-3], ZNPROF_NAME_MAX) ) {
		/*
		 * Found profile match
		 */
		have_profile = 1;

		znotify("Located profile ;%s; for node ;%s;",
		     yyvsp[-3], zestNodeInfo->znode_name);

		strncpy(zestNodeProfile->znprof_name, yyvsp[-3],
			ZNPROF_NAME_MAX);
		zestNodeProfile->znprof_name[ZNPROF_NAME_MAX - 1] = '\0';
	}
	free(yyvsp[-3]);
}
break;
case 23:
#line 127 "../config/zestYaccConfig.y"
{
	znotify("Found Path Statement: Tok '%s' Val '%s'",
	       yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
case 24:
#line 137 "../config/zestYaccConfig.y"
{
        znotify("Found Glob Statement: Tok '%s' Val '%s'",
               yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
case 25:
#line 147 "../config/zestYaccConfig.y"
{
	znotify("Found Bool Statement: Tok '%s' Val '%s'",
	       yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
case 26:
#line 157 "../config/zestYaccConfig.y"
{
	znotify("Found Sizeval Statement: Tok '%s' Val '%s'",
	       yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
case 27:
#line 167 "../config/zestYaccConfig.y"
{
        znotify("Found Num Statement: Tok '%s' Val '%s'",
		yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
case 28:
#line 177 "../config/zestYaccConfig.y"
{
	znotify("Found Float Statement: Tok '%s' Val '%s'",
	       yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
case 29:
#line 187 "../config/zestYaccConfig.y"
{
	znotify("Found Hexnum Statement: Tok '%s' Val '%s'",
	       yyvsp[-3], yyvsp[-1]);
	if (!have_profile)
		store_tok_val(yyvsp[-3], yyvsp[-1]);
	free(yyvsp[-3]);
	free(yyvsp[-1]);
}
break;
#line 632 "../config/zestYaccConfig.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
