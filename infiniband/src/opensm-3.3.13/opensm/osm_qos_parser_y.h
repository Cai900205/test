/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     TK_NUMBER = 258,
     TK_DASH = 259,
     TK_DOTDOT = 260,
     TK_COMMA = 261,
     TK_ASTERISK = 262,
     TK_TEXT = 263,
     TK_QOS_ULPS_START = 264,
     TK_QOS_ULPS_END = 265,
     TK_PORT_GROUPS_START = 266,
     TK_PORT_GROUPS_END = 267,
     TK_PORT_GROUP_START = 268,
     TK_PORT_GROUP_END = 269,
     TK_QOS_SETUP_START = 270,
     TK_QOS_SETUP_END = 271,
     TK_VLARB_TABLES_START = 272,
     TK_VLARB_TABLES_END = 273,
     TK_VLARB_SCOPE_START = 274,
     TK_VLARB_SCOPE_END = 275,
     TK_SL2VL_TABLES_START = 276,
     TK_SL2VL_TABLES_END = 277,
     TK_SL2VL_SCOPE_START = 278,
     TK_SL2VL_SCOPE_END = 279,
     TK_QOS_LEVELS_START = 280,
     TK_QOS_LEVELS_END = 281,
     TK_QOS_LEVEL_START = 282,
     TK_QOS_LEVEL_END = 283,
     TK_QOS_MATCH_RULES_START = 284,
     TK_QOS_MATCH_RULES_END = 285,
     TK_QOS_MATCH_RULE_START = 286,
     TK_QOS_MATCH_RULE_END = 287,
     TK_NAME = 288,
     TK_USE = 289,
     TK_PORT_GUID = 290,
     TK_PORT_NAME = 291,
     TK_PARTITION = 292,
     TK_NODE_TYPE = 293,
     TK_GROUP = 294,
     TK_ACROSS = 295,
     TK_VLARB_HIGH = 296,
     TK_VLARB_LOW = 297,
     TK_VLARB_HIGH_LIMIT = 298,
     TK_TO = 299,
     TK_FROM = 300,
     TK_ACROSS_TO = 301,
     TK_ACROSS_FROM = 302,
     TK_SL2VL_TABLE = 303,
     TK_SL = 304,
     TK_MTU_LIMIT = 305,
     TK_RATE_LIMIT = 306,
     TK_PACKET_LIFE = 307,
     TK_PATH_BITS = 308,
     TK_QOS_CLASS = 309,
     TK_SOURCE = 310,
     TK_DESTINATION = 311,
     TK_SERVICE_ID = 312,
     TK_QOS_LEVEL_NAME = 313,
     TK_PKEY = 314,
     TK_NODE_TYPE_ROUTER = 315,
     TK_NODE_TYPE_CA = 316,
     TK_NODE_TYPE_SWITCH = 317,
     TK_NODE_TYPE_SELF = 318,
     TK_NODE_TYPE_ALL = 319,
     TK_ULP_DEFAULT = 320,
     TK_ULP_ANY_SERVICE_ID = 321,
     TK_ULP_ANY_PKEY = 322,
     TK_ULP_ANY_TARGET_PORT_GUID = 323,
     TK_ULP_ANY_SOURCE_PORT_GUID = 324,
     TK_ULP_ANY_SOURCE_TARGET_PORT_GUID = 325,
     TK_ULP_SDP_DEFAULT = 326,
     TK_ULP_SDP_PORT = 327,
     TK_ULP_RDS_DEFAULT = 328,
     TK_ULP_RDS_PORT = 329,
     TK_ULP_ISER_DEFAULT = 330,
     TK_ULP_ISER_PORT = 331,
     TK_ULP_SRP_GUID = 332,
     TK_ULP_IPOIB_DEFAULT = 333,
     TK_ULP_IPOIB_PKEY = 334
   };
#endif
/* Tokens.  */
#define TK_NUMBER 258
#define TK_DASH 259
#define TK_DOTDOT 260
#define TK_COMMA 261
#define TK_ASTERISK 262
#define TK_TEXT 263
#define TK_QOS_ULPS_START 264
#define TK_QOS_ULPS_END 265
#define TK_PORT_GROUPS_START 266
#define TK_PORT_GROUPS_END 267
#define TK_PORT_GROUP_START 268
#define TK_PORT_GROUP_END 269
#define TK_QOS_SETUP_START 270
#define TK_QOS_SETUP_END 271
#define TK_VLARB_TABLES_START 272
#define TK_VLARB_TABLES_END 273
#define TK_VLARB_SCOPE_START 274
#define TK_VLARB_SCOPE_END 275
#define TK_SL2VL_TABLES_START 276
#define TK_SL2VL_TABLES_END 277
#define TK_SL2VL_SCOPE_START 278
#define TK_SL2VL_SCOPE_END 279
#define TK_QOS_LEVELS_START 280
#define TK_QOS_LEVELS_END 281
#define TK_QOS_LEVEL_START 282
#define TK_QOS_LEVEL_END 283
#define TK_QOS_MATCH_RULES_START 284
#define TK_QOS_MATCH_RULES_END 285
#define TK_QOS_MATCH_RULE_START 286
#define TK_QOS_MATCH_RULE_END 287
#define TK_NAME 288
#define TK_USE 289
#define TK_PORT_GUID 290
#define TK_PORT_NAME 291
#define TK_PARTITION 292
#define TK_NODE_TYPE 293
#define TK_GROUP 294
#define TK_ACROSS 295
#define TK_VLARB_HIGH 296
#define TK_VLARB_LOW 297
#define TK_VLARB_HIGH_LIMIT 298
#define TK_TO 299
#define TK_FROM 300
#define TK_ACROSS_TO 301
#define TK_ACROSS_FROM 302
#define TK_SL2VL_TABLE 303
#define TK_SL 304
#define TK_MTU_LIMIT 305
#define TK_RATE_LIMIT 306
#define TK_PACKET_LIFE 307
#define TK_PATH_BITS 308
#define TK_QOS_CLASS 309
#define TK_SOURCE 310
#define TK_DESTINATION 311
#define TK_SERVICE_ID 312
#define TK_QOS_LEVEL_NAME 313
#define TK_PKEY 314
#define TK_NODE_TYPE_ROUTER 315
#define TK_NODE_TYPE_CA 316
#define TK_NODE_TYPE_SWITCH 317
#define TK_NODE_TYPE_SELF 318
#define TK_NODE_TYPE_ALL 319
#define TK_ULP_DEFAULT 320
#define TK_ULP_ANY_SERVICE_ID 321
#define TK_ULP_ANY_PKEY 322
#define TK_ULP_ANY_TARGET_PORT_GUID 323
#define TK_ULP_ANY_SOURCE_PORT_GUID 324
#define TK_ULP_ANY_SOURCE_TARGET_PORT_GUID 325
#define TK_ULP_SDP_DEFAULT 326
#define TK_ULP_SDP_PORT 327
#define TK_ULP_RDS_DEFAULT 328
#define TK_ULP_RDS_PORT 329
#define TK_ULP_ISER_DEFAULT 330
#define TK_ULP_ISER_PORT 331
#define TK_ULP_SRP_GUID 332
#define TK_ULP_IPOIB_DEFAULT 333
#define TK_ULP_IPOIB_PKEY 334




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


