/* A Bison parser, made by GNU Bison 2.0.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     END = 258,
     EQ = 259,
     IOTESTS = 260,
     GROUP = 261,
     GROUP_START = 262,
     GROUP_END = 263,
     SUB = 264,
     SUBSECT_START = 265,
     SUBSECT_END = 266,
     RECIPE_SEP = 267,
     RECIPE_END = 268,
     RECIPE_START = 269,
     NUM = 270,
     NAME = 271,
     PATHNAME = 272,
     BOOL = 273,
     SIZEVAL = 274,
     FLOATVAL = 275
   };
#endif
#define END 258
#define EQ 259
#define IOTESTS 260
#define GROUP 261
#define GROUP_START 262
#define GROUP_END 263
#define SUB 264
#define SUBSECT_START 265
#define SUBSECT_END 266
#define RECIPE_SEP 267
#define RECIPE_END 268
#define RECIPE_START 269
#define NUM 270
#define NAME 271
#define PATHNAME 272
#define BOOL 273
#define SIZEVAL 274
#define FLOATVAL 275




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



