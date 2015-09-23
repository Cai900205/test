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


/*

*/

%{

  /* header section */
#include <stdlib.h>
#include <stdio.h>
#include "SysDef.h"
#define YYERROR_VERBOSE 1

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

  extern int yyerror(char *msg);
  extern int yylex(void);


%}


%union {
  IBNodeType tval;
  int        ival;
  char      *sval;
}

%token INT SYSTEM TOPSYSTEM NODE SUBSYSTEM NODETYPE NAME SPEED WIDTH LINE
%token CFG

%type <sval> NAME WIDTH SPEED
%type <ival> INT SYSTEM NODE SUBSYSTEM TOPSYSTEM CFG
%type <tval> NODETYPE

%start ibnl

%{

  static int ibnlErr;
  long lineNum;
  static const char *gp_fileName;
  static int gIsTopSystem = 0;
  static list< char * > gSysNames;
  static IBSystemsCollection *gp_sysColl = 0;
  static IBSysDef *gp_curSysDef = 0;
  static IBSysInst *gp_curInstDef = 0;

  void ibnlMakeSystem(list< char * > &sysNames) {
#ifdef DEBUG
    printf("Making new system named:");
#endif
    gp_curSysDef = new IBSysDef(gp_fileName);

    for( list< char * >::iterator snI = sysNames.begin();
         snI != sysNames.end(); snI++) {
      char sname[1024];
      if (gIsTopSystem) {
        sprintf(sname, "%s", *snI);
      } else {
        sprintf(sname, "%s/%s", gp_fileName, *snI);
      }
      string sNameStr(sname);
      gp_sysColl->addSysDef(sNameStr, gp_curSysDef);
#ifdef DEBUG
      printf("%s ", sname);
#endif
    }
#ifdef DEBUG
    printf("\n");
#endif

    // cleanup for next systems.
    sysNames.erase(sysNames.begin(), sysNames.end());
  }

  void ibnlMakeSubInstAttribute(char *hInst, char *attr, char *value) {
#ifdef DEBUG
    printf("Making new sub instance attribute inst:%s %s=%s\n",
           hInst, attr, value);
#endif
    if (! gp_curSysDef) {
        printf("-E- How com e we got no system???\n");
        exit(3);
    }
    // append to existing attr or create new
    string hierInstName = string(hInst);
    string attrStr = string(attr);
    if (value)
       attrStr += "=" +  string(value);
    gp_curSysDef->setSubInstAttr(hierInstName, attrStr);
  }

  void ibnlMakeNode(IBNodeType type, int numPorts, char *devName, char* name) {
#ifdef DEBUG
    printf(" Making Node:%s dev:%s ports:%d\n", name, devName, numPorts);
#endif
    gp_curInstDef = new IBSysInst(name, devName, numPorts, type);
    gp_curSysDef->addInst(gp_curInstDef);
  }

  void ibnlMakeNodeToNodeConn(
    int fromPort, const char *width, char *speed, const char *toNode, int toPort) {
#ifdef DEBUG
    printf("  Connecting N-N port:%d to Node:%s/%d (w=%s,s=%s)\n",
           fromPort, toNode, toPort, width, speed);
#endif
    char buf1[8],buf2[8] ;
    sprintf(buf1, "%d", toPort);
    sprintf(buf2, "%d", fromPort);
    IBSysInstPort *p_port =
      new IBSysInstPort(buf2, toNode, buf1, char2width(width),
                        char2speed(speed));
    gp_curInstDef->addPort(p_port);
  }

  void ibnlMakeNodeToPortConn(
    int fromPort, const char *width, const char *speed, const char *sysPortName) {
#ifdef DEBUG
    printf("  System port:%s on port:%d (w=%s,s=%s)\n",
           sysPortName, fromPort, width, speed);
#endif
    char buf[8];
    sprintf(buf,"%d",fromPort);
    IBSysPortDef *p_sysPort =
      new IBSysPortDef(sysPortName, gp_curInstDef->getName(), buf,
                       char2width(width), char2speed(speed));
    gp_curSysDef->addSysPort(p_sysPort);
  }

  void ibnlMakeSubsystem( char *masterName, char *instName) {
#ifdef DEBUG
    printf(" Making SubSystem:%s of type:%s\n", instName, masterName);
#endif
    gp_curInstDef = new IBSysInst(instName, masterName);
    gp_curSysDef->addInst(gp_curInstDef);
  }

  void ibnlRecordModification( char *subSystem, char *modifier) {
#ifdef DEBUG
    printf("  Using modifier:%s on %s\n", modifier, subSystem);
#endif
    gp_curInstDef->addInstMod(subSystem, modifier);
  }

  void ibnlMakeSubsystemToSubsystemConn(
    char *fromPort, char *width, char *speed, char *toSystem, char *toPort) {
#ifdef DEBUG
    printf("  Connecting S-S port:%s to SubSys:%s/%s\n",
         fromPort, toSystem, toPort);
#endif
    IBSysInstPort *p_port =
      new IBSysInstPort(fromPort, toSystem, toPort, char2width(width),
                        char2speed(speed));
    gp_curInstDef->addPort(p_port);
  }

  void ibnlMakeSubsystemToPortConn(
    const char *fromPort, const char *width, const char *speed, const char *toPort) {
#ifdef DEBUG
    printf("  Connecting port:%s to SysPort:%s\n",
         fromPort, toPort);
#endif

    IBSysPortDef *p_sysPort =
      new IBSysPortDef(toPort, gp_curInstDef->getName(), fromPort,
                       char2width(width), char2speed(speed));
    gp_curSysDef->addSysPort(p_sysPort);
  }

%}
%%

