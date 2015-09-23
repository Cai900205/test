/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

%{

// #define DEBUG 1
#define	yymaxdepth ibnl_maxdepth
#define	yyparse	ibnl_parse
#define	yylex	ibnl_lex
#define	yyerror	ibnl_error
#define	yylval	ibnl_lval
#define	yychar	ibnl_char
#define	yydebug	ibnl_debug
#define	yypact	ibnl_pact
#define	yyr1	ibnl_r1
#define	yyr2	ibnl_r2
#define	yydef	ibnl_def
#define	yychk	ibnl_chk
#define	yypgo	ibnl_pgo
#define	yyact	ibnl_act
#define	yyexca	ibnl_exca
#define  yyerrflag ibnl_errflag
#define  yynerrs	ibnl_nerrs
#define	yyps	ibnl_ps
#define	yypv	ibnl_pv
#define	yys	ibnl_s
#define	yy_yys	ibnl_yys
#define	yystate	ibnl_state
#define	yytmp	ibnl_tmp
#define	yyv	ibnl_v
#define	yy_yyv	ibnl_yyv
#define	yyval	ibnl_val
#define	yylloc	ibnl_lloc
#define yyreds	ibnl_reds
#define yytoks	ibnl_toks
#define yylhs	ibnl_yylhs
#define yylen	ibnl_yylen
#define yydefred ibnl_yydefred
#define yydgoto	ibnl_yydgoto
#define yysindex ibnl_yysindex
#define yyrindex ibnl_yyrindex
#define yygindex ibnl_yygindex
#define yytable	 ibnl_yytable
#define yycheck	 ibnl_yycheck
#define yyname   ibnl_yyname
#define yyrule   ibnl_yyrule

#include <string.h>
#include <stdio.h>
#include "Fabric.h"
#include "ibnl_parser.h"
#include "SysDef.h"
extern long lineNum;
%}

%%

([1-9][0-9]*|0) {
  yylval.ival = atoi(yytext);
#ifdef DEBUG
  printf("INT:%d\n",yylval.ival);
#endif
  return INT;
}

SYSTEM {
  yylval.ival = SYSTEM;
#ifdef DEBUG
  printf("SYSTEM\n");
#endif
  return SYSTEM;
}

TOPSYSTEM {
  yylval.ival = TOPSYSTEM;
#ifdef DEBUG
  printf("TOPSYSTEM\n");
#endif
  return TOPSYSTEM;
}

NODE {
  yylval.ival = NODE;
#ifdef DEBUG
  printf("NODE\n");
#endif
  return NODE;
}

1x|4x|8x|12x {
	 yylval.sval = (char *)malloc(strlen(yytext) + 1);
    strcpy(yylval.sval, yytext);
#ifdef DEBUG
	 printf("WIDTH:%s\n",yylval.sval);
#endif
  return WIDTH;
}

2.5G|5G|10G {
    yylval.sval = (char *)malloc(strlen(yytext));
    strncpy(yylval.sval, yytext, strlen(yytext) - 1);
    yylval.sval[strlen(yytext)-1]='\0';
#ifdef DEBUG
	 printf("SPEED:%s\n",yylval.sval);
#endif
  return SPEED;
}

SUBSYSTEM {
  yylval.ival = SUBSYSTEM;
#ifdef DEBUG
  printf("SUBSYSTEM\n");
#endif
  return SUBSYSTEM;
}

CFG: {
  yylval.ival = CFG;
#ifdef DEBUG
  printf("CFG\n");
#endif
  return CFG;
}

(SW|CA|HCA) {
  if (!strcmp(yytext,"SW")) {
    yylval.tval = IB_SW_NODE;
  } else {
    yylval.tval = IB_CA_NODE;
  }
#ifdef DEBUG
  printf("%s\n", yytext);
#endif
  return NODETYPE;
}

[A-Za-z][-\[\]\\\*/A-Za-z0-9_.:%@~]+ {
	 yylval.sval = (char *)malloc(strlen(yytext) + 1);
    strcpy(yylval.sval, yytext);
#ifdef DEBUG
	 printf("NAME:%s\n",yylval.sval);
#endif
	 return (NAME);
}

\n {
  lineNum++;
#ifdef DEBUG
  printf("LINE\n");
#endif
  if(lineNum % 10000==0)
	 fprintf(stderr,"-I- Parsed %ld lines\r",lineNum);
  yylval.ival = LINE;
  return(LINE);
}

[ \t]+ {}

. {
#ifdef DEBUG
  printf("CHAR:%c\n",yytext[0]);
#endif
  return(yytext[0]);
}

%%

int yywrap ()
{
  return (1);
}