NL:
    LINE
  | NL LINE;

ONL:
  | NL;

ibnl: ONL systems topsystem;

systems:
  | systems system
  ;

sub_inst_attributes:
  | sub_inst_attributes sub_inst_attribute NL
  ;

sub_inst_attribute:
  NAME '=' NAME '=' NAME { ibnlMakeSubInstAttribute($1,$3,$5); }
  | NAME '=' NAME '=' INT {
      char buf[16];
      sprintf(buf, "%d", $5);
      ibnlMakeSubInstAttribute($1,$3,buf);
   }
  | NAME '=' NAME {ibnlMakeSubInstAttribute($1,$3,NULL); }
  ;

topsystem:
    TOPSYSTEM { gIsTopSystem = 1; }
    system_names { ibnlMakeSystem(gSysNames); }
    NL sub_inst_attributes
    insts
  ;

system:
    SYSTEM { gIsTopSystem = 0; }
    system_names { ibnlMakeSystem(gSysNames); } NL insts
  ;

system_names:
    system_name
  | system_names ',' system_name
  ;

system_name:
    NAME { gSysNames.push_back($1); }
  ;

insts:
  | insts node
  | insts subsystem
  ;

node:
   node_header NL node_connections
  ;

node_header:
    NODE NODETYPE INT NAME NAME { ibnlMakeNode($2,$3,$4,$5); }
  ;

node_connections:
  | node_connections node_connection NL
  ;

node_connection:
    node_to_node_link
  | node_to_port_link
  ;

node_to_node_link:
    INT '-' WIDTH '-' SPEED '-' '>' NAME INT {
      ibnlMakeNodeToNodeConn($1, $3, $5, $8, $9);
    }
  | INT '-' WIDTH '-' '>' NAME INT {
      ibnlMakeNodeToNodeConn($1, $3, "2.5", $6, $7);
    }
  | INT '-' SPEED '-' '>' NAME INT {
      ibnlMakeNodeToNodeConn($1, "4x", $3, $6, $7);
    }
  | INT '-' '>' NAME INT {
      ibnlMakeNodeToNodeConn($1, "4x", "2.5", $4, $5);
    }
  ;

node_to_port_link:
    INT '-' WIDTH '-' SPEED '-' '>' NAME {
      ibnlMakeNodeToPortConn($1, $3, $5, $8);
    }
  | INT '-' WIDTH '-' '>' NAME {
      ibnlMakeNodeToPortConn($1, $3, "2.5", $6);
    }
  | INT '-' SPEED '-' '>' NAME {
      ibnlMakeNodeToPortConn($1, "4x", $3, $6);
    }
  | INT '-' '>' NAME {
      ibnlMakeNodeToPortConn($1, "4x", "2.5", $4);
    }
  ;

subsystem:
    subsystem_header NL subsystem_connections
  | subsystem_header NL CFG insts_modifications NL subsystem_connections
  ;

subsystem_header:
    SUBSYSTEM NAME NAME { ibnlMakeSubsystem($2,$3); }
  ;

insts_modifications:
  | insts_modifications modification
  ;

modification:
    NAME '=' NAME { ibnlRecordModification($1,$3); }
  ;

subsystem_connections:
  | subsystem_connections subsystem_connection NL
  ;

subsystem_connection:
    subsystem_to_subsystem_link
  | subsystem_to_port_link
  ;

subsystem_to_subsystem_link:
    NAME '-' WIDTH '-' SPEED '-' '>' NAME NAME {
      ibnlMakeSubsystemToSubsystemConn($1, $3, $5, $8, $9);
    }
  | NAME '-' WIDTH '-' '>' NAME NAME {
      ibnlMakeSubsystemToSubsystemConn($1, $3, "2.5", $6, $7);
    }
  | NAME '-' SPEED '-' '>' NAME NAME {
      ibnlMakeSubsystemToSubsystemConn($1, "4x", $3, $6, $7);
    }
  | NAME '-' '>' NAME NAME {
      ibnlMakeSubsystemToSubsystemConn($1, "4x", "2.5", $4, $5);
    }
  ;

subsystem_to_port_link:
    NAME '-' WIDTH '-' SPEED '-' '>' NAME {
      ibnlMakeSubsystemToPortConn($1, $3, $5, $8);
    }
  | NAME '-' WIDTH '-' '>' NAME {
      ibnlMakeSubsystemToPortConn($1, $3, "2.5", $6);
    }
  | NAME '-' SPEED '-' '>' NAME {
      ibnlMakeSubsystemToPortConn($1, "4x", $3, $6);
    }
  | NAME '-' '>' NAME {
      ibnlMakeSubsystemToPortConn($1, "4x", "2.5", $4);
    }
  ;

%%

int yyerror(char *msg)
{
  printf("-E-ibnlParse:%s at line:%ld\n", msg, lineNum);
  ibnlErr = 1;
  return 1;
}

/* parse apollo route dump file */
int ibnlParseSysDefs (IBSystemsCollection *p_sysColl, const char *fileName) {
  extern FILE * yyin;

  gp_sysColl = p_sysColl;
  gp_fileName = fileName;

  /* open the file */
  yyin = fopen(fileName,"r");
  if (!yyin) {
	 printf("-E- Fail to Open File:%s\n", fileName);
	 return(1);
  }
  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
     printf("-I- Parsing:%s\n", fileName);

  ibnlErr = 0;
  lineNum = 1;
  /* parse it */
  yyparse();

  fclose(yyin);
  return(ibnlErr);
}
