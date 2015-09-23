/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 1 "osm_qos_parser_y.y"

/*
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2008 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2008 HNR Consulting. All rights reserved.
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
 * Abstract:
 *    Grammar of OSM QoS parser.
 *
 * Environment:
 *    Linux User Mode
 *
 * Author:
 *    Yevgeny Kliteynik, Mellanox
 */

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_qos_policy.h>

#define OSM_QOS_POLICY_MAX_LINE_LEN         1024*10
#define OSM_QOS_POLICY_SL2VL_TABLE_LEN      IB_MAX_NUM_VLS
#define OSM_QOS_POLICY_MAX_VL_NUM           IB_MAX_NUM_VLS

typedef struct tmp_parser_struct_t_ {
    char       str[OSM_QOS_POLICY_MAX_LINE_LEN];
    uint64_t   num_pair[2];
    cl_list_t  str_list;
    cl_list_t  num_list;
    cl_list_t  num_pair_list;
} tmp_parser_struct_t;

static void __parser_tmp_struct_init();
static void __parser_tmp_struct_reset();
static void __parser_tmp_struct_destroy();

static char * __parser_strip_white(char * str);

static void __parser_str2uint64(uint64_t * p_val, char * str);

static void __parser_port_group_start();
static int __parser_port_group_end();

static void __parser_sl2vl_scope_start();
static int __parser_sl2vl_scope_end();

static void __parser_vlarb_scope_start();
static int __parser_vlarb_scope_end();

static void __parser_qos_level_start();
static int __parser_qos_level_end();

static void __parser_match_rule_start();
static int __parser_match_rule_end();

static void __parser_ulp_match_rule_start();
static int __parser_ulp_match_rule_end();

static void __pkey_rangelist2rangearr(
    cl_list_t    * p_list,
    uint64_t  ** * p_arr,
    unsigned     * p_arr_len);

static void __rangelist2rangearr(
    cl_list_t    * p_list,
    uint64_t  ** * p_arr,
    unsigned     * p_arr_len);

static void __merge_rangearr(
    uint64_t  **   range_arr_1,
    unsigned       range_len_1,
    uint64_t  **   range_arr_2,
    unsigned       range_len_2,
    uint64_t  ** * p_arr,
    unsigned     * p_arr_len );

static void __parser_add_port_to_port_map(
    cl_qmap_t   * p_map,
    osm_physp_t * p_physp);

static void __parser_add_guid_range_to_port_map(
    cl_qmap_t  * p_map,
    uint64_t  ** range_arr,
    unsigned     range_len);

static void __parser_add_pkey_range_to_port_map(
    cl_qmap_t  * p_map,
    uint64_t  ** range_arr,
    unsigned     range_len);

static void __parser_add_partition_list_to_port_map(
    cl_qmap_t  * p_map,
    cl_list_t  * p_list);

static void __parser_add_map_to_port_map(
    cl_qmap_t * p_dmap,
    cl_map_t  * p_smap);

static int __validate_pkeys(
    uint64_t ** range_arr,
    unsigned    range_len,
    boolean_t   is_ipoib);

static void __setup_simple_qos_levels();
static void __clear_simple_qos_levels();
static void __setup_ulp_match_rules();
static void __process_ulp_match_rules();
static void yyerror(const char *format, ...);

extern char * yytext;
extern int yylex (void);
extern FILE * yyin;
extern int errno;
extern void yyrestart(FILE *input_file);
int yyparse();

#define RESET_BUFFER  __parser_tmp_struct_reset()

tmp_parser_struct_t tmp_parser_struct;

int column_num;
int line_num;

osm_qos_policy_t       * p_qos_policy = NULL;
osm_qos_port_group_t   * p_current_port_group = NULL;
osm_qos_sl2vl_scope_t  * p_current_sl2vl_scope = NULL;
osm_qos_vlarb_scope_t  * p_current_vlarb_scope = NULL;
osm_qos_level_t        * p_current_qos_level = NULL;
osm_qos_match_rule_t   * p_current_qos_match_rule = NULL;
osm_log_t              * p_qos_parser_osm_log;

/* 16 Simple QoS Levels - one for each SL */
static osm_qos_level_t osm_qos_policy_simple_qos_levels[16];

/* Default Simple QoS Level */
osm_qos_level_t __default_simple_qos_level;

/*
 * List of match rules that will be generated by the
 * qos-ulp section. These rules are concatenated to
 * the end of the usual matching rules list at the
 * end of parsing.
 */
static cl_list_t __ulp_match_rules;

/***************************************************/



/* Line 268 of yacc.c  */
#line 259 "osm_qos_parser_y.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


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


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 459 "osm_qos_parser_y.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   275

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  80
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  169
/* YYNRULES -- Number of rules.  */
#define YYNRULES  244
/* YYNRULES -- Number of states.  */
#define YYNSTATES  340

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   334

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     6,     9,    11,    13,    15,    17,
      19,    23,    25,    28,    32,    34,    37,    41,    43,    45,
      46,    49,    51,    53,    55,    57,    59,    61,    63,    67,
      68,    71,    74,    78,    79,    82,    86,    88,    90,    91,
      94,    96,    98,   100,   102,   104,   108,   109,   112,   116,
     118,   120,   121,   124,   126,   128,   130,   132,   134,   136,
     138,   142,   143,   146,   150,   152,   154,   155,   158,   160,
     162,   164,   166,   168,   170,   172,   174,   178,   179,   182,
     186,   188,   190,   191,   194,   196,   198,   200,   202,   204,
     206,   208,   211,   212,   218,   219,   225,   226,   232,   233,
     239,   240,   246,   247,   251,   252,   258,   259,   263,   264,
     270,   271,   275,   276,   282,   283,   289,   290,   294,   295,
     301,   303,   305,   307,   309,   311,   313,   315,   317,   319,
     321,   323,   325,   327,   329,   331,   334,   336,   339,   341,
     344,   346,   349,   351,   354,   356,   359,   361,   364,   366,
     368,   372,   374,   376,   378,   380,   382,   384,   386,   388,
     390,   392,   395,   397,   400,   402,   405,   407,   410,   412,
     415,   417,   420,   422,   425,   427,   430,   432,   435,   437,
     440,   442,   445,   447,   449,   451,   453,   455,   457,   459,
     461,   463,   466,   468,   471,   473,   476,   478,   481,   483,
     486,   488,   491,   493,   496,   498,   501,   503,   506,   508,
     511,   513,   516,   518,   521,   523,   526,   528,   531,   533,
     536,   538,   541,   543,   545,   547,   550,   552,   554,   558,
     560,   562,   566,   568,   572,   578,   580,   582,   584,   586,
     590,   596,   600,   602,   604
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      81,     0,    -1,    82,    -1,    -1,    82,    83,    -1,    84,
      -1,    86,    -1,    93,    -1,   109,    -1,   116,    -1,     9,
      85,    10,    -1,   123,    -1,    85,   123,    -1,    11,    87,
      12,    -1,    88,    -1,    87,    88,    -1,    89,    91,    90,
      -1,    13,    -1,    14,    -1,    -1,    91,    92,    -1,   153,
      -1,   155,    -1,   159,    -1,   157,    -1,   161,    -1,   163,
      -1,   165,    -1,    15,    94,    16,    -1,    -1,    94,    95,
      -1,    94,   102,    -1,    17,    96,    18,    -1,    -1,    96,
      97,    -1,    98,   100,    99,    -1,    19,    -1,    20,    -1,
      -1,   100,   101,    -1,   174,    -1,   176,    -1,   180,    -1,
     182,    -1,   178,    -1,    21,   103,    22,    -1,    -1,   103,
     104,    -1,   105,   107,   106,    -1,    23,    -1,    24,    -1,
      -1,   107,   108,    -1,   184,    -1,   186,    -1,   188,    -1,
     190,    -1,   192,    -1,   194,    -1,   202,    -1,    25,   110,
      26,    -1,    -1,   110,   111,    -1,   112,   114,   113,    -1,
      27,    -1,    28,    -1,    -1,   114,   115,    -1,   204,    -1,
     206,    -1,   208,    -1,   210,    -1,   212,    -1,   214,    -1,
     216,    -1,   218,    -1,    29,   117,    30,    -1,    -1,   117,
     118,    -1,   119,   121,   120,    -1,    31,    -1,    32,    -1,
      -1,   121,   122,    -1,   220,    -1,   222,    -1,   228,    -1,
     224,    -1,   226,    -1,   230,    -1,   232,    -1,    65,   238,
      -1,    -1,   138,   244,     5,   124,   152,    -1,    -1,   139,
     244,     5,   125,   152,    -1,    -1,   140,   244,     5,   126,
     152,    -1,    -1,   141,   244,     5,   127,   152,    -1,    -1,
     142,   244,     5,   128,   152,    -1,    -1,   143,   129,   152,
      -1,    -1,   144,   244,     5,   130,   152,    -1,    -1,   145,
     131,   152,    -1,    -1,   146,   244,     5,   132,   152,    -1,
      -1,   147,   133,   152,    -1,    -1,   148,   244,     5,   134,
     152,    -1,    -1,   149,   244,     5,   135,   152,    -1,    -1,
     150,   136,   152,    -1,    -1,   151,   244,     5,   137,   152,
      -1,    66,    -1,    67,    -1,    68,    -1,    69,    -1,    70,
      -1,    71,    -1,    72,    -1,    73,    -1,    74,    -1,    75,
      -1,    76,    -1,    77,    -1,    78,    -1,    79,    -1,   238,
      -1,   154,   234,    -1,    33,    -1,   156,   234,    -1,    34,
      -1,   158,   237,    -1,    36,    -1,   160,   244,    -1,    35,
      -1,   162,   244,    -1,    59,    -1,   164,   237,    -1,    37,
      -1,   166,   167,    -1,    38,    -1,   168,    -1,   167,     6,
     168,    -1,   169,    -1,   170,    -1,   171,    -1,   172,    -1,
     173,    -1,    61,    -1,    62,    -1,    60,    -1,    64,    -1,
      63,    -1,   175,   237,    -1,    39,    -1,   177,   237,    -1,
      40,    -1,   179,   238,    -1,    43,    -1,   181,   241,    -1,
      41,    -1,   183,   241,    -1,    42,    -1,   185,   237,    -1,
      39,    -1,   187,   237,    -1,    40,    -1,   189,   237,    -1,
      47,    -1,   191,   237,    -1,    46,    -1,   193,   196,    -1,
      45,    -1,   195,   198,    -1,    44,    -1,   197,    -1,   200,
      -1,     7,    -1,   199,    -1,   201,    -1,     7,    -1,   244,
      -1,   244,    -1,   203,   239,    -1,    48,    -1,   205,   234,
      -1,    33,    -1,   207,   234,    -1,    34,    -1,   209,   238,
      -1,    49,    -1,   211,   238,    -1,    50,    -1,   213,   238,
      -1,    51,    -1,   215,   238,    -1,    52,    -1,   217,   244,
      -1,    53,    -1,   219,   244,    -1,    59,    -1,   221,   234,
      -1,    34,    -1,   223,   244,    -1,    54,    -1,   225,   237,
      -1,    55,    -1,   227,   237,    -1,    56,    -1,   229,   234,
      -1,    58,    -1,   231,   244,    -1,    57,    -1,   233,   244,
      -1,    59,    -1,   235,    -1,   236,    -1,   235,   236,    -1,
       8,    -1,   234,    -1,   237,     6,   234,    -1,   240,    -1,
     240,    -1,   239,     6,   240,    -1,     3,    -1,   242,     5,
     243,    -1,   241,     6,   242,     5,   243,    -1,     3,    -1,
       3,    -1,   245,    -1,   246,    -1,   247,     4,   248,    -1,
     245,     6,   247,     4,   248,    -1,   245,     6,   246,    -1,
       3,    -1,     3,    -1,     3,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   279,   279,   282,   283,   286,   287,   288,   289,   290,
     316,   319,   320,   349,   352,   353,   356,   359,   364,   370,
     371,   374,   375,   376,   377,   378,   379,   380,   407,   410,
     411,   412,   417,   420,   421,   424,   427,   432,   438,   439,
     454,   455,   456,   457,   458,   463,   466,   467,   470,   473,
     478,   484,   485,   504,   505,   506,   507,   508,   509,   510,
     534,   537,   538,   541,   544,   549,   555,   556,   559,   560,
     561,   562,   563,   564,   565,   566,   588,   591,   592,   595,
     598,   603,   609,   610,   613,   614,   615,   616,   617,   618,
     619,   643,   661,   661,   682,   682,   703,   703,   738,   738,
     773,   773,   811,   811,   824,   824,   858,   858,   871,   871,
     905,   905,   918,   918,   952,   952,   988,   988,  1008,  1008,
    1041,  1044,  1047,  1050,  1053,  1056,  1059,  1062,  1065,  1068,
    1071,  1074,  1077,  1080,  1084,  1123,  1146,  1151,  1174,  1179,
    1245,  1250,  1270,  1275,  1295,  1300,  1308,  1313,  1318,  1323,
    1324,  1327,  1328,  1329,  1330,  1331,  1334,  1340,  1346,  1352,
    1360,  1382,  1399,  1404,  1421,  1426,  1444,  1449,  1466,  1471,
    1488,  1504,  1521,  1526,  1545,  1550,  1567,  1572,  1590,  1595,
    1600,  1605,  1610,  1615,  1616,  1619,  1626,  1627,  1630,  1637,
    1669,  1702,  1745,  1762,  1785,  1790,  1813,  1818,  1838,  1843,
    1863,  1869,  1889,  1895,  1915,  1921,  1956,  1961,  1994,  2011,
    2034,  2039,  2073,  2078,  2095,  2100,  2117,  2122,  2145,  2150,
    2183,  2188,  2221,  2232,  2239,  2240,  2243,  2250,  2251,  2256,
    2259,  2260,  2263,  2271,  2277,  2285,  2291,  2297,  2300,  2306,
    2318,  2330,  2338,  2345,  2351
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "TK_NUMBER", "TK_DASH", "TK_DOTDOT",
  "TK_COMMA", "TK_ASTERISK", "TK_TEXT", "TK_QOS_ULPS_START",
  "TK_QOS_ULPS_END", "TK_PORT_GROUPS_START", "TK_PORT_GROUPS_END",
  "TK_PORT_GROUP_START", "TK_PORT_GROUP_END", "TK_QOS_SETUP_START",
  "TK_QOS_SETUP_END", "TK_VLARB_TABLES_START", "TK_VLARB_TABLES_END",
  "TK_VLARB_SCOPE_START", "TK_VLARB_SCOPE_END", "TK_SL2VL_TABLES_START",
  "TK_SL2VL_TABLES_END", "TK_SL2VL_SCOPE_START", "TK_SL2VL_SCOPE_END",
  "TK_QOS_LEVELS_START", "TK_QOS_LEVELS_END", "TK_QOS_LEVEL_START",
  "TK_QOS_LEVEL_END", "TK_QOS_MATCH_RULES_START", "TK_QOS_MATCH_RULES_END",
  "TK_QOS_MATCH_RULE_START", "TK_QOS_MATCH_RULE_END", "TK_NAME", "TK_USE",
  "TK_PORT_GUID", "TK_PORT_NAME", "TK_PARTITION", "TK_NODE_TYPE",
  "TK_GROUP", "TK_ACROSS", "TK_VLARB_HIGH", "TK_VLARB_LOW",
  "TK_VLARB_HIGH_LIMIT", "TK_TO", "TK_FROM", "TK_ACROSS_TO",
  "TK_ACROSS_FROM", "TK_SL2VL_TABLE", "TK_SL", "TK_MTU_LIMIT",
  "TK_RATE_LIMIT", "TK_PACKET_LIFE", "TK_PATH_BITS", "TK_QOS_CLASS",
  "TK_SOURCE", "TK_DESTINATION", "TK_SERVICE_ID", "TK_QOS_LEVEL_NAME",
  "TK_PKEY", "TK_NODE_TYPE_ROUTER", "TK_NODE_TYPE_CA",
  "TK_NODE_TYPE_SWITCH", "TK_NODE_TYPE_SELF", "TK_NODE_TYPE_ALL",
  "TK_ULP_DEFAULT", "TK_ULP_ANY_SERVICE_ID", "TK_ULP_ANY_PKEY",
  "TK_ULP_ANY_TARGET_PORT_GUID", "TK_ULP_ANY_SOURCE_PORT_GUID",
  "TK_ULP_ANY_SOURCE_TARGET_PORT_GUID", "TK_ULP_SDP_DEFAULT",
  "TK_ULP_SDP_PORT", "TK_ULP_RDS_DEFAULT", "TK_ULP_RDS_PORT",
  "TK_ULP_ISER_DEFAULT", "TK_ULP_ISER_PORT", "TK_ULP_SRP_GUID",
  "TK_ULP_IPOIB_DEFAULT", "TK_ULP_IPOIB_PKEY", "$accept", "head",
  "qos_policy_entries", "qos_policy_entry", "qos_ulps_section", "qos_ulps",
  "port_groups_section", "port_groups", "port_group", "port_group_start",
  "port_group_end", "port_group_entries", "port_group_entry",
  "qos_setup_section", "qos_setup_items", "vlarb_tables",
  "vlarb_scope_items", "vlarb_scope", "vlarb_scope_start",
  "vlarb_scope_end", "vlarb_scope_entries", "vlarb_scope_entry",
  "sl2vl_tables", "sl2vl_scope_items", "sl2vl_scope", "sl2vl_scope_start",
  "sl2vl_scope_end", "sl2vl_scope_entries", "sl2vl_scope_entry",
  "qos_levels_section", "qos_levels", "qos_level", "qos_level_start",
  "qos_level_end", "qos_level_entries", "qos_level_entry",
  "qos_match_rules_section", "qos_match_rules", "qos_match_rule",
  "qos_match_rule_start", "qos_match_rule_end", "qos_match_rule_entries",
  "qos_match_rule_entry", "qos_ulp", "$@1", "$@2", "$@3", "$@4", "$@5",
  "$@6", "$@7", "$@8", "$@9", "$@10", "$@11", "$@12", "$@13", "$@14",
  "qos_ulp_type_any_service", "qos_ulp_type_any_pkey",
  "qos_ulp_type_any_target_port_guid", "qos_ulp_type_any_source_port_guid",
  "qos_ulp_type_any_source_target_port_guid", "qos_ulp_type_sdp_default",
  "qos_ulp_type_sdp_port", "qos_ulp_type_rds_default",
  "qos_ulp_type_rds_port", "qos_ulp_type_iser_default",
  "qos_ulp_type_iser_port", "qos_ulp_type_srp_guid",
  "qos_ulp_type_ipoib_default", "qos_ulp_type_ipoib_pkey", "qos_ulp_sl",
  "port_group_name", "port_group_name_start", "port_group_use",
  "port_group_use_start", "port_group_port_name",
  "port_group_port_name_start", "port_group_port_guid",
  "port_group_port_guid_start", "port_group_pkey", "port_group_pkey_start",
  "port_group_partition", "port_group_partition_start",
  "port_group_node_type", "port_group_node_type_start",
  "port_group_node_type_list", "node_type_item", "node_type_ca",
  "node_type_switch", "node_type_router", "node_type_all",
  "node_type_self", "vlarb_scope_group", "vlarb_scope_group_start",
  "vlarb_scope_across", "vlarb_scope_across_start",
  "vlarb_scope_vlarb_high_limit", "vlarb_scope_vlarb_high_limit_start",
  "vlarb_scope_vlarb_high", "vlarb_scope_vlarb_high_start",
  "vlarb_scope_vlarb_low", "vlarb_scope_vlarb_low_start",
  "sl2vl_scope_group", "sl2vl_scope_group_start", "sl2vl_scope_across",
  "sl2vl_scope_across_start", "sl2vl_scope_across_from",
  "sl2vl_scope_across_from_start", "sl2vl_scope_across_to",
  "sl2vl_scope_across_to_start", "sl2vl_scope_from",
  "sl2vl_scope_from_start", "sl2vl_scope_to", "sl2vl_scope_to_start",
  "sl2vl_scope_from_list_or_asterisk", "sl2vl_scope_from_asterisk",
  "sl2vl_scope_to_list_or_asterisk", "sl2vl_scope_to_asterisk",
  "sl2vl_scope_from_list_of_ranges", "sl2vl_scope_to_list_of_ranges",
  "sl2vl_scope_sl2vl_table", "sl2vl_scope_sl2vl_table_start",
  "qos_level_name", "qos_level_name_start", "qos_level_use",
  "qos_level_use_start", "qos_level_sl", "qos_level_sl_start",
  "qos_level_mtu_limit", "qos_level_mtu_limit_start",
  "qos_level_rate_limit", "qos_level_rate_limit_start",
  "qos_level_packet_life", "qos_level_packet_life_start",
  "qos_level_path_bits", "qos_level_path_bits_start", "qos_level_pkey",
  "qos_level_pkey_start", "qos_match_rule_use", "qos_match_rule_use_start",
  "qos_match_rule_qos_class", "qos_match_rule_qos_class_start",
  "qos_match_rule_source", "qos_match_rule_source_start",
  "qos_match_rule_destination", "qos_match_rule_destination_start",
  "qos_match_rule_qos_level_name", "qos_match_rule_qos_level_name_start",
  "qos_match_rule_service_id", "qos_match_rule_service_id_start",
  "qos_match_rule_pkey", "qos_match_rule_pkey_start", "single_string",
  "single_string_elems", "single_string_element", "string_list",
  "single_number", "num_list", "number", "num_list_with_dotdot",
  "number_from_pair_1", "number_from_pair_2", "list_of_ranges",
  "num_list_with_dash", "single_number_from_range", "number_from_range_1",
  "number_from_range_2", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    80,    81,    82,    82,    83,    83,    83,    83,    83,
      84,    85,    85,    86,    87,    87,    88,    89,    90,    91,
      91,    92,    92,    92,    92,    92,    92,    92,    93,    94,
      94,    94,    95,    96,    96,    97,    98,    99,   100,   100,
     101,   101,   101,   101,   101,   102,   103,   103,   104,   105,
     106,   107,   107,   108,   108,   108,   108,   108,   108,   108,
     109,   110,   110,   111,   112,   113,   114,   114,   115,   115,
     115,   115,   115,   115,   115,   115,   116,   117,   117,   118,
     119,   120,   121,   121,   122,   122,   122,   122,   122,   122,
     122,   123,   124,   123,   125,   123,   126,   123,   127,   123,
     128,   123,   129,   123,   130,   123,   131,   123,   132,   123,
     133,   123,   134,   123,   135,   123,   136,   123,   137,   123,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     167,   168,   168,   168,   168,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   196,   196,   197,   198,   198,   199,   200,
     201,   202,   203,   204,   205,   206,   207,   208,   209,   210,
     211,   212,   213,   214,   215,   216,   217,   218,   219,   220,
     221,   222,   223,   224,   225,   226,   227,   228,   229,   230,
     231,   232,   233,   234,   235,   235,   236,   237,   237,   238,
     239,   239,   240,   241,   241,   242,   243,   244,   245,   245,
     245,   245,   246,   247,   248
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       3,     1,     2,     3,     1,     2,     3,     1,     1,     0,
       2,     1,     1,     1,     1,     1,     1,     1,     3,     0,
       2,     2,     3,     0,     2,     3,     1,     1,     0,     2,
       1,     1,     1,     1,     1,     3,     0,     2,     3,     1,
       1,     0,     2,     1,     1,     1,     1,     1,     1,     1,
       3,     0,     2,     3,     1,     1,     0,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     0,     2,     3,
       1,     1,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     2,     0,     5,     0,     5,     0,     5,     0,     5,
       0,     5,     0,     3,     0,     5,     0,     3,     0,     5,
       0,     3,     0,     5,     0,     5,     0,     3,     0,     5,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     1,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     1,     2,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     1,     2,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     2,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     1,     1,     2,     1,     1,     3,     1,
       1,     3,     1,     3,     5,     1,     1,     1,     1,     3,
       5,     3,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     2,     1,     0,     0,    29,    61,    77,     4,
       5,     6,     7,     8,     9,     0,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
       0,    11,     0,     0,     0,     0,     0,   102,     0,   106,
       0,   110,     0,     0,   116,     0,    17,     0,    14,    19,
       0,     0,     0,   232,    91,   229,    10,    12,   242,     0,
     237,   238,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    13,    15,     0,    28,
      33,    46,    30,    31,    60,    64,    62,    66,    76,    80,
      78,    82,    92,     0,     0,    94,    96,    98,   100,   103,
     134,   104,   107,   108,   111,   112,   114,   117,   118,    18,
     136,   138,   142,   140,   146,   148,   144,    16,    20,    21,
       0,    22,     0,    24,     0,    23,     0,    25,     0,    26,
       0,    27,     0,     0,     0,     0,     0,     0,   241,     0,
     244,   239,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   226,   135,   223,   224,   137,   227,   139,   141,   143,
     145,   158,   156,   157,   160,   159,   147,   149,   151,   152,
     153,   154,   155,    32,    36,    34,    38,    45,    49,    47,
      51,    65,   194,   196,   198,   200,   202,   204,   206,   208,
      63,    67,    68,     0,    69,     0,    70,     0,    71,     0,
      72,     0,    73,     0,    74,     0,    75,     0,    81,   210,
     212,   214,   216,   220,   218,   222,    79,    83,    84,     0,
      85,     0,    87,     0,    88,     0,    86,     0,    89,     0,
      90,     0,    93,     0,    95,    97,    99,   101,   105,   109,
     113,   115,   119,   225,     0,     0,     0,     0,   193,   195,
     197,   199,   201,   203,   205,   207,   209,   211,   213,   215,
     217,   219,   221,   240,   228,   150,    37,   162,   164,   168,
     170,   166,    35,    39,    40,     0,    41,     0,    44,     0,
      42,     0,    43,     0,    50,   172,   174,   182,   180,   178,
     176,   192,    48,    52,    53,     0,    54,     0,    55,     0,
      56,     0,    57,     0,    58,     0,    59,     0,   161,   163,
     165,   235,   167,     0,   169,   171,   173,   175,   177,   185,
     179,   183,   184,   189,   188,   181,   186,   187,   190,   191,
     230,     0,     0,     0,     0,   236,   233,   231,     0,   234
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     9,    10,    30,    11,    47,    48,    49,
     117,    78,   118,    12,    50,    82,   133,   175,   176,   272,
     246,   273,    83,   134,   179,   180,   292,   247,   293,    13,
      51,    86,    87,   190,   135,   191,    14,    52,    90,    91,
     216,   136,   217,    31,   137,   142,   143,   144,   145,    67,
     146,    69,   147,    71,   148,   149,    74,   150,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    99,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   166,   167,   168,
     169,   170,   171,   172,   274,   275,   276,   277,   278,   279,
     280,   281,   282,   283,   294,   295,   296,   297,   298,   299,
     300,   301,   302,   303,   304,   305,   320,   321,   325,   326,
     322,   327,   306,   307,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     218,   219,   220,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   156,   153,   154,   157,   100,   329,
      55,   312,   313,   336,    59,    60,    61,    62,   141
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -294
static const yytype_int16 yypact[] =
{
    -294,    18,    13,  -294,    87,     7,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,    20,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
      66,  -294,    26,    26,    26,    26,    26,  -294,    26,  -294,
      26,  -294,    26,    26,  -294,    26,  -294,    41,  -294,  -294,
       0,    29,    35,  -294,  -294,  -294,  -294,  -294,    27,    52,
      53,  -294,    69,    70,    72,    73,    75,    20,    79,    20,
      80,    20,    81,    89,    20,    91,  -294,  -294,    -1,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,    26,    71,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
      90,  -294,    90,  -294,    90,  -294,    26,  -294,    26,  -294,
      90,  -294,   116,    50,    49,    11,    -7,    20,  -294,    88,
    -294,  -294,    20,    20,    20,    20,    20,    20,    20,    20,
      20,  -294,  -294,    90,  -294,  -294,  -294,    94,  -294,  -294,
      94,  -294,  -294,  -294,  -294,  -294,    95,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,    90,  -294,    90,  -294,    20,  -294,    20,
    -294,    20,  -294,    20,  -294,    26,  -294,    26,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,    90,
    -294,    26,  -294,    90,  -294,    90,  -294,    90,  -294,    26,
    -294,    26,  -294,    71,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,    90,   116,    84,    43,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,    94,    94,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,    90,  -294,    90,  -294,    20,
    -294,    99,  -294,    99,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,    90,  -294,    90,  -294,    90,
    -294,    90,  -294,    12,  -294,    23,  -294,    20,    94,    94,
    -294,  -294,    97,   101,    97,    94,    94,    94,    94,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,   102,
    -294,    99,   104,    20,   105,  -294,  -294,  -294,   104,  -294
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,    64,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,    82,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,   -28,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -117,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -114,  -294,   -24,  -126,    -4,  -294,
    -293,  -137,  -184,  -190,   -33,  -294,    57,    74,   -65
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -244
static const yytype_int16 yytable[] =
{
      63,    64,    65,    66,   160,    68,   152,    70,   155,    72,
      73,    54,    75,   109,   330,    58,    79,    80,     3,   319,
      46,    81,     4,    53,     5,   208,    58,   209,     6,    58,
     324,  -243,   110,   111,   112,   113,   114,   115,     7,   181,
     337,   102,     8,   104,   182,   183,   107,   210,   211,   212,
     213,   214,   215,    76,    46,    84,    85,    92,   116,    93,
     184,   185,   186,   187,   188,    88,    89,   284,   173,   174,
     189,   177,   178,    94,   140,    95,    56,    96,    97,   248,
      98,   249,   285,   286,   101,   103,   105,   287,   288,   289,
     290,   291,   233,   158,   106,   159,   108,   258,   151,   259,
     244,   245,   311,   331,   266,   256,   332,   335,   333,   232,
     338,    77,    57,   260,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   267,   268,   269,   270,   271,   265,   243,
     264,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,   314,   334,   339,   308,
     138,   309,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,   139,   263,   315,
       0,   316,   254,   317,   255,   318,   161,   162,   163,   164,
     165,     0,     0,     0,     0,     0,     0,     0,   257,     0,
       0,     0,     0,   250,     0,   251,   261,   252,   262,   253,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     323,     0,   328,     0,     0,   310
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-294))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
      33,    34,    35,    36,   130,    38,   120,    40,   122,    42,
      43,    15,    45,    14,   307,     3,    16,    17,     0,     7,
      13,    21,     9,     3,    11,    32,     3,    34,    15,     3,
       7,     4,    33,    34,    35,    36,    37,    38,    25,    28,
     333,    69,    29,    71,    33,    34,    74,    54,    55,    56,
      57,    58,    59,    12,    13,    26,    27,     5,    59,     6,
      49,    50,    51,    52,    53,    30,    31,    24,    18,    19,
      59,    22,    23,     4,     3,     5,    10,     5,     5,   193,
       5,   195,    39,    40,     5,     5,     5,    44,    45,    46,
      47,    48,     4,   126,     5,   128,     5,   223,     8,   225,
       6,     6,     3,     6,    20,   219,     5,     3,     6,   137,
       5,    47,    30,   227,   142,   143,   144,   145,   146,   147,
     148,   149,   150,    39,    40,    41,    42,    43,   245,   153,
     244,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,   283,   331,   338,   275,
      93,   277,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    93,   233,   295,
      -1,   297,   205,   299,   207,   301,    60,    61,    62,    63,
      64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   221,    -1,
      -1,    -1,    -1,   197,    -1,   199,   229,   201,   231,   203,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     303,    -1,   305,    -1,    -1,   279
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    81,    82,     0,     9,    11,    15,    25,    29,    83,
      84,    86,    93,   109,   116,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      85,   123,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,    13,    87,    88,    89,
      94,   110,   117,     3,   238,   240,    10,   123,     3,   244,
     245,   246,   247,   244,   244,   244,   244,   129,   244,   131,
     244,   133,   244,   244,   136,   244,    12,    88,    91,    16,
      17,    21,    95,   102,    26,    27,   111,   112,    30,    31,
     118,   119,     5,     6,     4,     5,     5,     5,     5,   152,
     238,     5,   152,     5,   152,     5,     5,   152,     5,    14,
      33,    34,    35,    36,    37,    38,    59,    90,    92,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,    96,   103,   114,   121,   124,   246,   247,
       3,   248,   125,   126,   127,   128,   130,   132,   134,   135,
     137,     8,   234,   235,   236,   234,   234,   237,   244,   244,
     237,    60,    61,    62,    63,    64,   167,   168,   169,   170,
     171,   172,   173,    18,    19,    97,    98,    22,    23,   104,
     105,    28,    33,    34,    49,    50,    51,    52,    53,    59,
     113,   115,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,    32,    34,
      54,    55,    56,    57,    58,    59,   120,   122,   220,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   152,     4,   152,   152,   152,   152,   152,   152,
     152,   152,   152,   236,     6,     6,   100,   107,   234,   234,
     238,   238,   238,   238,   244,   244,   234,   244,   237,   237,
     234,   244,   244,   248,   234,   168,    20,    39,    40,    41,
      42,    43,    99,   101,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,    24,    39,    40,    44,    45,    46,
      47,    48,   106,   108,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   202,   203,   237,   237,
     238,     3,   241,   242,   241,   237,   237,   237,   237,     7,
     196,   197,   200,   244,     7,   198,   199,   201,   244,   239,
     240,     6,     5,     6,   242,     3,   243,   240,     5,   243
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 17:

/* Line 1806 of yacc.c  */
#line 359 "osm_qos_parser_y.y"
    {
                        __parser_port_group_start();
                    }
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 364 "osm_qos_parser_y.y"
    {
                        if ( __parser_port_group_end() )
                            return 1;
                    }
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 427 "osm_qos_parser_y.y"
    {
                        __parser_vlarb_scope_start();
                    }
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 432 "osm_qos_parser_y.y"
    {
                        if ( __parser_vlarb_scope_end() )
                            return 1;
                    }
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 473 "osm_qos_parser_y.y"
    {
                        __parser_sl2vl_scope_start();
                    }
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 478 "osm_qos_parser_y.y"
    {
                        if ( __parser_sl2vl_scope_end() )
                            return 1;
                    }
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 544 "osm_qos_parser_y.y"
    {
                        __parser_qos_level_start();
                    }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 549 "osm_qos_parser_y.y"
    {
                        if ( __parser_qos_level_end() )
                            return 1;
                    }
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 598 "osm_qos_parser_y.y"
    {
                        __parser_match_rule_start();
                    }
    break;

  case 81:

/* Line 1806 of yacc.c  */
#line 603 "osm_qos_parser_y.y"
    {
                        if ( __parser_match_rule_end() )
                            return 1;
                    }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 643 "osm_qos_parser_y.y"
    {
                        /* parsing default ulp rule: "default: num" */
                        cl_list_iterator_t    list_iterator;
                        uint64_t            * p_tmp_num;

                        list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                        p_tmp_num = (uint64_t*)cl_list_obj(list_iterator);
                        if (*p_tmp_num > 15)
                        {
                            yyerror("illegal SL value");
                            return 1;
                        }
                        __default_simple_qos_level.sl = (uint8_t)(*p_tmp_num);
                        __default_simple_qos_level.sl_set = TRUE;
                        free(p_tmp_num);
                        cl_list_remove_all(&tmp_parser_struct.num_list);
                    }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 661 "osm_qos_parser_y.y"
    {
                        /* "any, service-id ... : sl" - one instance of list of ranges */
                        uint64_t ** range_arr;
                        unsigned    range_len;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("ULP rule doesn't have service ids");
                            return 1;
                        }

                        /* get all the service id ranges */
                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = range_len;

                    }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 682 "osm_qos_parser_y.y"
    {
                        /* "any, pkey ... : sl" - one instance of list of ranges */
                        uint64_t ** range_arr;
                        unsigned    range_len;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("ULP rule doesn't have pkeys");
                            return 1;
                        }

                        /* get all the pkey ranges */
                        __pkey_rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        p_current_qos_match_rule->pkey_range_arr = range_arr;
                        p_current_qos_match_rule->pkey_range_len = range_len;

                    }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 703 "osm_qos_parser_y.y"
    {
                        /* any, target-port-guid ... : sl */
                        uint64_t ** range_arr;
                        unsigned    range_len;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("ULP rule doesn't have port guids");
                            return 1;
                        }

                        /* create a new port group with these ports */
                        __parser_port_group_start();

                        p_current_port_group->name = strdup("_ULP_Targets_");
                        p_current_port_group->use = strdup("Generated from ULP rules");

                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        __parser_add_guid_range_to_port_map(
                                              &p_current_port_group->port_map,
                                              range_arr,
                                              range_len);

                        /* add this port group to the destination
                           groups of the current match rule */
                        cl_list_insert_tail(&p_current_qos_match_rule->destination_group_list,
                                            p_current_port_group);

                        __parser_port_group_end();

                    }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 738 "osm_qos_parser_y.y"
    {
			/* any, source-port-guid ... : sl */
			uint64_t ** range_arr;
			unsigned    range_len;

			if (!cl_list_count(&tmp_parser_struct.num_pair_list))
			{
				yyerror("ULP rule doesn't have port guids");
				return 1;
			}

                        /* create a new port group with these ports */
                        __parser_port_group_start();

                        p_current_port_group->name = strdup("_ULP_Sources_");
                        p_current_port_group->use = strdup("Generated from ULP rules");

                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        __parser_add_guid_range_to_port_map(
                                              &p_current_port_group->port_map,
                                              range_arr,
                                              range_len);

                        /* add this port group to the source
                           groups of the current match rule */
                        cl_list_insert_tail(&p_current_qos_match_rule->source_group_list,
                                            p_current_port_group);

                        __parser_port_group_end();

		    }
    break;

  case 100:

/* Line 1806 of yacc.c  */
#line 773 "osm_qos_parser_y.y"
    {
			/* any, source-target-port-guid ... : sl */
			uint64_t ** range_arr;
			unsigned    range_len;

			if (!cl_list_count(&tmp_parser_struct.num_pair_list))
			{
				yyerror("ULP rule doesn't have port guids");
				return 1;
			}

                        /* create a new port group with these ports */
                        __parser_port_group_start();

                        p_current_port_group->name = strdup("_ULP_Sources_Targets_");
                        p_current_port_group->use = strdup("Generated from ULP rules");

                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        __parser_add_guid_range_to_port_map(
                                              &p_current_port_group->port_map,
                                              range_arr,
                                              range_len);

                        /* add this port group to the source and destination
                           groups of the current match rule */
                        cl_list_insert_tail(&p_current_qos_match_rule->source_group_list,
                                            p_current_port_group);

                        cl_list_insert_tail(&p_current_qos_match_rule->destination_group_list,
                                            p_current_port_group);

                        __parser_port_group_end();

		    }
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 811 "osm_qos_parser_y.y"
    {
                        /* "sdp : sl" - default SL for SDP */
                        uint64_t ** range_arr =
                               (uint64_t **)malloc(sizeof(uint64_t *));
                        range_arr[0] = (uint64_t *)malloc(2*sizeof(uint64_t));
                        range_arr[0][0] = OSM_QOS_POLICY_ULP_SDP_SERVICE_ID;
                        range_arr[0][1] = OSM_QOS_POLICY_ULP_SDP_SERVICE_ID + 0xFFFF;

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = 1;

                    }
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 824 "osm_qos_parser_y.y"
    {
                        /* sdp with port numbers */
                        uint64_t ** range_arr;
                        unsigned    range_len;
                        unsigned    i;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("SDP ULP rule doesn't have port numbers");
                            return 1;
                        }

                        /* get all the port ranges */
                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );
                        /* now translate these port numbers into service ids */
                        for (i = 0; i < range_len; i++)
                        {
                            if (range_arr[i][0] > 0xFFFF || range_arr[i][1] > 0xFFFF)
                            {
                                yyerror("SDP port number out of range");
				free(range_arr);
                                return 1;
                            }
                            range_arr[i][0] += OSM_QOS_POLICY_ULP_SDP_SERVICE_ID;
                            range_arr[i][1] += OSM_QOS_POLICY_ULP_SDP_SERVICE_ID;
                        }

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = range_len;

                    }
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 858 "osm_qos_parser_y.y"
    {
                        /* "rds : sl" - default SL for RDS */
                        uint64_t ** range_arr =
                               (uint64_t **)malloc(sizeof(uint64_t *));
                        range_arr[0] = (uint64_t *)malloc(2*sizeof(uint64_t));
                        range_arr[0][0] = range_arr[0][1] =
                           OSM_QOS_POLICY_ULP_RDS_SERVICE_ID + OSM_QOS_POLICY_ULP_RDS_PORT;

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = 1;

                    }
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 871 "osm_qos_parser_y.y"
    {
                        /* rds with port numbers */
                        uint64_t ** range_arr;
                        unsigned    range_len;
                        unsigned    i;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("RDS ULP rule doesn't have port numbers");
                            return 1;
                        }

                        /* get all the port ranges */
                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );
                        /* now translate these port numbers into service ids */
                        for (i = 0; i < range_len; i++)
                        {
                            if (range_arr[i][0] > 0xFFFF || range_arr[i][1] > 0xFFFF)
                            {
                                yyerror("SDP port number out of range");
				free(range_arr);
                                return 1;
                            }
                            range_arr[i][0] += OSM_QOS_POLICY_ULP_RDS_SERVICE_ID;
                            range_arr[i][1] += OSM_QOS_POLICY_ULP_RDS_SERVICE_ID;
                        }

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = range_len;

                    }
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 905 "osm_qos_parser_y.y"
    {
                        /* "iSER : sl" - default SL for iSER */
                        uint64_t ** range_arr =
                               (uint64_t **)malloc(sizeof(uint64_t *));
                        range_arr[0] = (uint64_t *)malloc(2*sizeof(uint64_t));
                        range_arr[0][0] = range_arr[0][1] =
                           OSM_QOS_POLICY_ULP_ISER_SERVICE_ID + OSM_QOS_POLICY_ULP_ISER_PORT;

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = 1;

                    }
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 918 "osm_qos_parser_y.y"
    {
                        /* iser with port numbers */
                        uint64_t ** range_arr;
                        unsigned    range_len;
                        unsigned    i;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("iSER ULP rule doesn't have port numbers");
                            return 1;
                        }

                        /* get all the port ranges */
                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );
                        /* now translate these port numbers into service ids */
                        for (i = 0; i < range_len; i++)
                        {
                            if (range_arr[i][0] > 0xFFFF || range_arr[i][1] > 0xFFFF)
                            {
                                yyerror("SDP port number out of range");
				free(range_arr);
                                return 1;
                            }
                            range_arr[i][0] += OSM_QOS_POLICY_ULP_ISER_SERVICE_ID;
                            range_arr[i][1] += OSM_QOS_POLICY_ULP_ISER_SERVICE_ID;
                        }

                        p_current_qos_match_rule->service_id_range_arr = range_arr;
                        p_current_qos_match_rule->service_id_range_len = range_len;

                    }
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 952 "osm_qos_parser_y.y"
    {
                        /* srp with target guids - this rule is similar
                           to writing 'any' ulp with target port guids */
                        uint64_t ** range_arr;
                        unsigned    range_len;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("SRP ULP rule doesn't have port guids");
                            return 1;
                        }

                        /* create a new port group with these ports */
                        __parser_port_group_start();

                        p_current_port_group->name = strdup("_SRP_Targets_");
                        p_current_port_group->use = strdup("Generated from ULP rules");

                        __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        __parser_add_guid_range_to_port_map(
                                              &p_current_port_group->port_map,
                                              range_arr,
                                              range_len);

                        /* add this port group to the destination
                           groups of the current match rule */
                        cl_list_insert_tail(&p_current_qos_match_rule->destination_group_list,
                                            p_current_port_group);

                        __parser_port_group_end();

                    }
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 988 "osm_qos_parser_y.y"
    {
                        /* ipoib w/o any pkeys (default pkey) */
                        uint64_t ** range_arr =
                               (uint64_t **)malloc(sizeof(uint64_t *));
                        range_arr[0] = (uint64_t *)malloc(2*sizeof(uint64_t));
                        range_arr[0][0] = range_arr[0][1] = 0x7fff;

                        /*
                         * Although we know that the default partition exists,
                         * we still need to validate it by checking that it has
                         * at least two full members. Otherwise IPoIB won't work.
                         */
                        if (__validate_pkeys(range_arr, 1, TRUE))
                            return 1;

                        p_current_qos_match_rule->pkey_range_arr = range_arr;
                        p_current_qos_match_rule->pkey_range_len = 1;

                    }
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 1008 "osm_qos_parser_y.y"
    {
                        /* ipoib with pkeys */
                        uint64_t ** range_arr;
                        unsigned    range_len;

                        if (!cl_list_count(&tmp_parser_struct.num_pair_list))
                        {
                            yyerror("IPoIB ULP rule doesn't have pkeys");
                            return 1;
                        }

                        /* get all the pkey ranges */
                        __pkey_rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                              &range_arr,
                                              &range_len );

                        /*
                         * Validate pkeys.
                         * For IPoIB pkeys the validation is strict.
                         * If some problem would be found, parsing will
                         * be aborted with a proper error messages.
                         */
			if (__validate_pkeys(range_arr, range_len, TRUE)) {
			    free(range_arr);
                            return 1;
			}

                        p_current_qos_match_rule->pkey_range_arr = range_arr;
                        p_current_qos_match_rule->pkey_range_len = range_len;

                    }
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 1042 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 1045 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 122:

/* Line 1806 of yacc.c  */
#line 1048 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 1051 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 1054 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 1057 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 1060 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 1063 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 1066 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 1069 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 1072 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 1075 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 1078 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 1081 "osm_qos_parser_y.y"
    { __parser_ulp_match_rule_start(); }
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 1084 "osm_qos_parser_y.y"
    {
                        /* get the SL for ULP rules */
                        cl_list_iterator_t  list_iterator;
                        uint64_t          * p_tmp_num;
                        uint8_t             sl;

                        list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                        p_tmp_num = (uint64_t*)cl_list_obj(list_iterator);
                        if (*p_tmp_num > 15)
                        {
                            yyerror("illegal SL value");
                            return 1;
                        }

                        sl = (uint8_t)(*p_tmp_num);
                        free(p_tmp_num);
                        cl_list_remove_all(&tmp_parser_struct.num_list);

                        p_current_qos_match_rule->p_qos_level =
                                 &osm_qos_policy_simple_qos_levels[sl];
                        p_current_qos_match_rule->qos_level_name =
                                 strdup(osm_qos_policy_simple_qos_levels[sl].name);

                        if (__parser_ulp_match_rule_end())
                            return 1;
                    }
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 1123 "osm_qos_parser_y.y"
    {
                            /* 'name' of 'port-group' - one instance */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            if (p_current_port_group->name)
                            {
                                yyerror("port-group has multiple 'name' tags");
                                cl_list_remove_all(&tmp_parser_struct.str_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            if ( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    p_current_port_group->name = tmp_str;
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 1146 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1151 "osm_qos_parser_y.y"
    {
                            /* 'use' of 'port-group' - one instance */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            if (p_current_port_group->use)
                            {
                                yyerror("port-group has multiple 'use' tags");
                                cl_list_remove_all(&tmp_parser_struct.str_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            if ( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    p_current_port_group->use = tmp_str;
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1174 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1179 "osm_qos_parser_y.y"
    {
                            /* 'port-name' in 'port-group' - any num of instances */
                            cl_list_iterator_t list_iterator;
                            osm_node_t * p_node;
                            osm_physp_t * p_physp;
                            unsigned port_num;
                            char * tmp_str;
                            char * port_str;

                            /* parsing port name strings */
                            for (list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                                 list_iterator != cl_list_end(&tmp_parser_struct.str_list);
                                 list_iterator = cl_list_next(list_iterator))
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                {
                                    /* last slash in port name string is a separator
                                       between node name and port number */
                                    port_str = strrchr(tmp_str, '/');
                                    if (!port_str || (strlen(port_str) < 3) ||
                                        (port_str[1] != 'p' && port_str[1] != 'P')) {
                                        yyerror("'%s' - illegal port name",
                                                           tmp_str);
                                        free(tmp_str);
                                        cl_list_remove_all(&tmp_parser_struct.str_list);
                                        return 1;
                                    }

                                    if (!(port_num = strtoul(&port_str[2],NULL,0))) {
                                        yyerror(
                                               "'%s' - illegal port number in port name",
                                               tmp_str);
                                        free(tmp_str);
                                        cl_list_remove_all(&tmp_parser_struct.str_list);
                                        return 1;
                                    }

                                    /* separate node name from port number */
                                    port_str[0] = '\0';

                                    if (st_lookup(p_qos_policy->p_node_hash,
                                                  (st_data_t)tmp_str,
                                                  (void *)&p_node))
                                    {
                                        /* we found the node, now get the right port */
                                        p_physp = osm_node_get_physp_ptr(p_node, port_num);
                                        if (!p_physp) {
                                            yyerror(
                                                   "'%s' - port number out of range in port name",
                                                   tmp_str);
                                            free(tmp_str);
                                            cl_list_remove_all(&tmp_parser_struct.str_list);
                                            return 1;
                                        }
                                        /* we found the port, now add it to guid table */
                                        __parser_add_port_to_port_map(&p_current_port_group->port_map,
                                                                      p_physp);
                                    }
                                    free(tmp_str);
                                }
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1245 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1250 "osm_qos_parser_y.y"
    {
                            /* 'port-guid' in 'port-group' - any num of instances */
                            /* list of guid ranges */
                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                __parser_add_guid_range_to_port_map(
                                                      &p_current_port_group->port_map,
                                                      range_arr,
                                                      range_len);
                            }
                        }
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1270 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1275 "osm_qos_parser_y.y"
    {
                            /* 'pkey' in 'port-group' - any num of instances */
                            /* list of pkey ranges */
                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __pkey_rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                __parser_add_pkey_range_to_port_map(
                                                      &p_current_port_group->port_map,
                                                      range_arr,
                                                      range_len);
                            }
                        }
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1295 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 1300 "osm_qos_parser_y.y"
    {
                            /* 'partition' in 'port-group' - any num of instances */
                            __parser_add_partition_list_to_port_map(
                                               &p_current_port_group->port_map,
                                               &tmp_parser_struct.str_list);
                        }
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 1308 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 1313 "osm_qos_parser_y.y"
    {
                            /* 'node-type' in 'port-group' - any num of instances */
                        }
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 1318 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 156:

/* Line 1806 of yacc.c  */
#line 1334 "osm_qos_parser_y.y"
    {
                            p_current_port_group->node_types |=
                               OSM_QOS_POLICY_NODE_TYPE_CA;
                        }
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 1340 "osm_qos_parser_y.y"
    {
                            p_current_port_group->node_types |=
                               OSM_QOS_POLICY_NODE_TYPE_SWITCH;
                        }
    break;

  case 158:

/* Line 1806 of yacc.c  */
#line 1346 "osm_qos_parser_y.y"
    {
                            p_current_port_group->node_types |=
                               OSM_QOS_POLICY_NODE_TYPE_ROUTER;
                        }
    break;

  case 159:

/* Line 1806 of yacc.c  */
#line 1352 "osm_qos_parser_y.y"
    {
                            p_current_port_group->node_types |=
                               (OSM_QOS_POLICY_NODE_TYPE_CA |
                                OSM_QOS_POLICY_NODE_TYPE_SWITCH |
                                OSM_QOS_POLICY_NODE_TYPE_ROUTER);
                        }
    break;

  case 160:

/* Line 1806 of yacc.c  */
#line 1360 "osm_qos_parser_y.y"
    {
                            osm_port_t * p_osm_port =
                                osm_get_port_by_guid(p_qos_policy->p_subn,
                                     p_qos_policy->p_subn->sm_port_guid);
                            if (p_osm_port)
                                __parser_add_port_to_port_map(
                                   &p_current_port_group->port_map,
                                   p_osm_port->p_physp);
                        }
    break;

  case 161:

/* Line 1806 of yacc.c  */
#line 1382 "osm_qos_parser_y.y"
    {
                            /* 'group' in 'vlarb-scope' - any num of instances */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    cl_list_insert_tail(&p_current_vlarb_scope->group_list,tmp_str);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 162:

/* Line 1806 of yacc.c  */
#line 1399 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 163:

/* Line 1806 of yacc.c  */
#line 1404 "osm_qos_parser_y.y"
    {
                            /* 'across' in 'vlarb-scope' - any num of instances */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    cl_list_insert_tail(&p_current_vlarb_scope->across_list,tmp_str);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 164:

/* Line 1806 of yacc.c  */
#line 1421 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 165:

/* Line 1806 of yacc.c  */
#line 1426 "osm_qos_parser_y.y"
    {
                            /* 'vl-high-limit' in 'vlarb-scope' - one instance of one number */
                            cl_list_iterator_t    list_iterator;
                            uint64_t            * p_tmp_num;

                            list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                            p_tmp_num = (uint64_t*)cl_list_obj(list_iterator);
                            if (p_tmp_num)
                            {
                                p_current_vlarb_scope->vl_high_limit = (uint32_t)(*p_tmp_num);
                                p_current_vlarb_scope->vl_high_limit_set = TRUE;
                                free(p_tmp_num);
                            }

                            cl_list_remove_all(&tmp_parser_struct.num_list);
                        }
    break;

  case 166:

/* Line 1806 of yacc.c  */
#line 1444 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 167:

/* Line 1806 of yacc.c  */
#line 1449 "osm_qos_parser_y.y"
    {
                            /* 'vlarb-high' in 'vlarb-scope' - list of pairs of numbers with ':' and ',' */
                            cl_list_iterator_t    list_iterator;
                            uint64_t            * num_pair;

                            list_iterator = cl_list_head(&tmp_parser_struct.num_pair_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.num_pair_list) )
                            {
                                num_pair = (uint64_t*)cl_list_obj(list_iterator);
                                if (num_pair)
                                    cl_list_insert_tail(&p_current_vlarb_scope->vlarb_high_list,num_pair);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.num_pair_list);
                        }
    break;

  case 168:

/* Line 1806 of yacc.c  */
#line 1466 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 169:

/* Line 1806 of yacc.c  */
#line 1471 "osm_qos_parser_y.y"
    {
                            /* 'vlarb-low' in 'vlarb-scope' - list of pairs of numbers with ':' and ',' */
                            cl_list_iterator_t    list_iterator;
                            uint64_t            * num_pair;

                            list_iterator = cl_list_head(&tmp_parser_struct.num_pair_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.num_pair_list) )
                            {
                                num_pair = (uint64_t*)cl_list_obj(list_iterator);
                                if (num_pair)
                                    cl_list_insert_tail(&p_current_vlarb_scope->vlarb_low_list,num_pair);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.num_pair_list);
                        }
    break;

  case 170:

/* Line 1806 of yacc.c  */
#line 1488 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 171:

/* Line 1806 of yacc.c  */
#line 1504 "osm_qos_parser_y.y"
    {
                            /* 'group' in 'sl2vl-scope' - any num of instances */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    cl_list_insert_tail(&p_current_sl2vl_scope->group_list,tmp_str);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 172:

/* Line 1806 of yacc.c  */
#line 1521 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 173:

/* Line 1806 of yacc.c  */
#line 1526 "osm_qos_parser_y.y"
    {
                            /* 'across' in 'sl2vl-scope' - any num of instances */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str) {
                                    cl_list_insert_tail(&p_current_sl2vl_scope->across_from_list,tmp_str);
                                    cl_list_insert_tail(&p_current_sl2vl_scope->across_to_list,strdup(tmp_str));
                                }
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 174:

/* Line 1806 of yacc.c  */
#line 1545 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 175:

/* Line 1806 of yacc.c  */
#line 1550 "osm_qos_parser_y.y"
    {
                            /* 'across-from' in 'sl2vl-scope' - any num of instances */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    cl_list_insert_tail(&p_current_sl2vl_scope->across_from_list,tmp_str);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 176:

/* Line 1806 of yacc.c  */
#line 1567 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 177:

/* Line 1806 of yacc.c  */
#line 1572 "osm_qos_parser_y.y"
    {
                            /* 'across-to' in 'sl2vl-scope' - any num of instances */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str) {
                                    cl_list_insert_tail(&p_current_sl2vl_scope->across_to_list,tmp_str);
                                }
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 178:

/* Line 1806 of yacc.c  */
#line 1590 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 179:

/* Line 1806 of yacc.c  */
#line 1595 "osm_qos_parser_y.y"
    {
                            /* 'from' in 'sl2vl-scope' - any num of instances */
                        }
    break;

  case 180:

/* Line 1806 of yacc.c  */
#line 1600 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 181:

/* Line 1806 of yacc.c  */
#line 1605 "osm_qos_parser_y.y"
    {
                            /* 'to' in 'sl2vl-scope' - any num of instances */
                        }
    break;

  case 182:

/* Line 1806 of yacc.c  */
#line 1610 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 185:

/* Line 1806 of yacc.c  */
#line 1619 "osm_qos_parser_y.y"
    {
                            int i;
                            for (i = 0; i < OSM_QOS_POLICY_MAX_PORTS_ON_SWITCH; i++)
                                p_current_sl2vl_scope->from[i] = TRUE;
                        }
    break;

  case 188:

/* Line 1806 of yacc.c  */
#line 1630 "osm_qos_parser_y.y"
    {
                            int i;
                            for (i = 0; i < OSM_QOS_POLICY_MAX_PORTS_ON_SWITCH; i++)
                                p_current_sl2vl_scope->to[i] = TRUE;
                        }
    break;

  case 189:

/* Line 1806 of yacc.c  */
#line 1637 "osm_qos_parser_y.y"
    {
                            int i;
                            cl_list_iterator_t    list_iterator;
                            uint64_t            * num_pair;
                            uint8_t               num1, num2;

                            list_iterator = cl_list_head(&tmp_parser_struct.num_pair_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.num_pair_list) )
                            {
                                num_pair = (uint64_t*)cl_list_obj(list_iterator);
                                if (num_pair)
                                {
                                    if ( num_pair[0] < 0 ||
                                         num_pair[1] >= OSM_QOS_POLICY_MAX_PORTS_ON_SWITCH )
                                    {
                                        yyerror("port number out of range 'from' list");
                                        free(num_pair);
                                        cl_list_remove_all(&tmp_parser_struct.num_pair_list);
                                        return 1;
                                    }
                                    num1 = (uint8_t)num_pair[0];
                                    num2 = (uint8_t)num_pair[1];
                                    free(num_pair);
                                    for (i = num1; i <= num2; i++)
                                        p_current_sl2vl_scope->from[i] = TRUE;
                                }
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.num_pair_list);
                        }
    break;

  case 190:

/* Line 1806 of yacc.c  */
#line 1669 "osm_qos_parser_y.y"
    {
                            int i;
                            cl_list_iterator_t    list_iterator;
                            uint64_t            * num_pair;
                            uint8_t               num1, num2;

                            list_iterator = cl_list_head(&tmp_parser_struct.num_pair_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.num_pair_list) )
                            {
                                num_pair = (uint64_t*)cl_list_obj(list_iterator);
                                if (num_pair)
                                {
                                    if ( num_pair[0] < 0 ||
                                         num_pair[1] >= OSM_QOS_POLICY_MAX_PORTS_ON_SWITCH )
                                    {
                                        yyerror("port number out of range 'to' list");
                                        free(num_pair);
                                        cl_list_remove_all(&tmp_parser_struct.num_pair_list);
                                        return 1;
                                    }
                                    num1 = (uint8_t)num_pair[0];
                                    num2 = (uint8_t)num_pair[1];
                                    free(num_pair);
                                    for (i = num1; i <= num2; i++)
                                        p_current_sl2vl_scope->to[i] = TRUE;
                                }
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.num_pair_list);
                        }
    break;

  case 191:

/* Line 1806 of yacc.c  */
#line 1702 "osm_qos_parser_y.y"
    {
                            /* 'sl2vl-table' - one instance of exactly
                               OSM_QOS_POLICY_SL2VL_TABLE_LEN numbers */
                            cl_list_iterator_t    list_iterator;
                            uint64_t              num;
                            uint64_t            * p_num;
                            int                   i = 0;

                            if (p_current_sl2vl_scope->sl2vl_table_set)
                            {
                                yyerror("sl2vl-scope has more than one sl2vl-table");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }

                            if (cl_list_count(&tmp_parser_struct.num_list) != OSM_QOS_POLICY_SL2VL_TABLE_LEN)
                            {
                                yyerror("wrong number of values in 'sl2vl-table' (should be 16)");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.num_list) )
                            {
                                p_num = (uint64_t*)cl_list_obj(list_iterator);
                                num = *p_num;
                                free(p_num);
                                if (num >= OSM_QOS_POLICY_MAX_VL_NUM)
                                {
                                    yyerror("wrong VL value in 'sl2vl-table' (should be 0 to 15)");
                                    cl_list_remove_all(&tmp_parser_struct.num_list);
                                    return 1;
                                }

                                p_current_sl2vl_scope->sl2vl_table[i++] = (uint8_t)num;
                                list_iterator = cl_list_next(list_iterator);
                            }
                            p_current_sl2vl_scope->sl2vl_table_set = TRUE;
                            cl_list_remove_all(&tmp_parser_struct.num_list);
                        }
    break;

  case 192:

/* Line 1806 of yacc.c  */
#line 1745 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 193:

/* Line 1806 of yacc.c  */
#line 1762 "osm_qos_parser_y.y"
    {
                            /* 'name' of 'qos-level' - one instance */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            if (p_current_qos_level->name)
                            {
                                yyerror("qos-level has multiple 'name' tags");
                                cl_list_remove_all(&tmp_parser_struct.str_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            if ( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    p_current_qos_level->name = tmp_str;
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 194:

/* Line 1806 of yacc.c  */
#line 1785 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 195:

/* Line 1806 of yacc.c  */
#line 1790 "osm_qos_parser_y.y"
    {
                            /* 'use' of 'qos-level' - one instance */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            if (p_current_qos_level->use)
                            {
                                yyerror("qos-level has multiple 'use' tags");
                                cl_list_remove_all(&tmp_parser_struct.str_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            if ( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    p_current_qos_level->use = tmp_str;
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 196:

/* Line 1806 of yacc.c  */
#line 1813 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 197:

/* Line 1806 of yacc.c  */
#line 1818 "osm_qos_parser_y.y"
    {
                            /* 'sl' in 'qos-level' - one instance */
                            cl_list_iterator_t   list_iterator;
                            uint64_t           * p_num;

                            if (p_current_qos_level->sl_set)
                            {
                                yyerror("'qos-level' has multiple 'sl' tags");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }
                            list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                            p_num = (uint64_t*)cl_list_obj(list_iterator);
                            p_current_qos_level->sl = (uint8_t)(*p_num);
                            free(p_num);
                            p_current_qos_level->sl_set = TRUE;
                            cl_list_remove_all(&tmp_parser_struct.num_list);
                        }
    break;

  case 198:

/* Line 1806 of yacc.c  */
#line 1838 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 199:

/* Line 1806 of yacc.c  */
#line 1843 "osm_qos_parser_y.y"
    {
                            /* 'mtu-limit' in 'qos-level' - one instance */
                            cl_list_iterator_t   list_iterator;
                            uint64_t           * p_num;

                            if (p_current_qos_level->mtu_limit_set)
                            {
                                yyerror("'qos-level' has multiple 'mtu-limit' tags");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }
                            list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                            p_num = (uint64_t*)cl_list_obj(list_iterator);
                            p_current_qos_level->mtu_limit = (uint8_t)(*p_num);
                            free(p_num);
                            p_current_qos_level->mtu_limit_set = TRUE;
                            cl_list_remove_all(&tmp_parser_struct.num_list);
                        }
    break;

  case 200:

/* Line 1806 of yacc.c  */
#line 1863 "osm_qos_parser_y.y"
    {
                            /* 'mtu-limit' in 'qos-level' - one instance */
                            RESET_BUFFER;
                        }
    break;

  case 201:

/* Line 1806 of yacc.c  */
#line 1869 "osm_qos_parser_y.y"
    {
                            /* 'rate-limit' in 'qos-level' - one instance */
                            cl_list_iterator_t   list_iterator;
                            uint64_t           * p_num;

                            if (p_current_qos_level->rate_limit_set)
                            {
                                yyerror("'qos-level' has multiple 'rate-limit' tags");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }
                            list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                            p_num = (uint64_t*)cl_list_obj(list_iterator);
                            p_current_qos_level->rate_limit = (uint8_t)(*p_num);
                            free(p_num);
                            p_current_qos_level->rate_limit_set = TRUE;
                            cl_list_remove_all(&tmp_parser_struct.num_list);
                        }
    break;

  case 202:

/* Line 1806 of yacc.c  */
#line 1889 "osm_qos_parser_y.y"
    {
                            /* 'rate-limit' in 'qos-level' - one instance */
                            RESET_BUFFER;
                        }
    break;

  case 203:

/* Line 1806 of yacc.c  */
#line 1895 "osm_qos_parser_y.y"
    {
                            /* 'packet-life' in 'qos-level' - one instance */
                            cl_list_iterator_t   list_iterator;
                            uint64_t           * p_num;

                            if (p_current_qos_level->pkt_life_set)
                            {
                                yyerror("'qos-level' has multiple 'packet-life' tags");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }
                            list_iterator = cl_list_head(&tmp_parser_struct.num_list);
                            p_num = (uint64_t*)cl_list_obj(list_iterator);
                            p_current_qos_level->pkt_life = (uint8_t)(*p_num);
                            free(p_num);
                            p_current_qos_level->pkt_life_set= TRUE;
                            cl_list_remove_all(&tmp_parser_struct.num_list);
                        }
    break;

  case 204:

/* Line 1806 of yacc.c  */
#line 1915 "osm_qos_parser_y.y"
    {
                            /* 'packet-life' in 'qos-level' - one instance */
                            RESET_BUFFER;
                        }
    break;

  case 205:

/* Line 1806 of yacc.c  */
#line 1921 "osm_qos_parser_y.y"
    {
                            /* 'path-bits' in 'qos-level' - any num of instances */
                            /* list of path bit ranges */

                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                if ( !p_current_qos_level->path_bits_range_len )
                                {
                                    p_current_qos_level->path_bits_range_arr = range_arr;
                                    p_current_qos_level->path_bits_range_len = range_len;
                                }
                                else
                                {
                                    uint64_t ** new_range_arr;
                                    unsigned new_range_len;
                                    __merge_rangearr( p_current_qos_level->path_bits_range_arr,
                                                      p_current_qos_level->path_bits_range_len,
                                                      range_arr,
                                                      range_len,
                                                      &new_range_arr,
                                                      &new_range_len );
                                    p_current_qos_level->path_bits_range_arr = new_range_arr;
                                    p_current_qos_level->path_bits_range_len = new_range_len;
                                }
                            }
                        }
    break;

  case 206:

/* Line 1806 of yacc.c  */
#line 1956 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 207:

/* Line 1806 of yacc.c  */
#line 1961 "osm_qos_parser_y.y"
    {
                            /* 'pkey' in 'qos-level' - num of instances of list of ranges */
                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __pkey_rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                if ( !p_current_qos_level->pkey_range_len )
                                {
                                    p_current_qos_level->pkey_range_arr = range_arr;
                                    p_current_qos_level->pkey_range_len = range_len;
                                }
                                else
                                {
                                    uint64_t ** new_range_arr;
                                    unsigned new_range_len;
                                    __merge_rangearr( p_current_qos_level->pkey_range_arr,
                                                      p_current_qos_level->pkey_range_len,
                                                      range_arr,
                                                      range_len,
                                                      &new_range_arr,
                                                      &new_range_len );
                                    p_current_qos_level->pkey_range_arr = new_range_arr;
                                    p_current_qos_level->pkey_range_len = new_range_len;
                                }
                            }
                        }
    break;

  case 208:

/* Line 1806 of yacc.c  */
#line 1994 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 209:

/* Line 1806 of yacc.c  */
#line 2011 "osm_qos_parser_y.y"
    {
                            /* 'use' of 'qos-match-rule' - one instance */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            if (p_current_qos_match_rule->use)
                            {
                                yyerror("'qos-match-rule' has multiple 'use' tags");
                                cl_list_remove_all(&tmp_parser_struct.str_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            if ( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    p_current_qos_match_rule->use = tmp_str;
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 210:

/* Line 1806 of yacc.c  */
#line 2034 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 211:

/* Line 1806 of yacc.c  */
#line 2039 "osm_qos_parser_y.y"
    {
                            /* 'qos-class' in 'qos-match-rule' - num of instances of list of ranges */
                            /* list of class ranges (QoS Class is 12-bit value) */
                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                if ( !p_current_qos_match_rule->qos_class_range_len )
                                {
                                    p_current_qos_match_rule->qos_class_range_arr = range_arr;
                                    p_current_qos_match_rule->qos_class_range_len = range_len;
                                }
                                else
                                {
                                    uint64_t ** new_range_arr;
                                    unsigned new_range_len;
                                    __merge_rangearr( p_current_qos_match_rule->qos_class_range_arr,
                                                      p_current_qos_match_rule->qos_class_range_len,
                                                      range_arr,
                                                      range_len,
                                                      &new_range_arr,
                                                      &new_range_len );
                                    p_current_qos_match_rule->qos_class_range_arr = new_range_arr;
                                    p_current_qos_match_rule->qos_class_range_len = new_range_len;
                                }
                            }
                        }
    break;

  case 212:

/* Line 1806 of yacc.c  */
#line 2073 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 213:

/* Line 1806 of yacc.c  */
#line 2078 "osm_qos_parser_y.y"
    {
                            /* 'source' in 'qos-match-rule' - text */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    cl_list_insert_tail(&p_current_qos_match_rule->source_list,tmp_str);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 214:

/* Line 1806 of yacc.c  */
#line 2095 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 215:

/* Line 1806 of yacc.c  */
#line 2100 "osm_qos_parser_y.y"
    {
                            /* 'destination' in 'qos-match-rule' - text */
                            cl_list_iterator_t    list_iterator;
                            char                * tmp_str;

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            while( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    cl_list_insert_tail(&p_current_qos_match_rule->destination_list,tmp_str);
                                list_iterator = cl_list_next(list_iterator);
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 216:

/* Line 1806 of yacc.c  */
#line 2117 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 217:

/* Line 1806 of yacc.c  */
#line 2122 "osm_qos_parser_y.y"
    {
                            /* 'qos-level-name' in 'qos-match-rule' - single string */
                            cl_list_iterator_t   list_iterator;
                            char               * tmp_str;

                            if (p_current_qos_match_rule->qos_level_name)
                            {
                                yyerror("qos-match-rule has multiple 'qos-level-name' tags");
                                cl_list_remove_all(&tmp_parser_struct.num_list);
                                return 1;
                            }

                            list_iterator = cl_list_head(&tmp_parser_struct.str_list);
                            if ( list_iterator != cl_list_end(&tmp_parser_struct.str_list) )
                            {
                                tmp_str = (char*)cl_list_obj(list_iterator);
                                if (tmp_str)
                                    p_current_qos_match_rule->qos_level_name = tmp_str;
                            }
                            cl_list_remove_all(&tmp_parser_struct.str_list);
                        }
    break;

  case 218:

/* Line 1806 of yacc.c  */
#line 2145 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 219:

/* Line 1806 of yacc.c  */
#line 2150 "osm_qos_parser_y.y"
    {
                            /* 'service-id' in 'qos-match-rule' - num of instances of list of ranges */
                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                if ( !p_current_qos_match_rule->service_id_range_len )
                                {
                                    p_current_qos_match_rule->service_id_range_arr = range_arr;
                                    p_current_qos_match_rule->service_id_range_len = range_len;
                                }
                                else
                                {
                                    uint64_t ** new_range_arr;
                                    unsigned new_range_len;
                                    __merge_rangearr( p_current_qos_match_rule->service_id_range_arr,
                                                      p_current_qos_match_rule->service_id_range_len,
                                                      range_arr,
                                                      range_len,
                                                      &new_range_arr,
                                                      &new_range_len );
                                    p_current_qos_match_rule->service_id_range_arr = new_range_arr;
                                    p_current_qos_match_rule->service_id_range_len = new_range_len;
                                }
                            }
                        }
    break;

  case 220:

/* Line 1806 of yacc.c  */
#line 2183 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 221:

/* Line 1806 of yacc.c  */
#line 2188 "osm_qos_parser_y.y"
    {
                            /* 'pkey' in 'qos-match-rule' - num of instances of list of ranges */
                            if (cl_list_count(&tmp_parser_struct.num_pair_list))
                            {
                                uint64_t ** range_arr;
                                unsigned range_len;

                                __pkey_rangelist2rangearr( &tmp_parser_struct.num_pair_list,
                                                      &range_arr,
                                                      &range_len );

                                if ( !p_current_qos_match_rule->pkey_range_len )
                                {
                                    p_current_qos_match_rule->pkey_range_arr = range_arr;
                                    p_current_qos_match_rule->pkey_range_len = range_len;
                                }
                                else
                                {
                                    uint64_t ** new_range_arr;
                                    unsigned new_range_len;
                                    __merge_rangearr( p_current_qos_match_rule->pkey_range_arr,
                                                      p_current_qos_match_rule->pkey_range_len,
                                                      range_arr,
                                                      range_len,
                                                      &new_range_arr,
                                                      &new_range_len );
                                    p_current_qos_match_rule->pkey_range_arr = new_range_arr;
                                    p_current_qos_match_rule->pkey_range_len = new_range_len;
                                }
                            }
                        }
    break;

  case 222:

/* Line 1806 of yacc.c  */
#line 2221 "osm_qos_parser_y.y"
    {
                            RESET_BUFFER;
                        }
    break;

  case 223:

/* Line 1806 of yacc.c  */
#line 2232 "osm_qos_parser_y.y"
    {
                        cl_list_insert_tail(&tmp_parser_struct.str_list,
                                            strdup(__parser_strip_white(tmp_parser_struct.str)));
                        tmp_parser_struct.str[0] = '\0';
                    }
    break;

  case 226:

/* Line 1806 of yacc.c  */
#line 2243 "osm_qos_parser_y.y"
    {
                        strcat(tmp_parser_struct.str,(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                    }
    break;

  case 232:

/* Line 1806 of yacc.c  */
#line 2263 "osm_qos_parser_y.y"
    {
                        uint64_t * p_num = (uint64_t*)malloc(sizeof(uint64_t));
                        __parser_str2uint64(p_num,(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                        cl_list_insert_tail(&tmp_parser_struct.num_list, p_num);
                    }
    break;

  case 233:

/* Line 1806 of yacc.c  */
#line 2271 "osm_qos_parser_y.y"
    {
                        uint64_t * num_pair = (uint64_t*)malloc(sizeof(uint64_t)*2);
                        num_pair[0] = tmp_parser_struct.num_pair[0];
                        num_pair[1] = tmp_parser_struct.num_pair[1];
                        cl_list_insert_tail(&tmp_parser_struct.num_pair_list, num_pair);
                    }
    break;

  case 234:

/* Line 1806 of yacc.c  */
#line 2277 "osm_qos_parser_y.y"
    {
                        uint64_t * num_pair = (uint64_t*)malloc(sizeof(uint64_t)*2);
                        num_pair[0] = tmp_parser_struct.num_pair[0];
                        num_pair[1] = tmp_parser_struct.num_pair[1];
                        cl_list_insert_tail(&tmp_parser_struct.num_pair_list, num_pair);
                    }
    break;

  case 235:

/* Line 1806 of yacc.c  */
#line 2285 "osm_qos_parser_y.y"
    {
                        __parser_str2uint64(&tmp_parser_struct.num_pair[0],(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                    }
    break;

  case 236:

/* Line 1806 of yacc.c  */
#line 2291 "osm_qos_parser_y.y"
    {
                        __parser_str2uint64(&tmp_parser_struct.num_pair[1],(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                    }
    break;

  case 238:

/* Line 1806 of yacc.c  */
#line 2300 "osm_qos_parser_y.y"
    {
                        uint64_t * num_pair = (uint64_t*)malloc(sizeof(uint64_t)*2);
                        num_pair[0] = tmp_parser_struct.num_pair[0];
                        num_pair[1] = tmp_parser_struct.num_pair[1];
                        cl_list_insert_tail(&tmp_parser_struct.num_pair_list, num_pair);
                    }
    break;

  case 239:

/* Line 1806 of yacc.c  */
#line 2306 "osm_qos_parser_y.y"
    {
                        uint64_t * num_pair = (uint64_t*)malloc(sizeof(uint64_t)*2);
                        if (tmp_parser_struct.num_pair[0] <= tmp_parser_struct.num_pair[1]) {
                            num_pair[0] = tmp_parser_struct.num_pair[0];
                            num_pair[1] = tmp_parser_struct.num_pair[1];
                        }
                        else {
                            num_pair[1] = tmp_parser_struct.num_pair[0];
                            num_pair[0] = tmp_parser_struct.num_pair[1];
                        }
                        cl_list_insert_tail(&tmp_parser_struct.num_pair_list, num_pair);
                    }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 2318 "osm_qos_parser_y.y"
    {
                        uint64_t * num_pair = (uint64_t*)malloc(sizeof(uint64_t)*2);
                        if (tmp_parser_struct.num_pair[0] <= tmp_parser_struct.num_pair[1]) {
                            num_pair[0] = tmp_parser_struct.num_pair[0];
                            num_pair[1] = tmp_parser_struct.num_pair[1];
                        }
                        else {
                            num_pair[1] = tmp_parser_struct.num_pair[0];
                            num_pair[0] = tmp_parser_struct.num_pair[1];
                        }
                        cl_list_insert_tail(&tmp_parser_struct.num_pair_list, num_pair);
                    }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 2330 "osm_qos_parser_y.y"
    {
                        uint64_t * num_pair = (uint64_t*)malloc(sizeof(uint64_t)*2);
                        num_pair[0] = tmp_parser_struct.num_pair[0];
                        num_pair[1] = tmp_parser_struct.num_pair[1];
                        cl_list_insert_tail(&tmp_parser_struct.num_pair_list, num_pair);
                    }
    break;

  case 242:

/* Line 1806 of yacc.c  */
#line 2338 "osm_qos_parser_y.y"
    {
                        __parser_str2uint64(&tmp_parser_struct.num_pair[0],(yyvsp[(1) - (1)]));
                        __parser_str2uint64(&tmp_parser_struct.num_pair[1],(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                    }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 2345 "osm_qos_parser_y.y"
    {
                        __parser_str2uint64(&tmp_parser_struct.num_pair[0],(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                    }
    break;

  case 244:

/* Line 1806 of yacc.c  */
#line 2351 "osm_qos_parser_y.y"
    {
                        __parser_str2uint64(&tmp_parser_struct.num_pair[1],(yyvsp[(1) - (1)]));
                        free((yyvsp[(1) - (1)]));
                    }
    break;



/* Line 1806 of yacc.c  */
#line 4317 "osm_qos_parser_y.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 2067 of yacc.c  */
#line 2357 "osm_qos_parser_y.y"


/***************************************************
 ***************************************************/

int osm_qos_parse_policy_file(IN osm_subn_t * p_subn)
{
    int res = 0;
    static boolean_t first_time = TRUE;
    p_qos_parser_osm_log = &p_subn->p_osm->log;

    OSM_LOG_ENTER(p_qos_parser_osm_log);

    osm_qos_policy_destroy(p_subn->p_qos_policy);
    p_subn->p_qos_policy = NULL;

    yyin = fopen (p_subn->opt.qos_policy_file, "r");
    if (!yyin)
    {
        if (strcmp(p_subn->opt.qos_policy_file,OSM_DEFAULT_QOS_POLICY_FILE)) {
            OSM_LOG(p_qos_parser_osm_log, OSM_LOG_ERROR, "ERR AC01: "
                    "Failed opening QoS policy file %s - %s\n",
                    p_subn->opt.qos_policy_file, strerror(errno));
            res = 1;
        }
        else
            OSM_LOG(p_qos_parser_osm_log, OSM_LOG_VERBOSE,
                    "QoS policy file not found (%s)\n",
                    p_subn->opt.qos_policy_file);

        goto Exit;
    }

    if (first_time)
    {
        first_time = FALSE;
        __setup_simple_qos_levels();
        __setup_ulp_match_rules();
        OSM_LOG(p_qos_parser_osm_log, OSM_LOG_INFO,
		"Loading QoS policy file (%s)\n",
                p_subn->opt.qos_policy_file);
    }
    else
        /*
         * ULP match rules list was emptied at the end of
         * previous parsing iteration.
         * What's left is to clear simple QoS levels.
         */
        __clear_simple_qos_levels();

    column_num = 1;
    line_num = 1;

    p_subn->p_qos_policy = osm_qos_policy_create(p_subn);

    __parser_tmp_struct_init();
    p_qos_policy = p_subn->p_qos_policy;

    res = yyparse();

    __parser_tmp_struct_destroy();

    if (res != 0)
    {
        OSM_LOG(p_qos_parser_osm_log, OSM_LOG_ERROR, "ERR AC03: "
                "Failed parsing QoS policy file (%s)\n",
                p_subn->opt.qos_policy_file);
        osm_qos_policy_destroy(p_subn->p_qos_policy);
        p_subn->p_qos_policy = NULL;
        res = 1;
        goto Exit;
    }

    /* add generated ULP match rules to the usual match rules */
    __process_ulp_match_rules();

    if (osm_qos_policy_validate(p_subn->p_qos_policy,p_qos_parser_osm_log))
    {
        OSM_LOG(p_qos_parser_osm_log, OSM_LOG_ERROR, "ERR AC04: "
                "Error(s) in QoS policy file (%s)\n",
                p_subn->opt.qos_policy_file);
        fprintf(stderr, "Error(s) in QoS policy file (%s)\n",
                p_subn->opt.qos_policy_file);
        osm_qos_policy_destroy(p_subn->p_qos_policy);
        p_subn->p_qos_policy = NULL;
        res = 1;
        goto Exit;
    }

  Exit:
    if (yyin)
    {
        yyrestart(yyin);
        fclose(yyin);
    }
    OSM_LOG_EXIT(p_qos_parser_osm_log);
    return res;
}

/***************************************************
 ***************************************************/

int yywrap()
{
    return(1);
}

/***************************************************
 ***************************************************/

static void yyerror(const char *format, ...)
{
    char s[256];
    va_list pvar;

    OSM_LOG_ENTER(p_qos_parser_osm_log);

    va_start(pvar, format);
    vsnprintf(s, sizeof(s), format, pvar);
    va_end(pvar);

    OSM_LOG(p_qos_parser_osm_log, OSM_LOG_ERROR, "ERR AC05: "
            "Syntax error (line %d:%d): %s\n",
            line_num, column_num, s);
    fprintf(stderr, "Error in QoS Policy File (line %d:%d): %s.\n",
            line_num, column_num, s);
    OSM_LOG_EXIT(p_qos_parser_osm_log);
}

/***************************************************
 ***************************************************/

static char * __parser_strip_white(char * str)
{
	char *p;

	while (isspace(*str))
		str++;
	if (!*str)
		return str;
	p = str + strlen(str) - 1;
	while (isspace(*p))
		*p-- = '\0';

	return str;
}

/***************************************************
 ***************************************************/

static void __parser_str2uint64(uint64_t * p_val, char * str)
{
   *p_val = strtoull(str, NULL, 0);
}

/***************************************************
 ***************************************************/

static void __parser_port_group_start()
{
    p_current_port_group = osm_qos_policy_port_group_create();
}

/***************************************************
 ***************************************************/

static int __parser_port_group_end()
{
    if(!p_current_port_group->name)
    {
        yyerror("port-group validation failed - no port group name specified");
        return -1;
    }

    cl_list_insert_tail(&p_qos_policy->port_groups,
                        p_current_port_group);
    p_current_port_group = NULL;
    return 0;
}

/***************************************************
 ***************************************************/

static void __parser_vlarb_scope_start()
{
    p_current_vlarb_scope = osm_qos_policy_vlarb_scope_create();
}

/***************************************************
 ***************************************************/

static int __parser_vlarb_scope_end()
{
    if ( !cl_list_count(&p_current_vlarb_scope->group_list) &&
         !cl_list_count(&p_current_vlarb_scope->across_list) )
    {
        yyerror("vlarb-scope validation failed - no port groups specified by 'group' or by 'across'");
        return -1;
    }

    cl_list_insert_tail(&p_qos_policy->vlarb_tables,
                        p_current_vlarb_scope);
    p_current_vlarb_scope = NULL;
    return 0;
}

/***************************************************
 ***************************************************/

static void __parser_sl2vl_scope_start()
{
    p_current_sl2vl_scope = osm_qos_policy_sl2vl_scope_create();
}

/***************************************************
 ***************************************************/

static int __parser_sl2vl_scope_end()
{
    if (!p_current_sl2vl_scope->sl2vl_table_set)
    {
        yyerror("sl2vl-scope validation failed - no sl2vl table specified");
        return -1;
    }
    if ( !cl_list_count(&p_current_sl2vl_scope->group_list) &&
         !cl_list_count(&p_current_sl2vl_scope->across_to_list) &&
         !cl_list_count(&p_current_sl2vl_scope->across_from_list) )
    {
        yyerror("sl2vl-scope validation failed - no port groups specified by 'group', 'across-to' or 'across-from'");
        return -1;
    }

    cl_list_insert_tail(&p_qos_policy->sl2vl_tables,
                        p_current_sl2vl_scope);
    p_current_sl2vl_scope = NULL;
    return 0;
}

/***************************************************
 ***************************************************/

static void __parser_qos_level_start()
{
    p_current_qos_level = osm_qos_policy_qos_level_create();
}

/***************************************************
 ***************************************************/

static int __parser_qos_level_end()
{
    if (!p_current_qos_level->sl_set)
    {
        yyerror("qos-level validation failed - no 'sl' specified");
        return -1;
    }
    if (!p_current_qos_level->name)
    {
        yyerror("qos-level validation failed - no 'name' specified");
        return -1;
    }

    cl_list_insert_tail(&p_qos_policy->qos_levels,
                        p_current_qos_level);
    p_current_qos_level = NULL;
    return 0;
}

/***************************************************
 ***************************************************/

static void __parser_match_rule_start()
{
    p_current_qos_match_rule = osm_qos_policy_match_rule_create();
}

/***************************************************
 ***************************************************/

static int __parser_match_rule_end()
{
    if (!p_current_qos_match_rule->qos_level_name)
    {
        yyerror("match-rule validation failed - no 'qos-level-name' specified");
        return -1;
    }

    cl_list_insert_tail(&p_qos_policy->qos_match_rules,
                        p_current_qos_match_rule);
    p_current_qos_match_rule = NULL;
    return 0;
}

/***************************************************
 ***************************************************/

static void __parser_ulp_match_rule_start()
{
    p_current_qos_match_rule = osm_qos_policy_match_rule_create();
}

/***************************************************
 ***************************************************/

static int __parser_ulp_match_rule_end()
{
    CL_ASSERT(p_current_qos_match_rule->p_qos_level);
    cl_list_insert_tail(&__ulp_match_rules,
                        p_current_qos_match_rule);
    p_current_qos_match_rule = NULL;
    return 0;
}

/***************************************************
 ***************************************************/

static void __parser_tmp_struct_init()
{
    tmp_parser_struct.str[0] = '\0';
    cl_list_construct(&tmp_parser_struct.str_list);
    cl_list_init(&tmp_parser_struct.str_list, 10);
    cl_list_construct(&tmp_parser_struct.num_list);
    cl_list_init(&tmp_parser_struct.num_list, 10);
    cl_list_construct(&tmp_parser_struct.num_pair_list);
    cl_list_init(&tmp_parser_struct.num_pair_list, 10);
}

/***************************************************
 ***************************************************/

/*
 * Do NOT free objects from the temp struct.
 * Either they are inserted into the parse tree data
 * structure, or they are already freed when copying
 * their values to the parse tree data structure.
 */
static void __parser_tmp_struct_reset()
{
    tmp_parser_struct.str[0] = '\0';
    cl_list_remove_all(&tmp_parser_struct.str_list);
    cl_list_remove_all(&tmp_parser_struct.num_list);
    cl_list_remove_all(&tmp_parser_struct.num_pair_list);
}

/***************************************************
 ***************************************************/

static void __parser_tmp_struct_destroy()
{
    __parser_tmp_struct_reset();
    cl_list_destroy(&tmp_parser_struct.str_list);
    cl_list_destroy(&tmp_parser_struct.num_list);
    cl_list_destroy(&tmp_parser_struct.num_pair_list);
}

/***************************************************
 ***************************************************/

#define __SIMPLE_QOS_LEVEL_NAME "SimpleQoSLevel_SL"
#define __SIMPLE_QOS_LEVEL_DEFAULT_NAME "SimpleQoSLevel_DEFAULT"

static void __setup_simple_qos_levels()
{
    uint8_t i;
    char tmp_buf[30];
    memset(osm_qos_policy_simple_qos_levels, 0,
           sizeof(osm_qos_policy_simple_qos_levels));
    for (i = 0; i < 16; i++)
    {
        osm_qos_policy_simple_qos_levels[i].sl = i;
        osm_qos_policy_simple_qos_levels[i].sl_set = TRUE;
        sprintf(tmp_buf, "%s%u", __SIMPLE_QOS_LEVEL_NAME, i);
        osm_qos_policy_simple_qos_levels[i].name = strdup(tmp_buf);
    }

    memset(&__default_simple_qos_level, 0,
           sizeof(__default_simple_qos_level));
    __default_simple_qos_level.name =
           strdup(__SIMPLE_QOS_LEVEL_DEFAULT_NAME);
}

/***************************************************
 ***************************************************/

static void __clear_simple_qos_levels()
{
    /*
     * Simple QoS levels are static.
     * What's left is to invalidate default simple QoS level.
     */
    __default_simple_qos_level.sl_set = FALSE;
}

/***************************************************
 ***************************************************/

static void __setup_ulp_match_rules()
{
    cl_list_construct(&__ulp_match_rules);
    cl_list_init(&__ulp_match_rules, 10);
}

/***************************************************
 ***************************************************/

static void __process_ulp_match_rules()
{
    cl_list_iterator_t list_iterator;
    osm_qos_match_rule_t *p_qos_match_rule = NULL;

    list_iterator = cl_list_head(&__ulp_match_rules);
    while (list_iterator != cl_list_end(&__ulp_match_rules))
    {
        p_qos_match_rule = (osm_qos_match_rule_t *) cl_list_obj(list_iterator);
        if (p_qos_match_rule)
            cl_list_insert_tail(&p_qos_policy->qos_match_rules,
                                p_qos_match_rule);
        list_iterator = cl_list_next(list_iterator);
    }
    cl_list_remove_all(&__ulp_match_rules);
}

/***************************************************
 ***************************************************/

static int __cmp_num_range(const void * p1, const void * p2)
{
    uint64_t * pair1 = *((uint64_t **)p1);
    uint64_t * pair2 = *((uint64_t **)p2);

    if (pair1[0] < pair2[0])
        return -1;
    if (pair1[0] > pair2[0])
        return 1;

    if (pair1[1] < pair2[1])
        return -1;
    if (pair1[1] > pair2[1])
        return 1;

    return 0;
}

/***************************************************
 ***************************************************/

static void __sort_reduce_rangearr(
    uint64_t  **   arr,
    unsigned       arr_len,
    uint64_t  ** * p_res_arr,
    unsigned     * p_res_arr_len )
{
    unsigned i = 0;
    unsigned j = 0;
    unsigned last_valid_ind = 0;
    unsigned valid_cnt = 0;
    uint64_t ** res_arr;
    boolean_t * is_valid_arr;

    *p_res_arr = NULL;
    *p_res_arr_len = 0;

    qsort(arr, arr_len, sizeof(uint64_t*), __cmp_num_range);

    is_valid_arr = (boolean_t *)malloc(arr_len * sizeof(boolean_t));
    is_valid_arr[last_valid_ind] = TRUE;
    valid_cnt++;
    for (i = 1; i < arr_len; i++)
    {
        if (arr[i][0] <= arr[last_valid_ind][1])
        {
            if (arr[i][1] > arr[last_valid_ind][1])
                arr[last_valid_ind][1] = arr[i][1];
            free(arr[i]);
            arr[i] = NULL;
            is_valid_arr[i] = FALSE;
        }
        else if ((arr[i][0] - 1) == arr[last_valid_ind][1])
        {
            arr[last_valid_ind][1] = arr[i][1];
            free(arr[i]);
            arr[i] = NULL;
            is_valid_arr[i] = FALSE;
        }
        else
        {
            is_valid_arr[i] = TRUE;
            last_valid_ind = i;
            valid_cnt++;
        }
    }

    res_arr = (uint64_t **)malloc(valid_cnt * sizeof(uint64_t *));
    for (i = 0; i < arr_len; i++)
    {
        if (is_valid_arr[i])
            res_arr[j++] = arr[i];
    }
    free(is_valid_arr);
    free(arr);

    *p_res_arr = res_arr;
    *p_res_arr_len = valid_cnt;
}

/***************************************************
 ***************************************************/

static void __pkey_rangelist2rangearr(
    cl_list_t    * p_list,
    uint64_t  ** * p_arr,
    unsigned     * p_arr_len)
{
    uint64_t   tmp_pkey;
    uint64_t * p_pkeys;
    cl_list_iterator_t list_iterator;

    list_iterator= cl_list_head(p_list);
    while( list_iterator != cl_list_end(p_list) )
    {
       p_pkeys = (uint64_t *)cl_list_obj(list_iterator);
       p_pkeys[0] &= 0x7fff;
       p_pkeys[1] &= 0x7fff;
       if (p_pkeys[0] > p_pkeys[1])
       {
           tmp_pkey = p_pkeys[1];
           p_pkeys[1] = p_pkeys[0];
           p_pkeys[0] = tmp_pkey;
       }
       list_iterator = cl_list_next(list_iterator);
    }

    __rangelist2rangearr(p_list, p_arr, p_arr_len);
}

/***************************************************
 ***************************************************/

static void __rangelist2rangearr(
    cl_list_t    * p_list,
    uint64_t  ** * p_arr,
    unsigned     * p_arr_len)
{
    cl_list_iterator_t list_iterator;
    unsigned len = cl_list_count(p_list);
    unsigned i = 0;
    uint64_t ** tmp_arr;
    uint64_t ** res_arr = NULL;
    unsigned res_arr_len = 0;

    tmp_arr = (uint64_t **)malloc(len * sizeof(uint64_t *));

    list_iterator = cl_list_head(p_list);
    while( list_iterator != cl_list_end(p_list) )
    {
       tmp_arr[i++] = (uint64_t *)cl_list_obj(list_iterator);
       list_iterator = cl_list_next(list_iterator);
    }
    cl_list_remove_all(p_list);

    __sort_reduce_rangearr( tmp_arr,
                            len,
                            &res_arr,
                            &res_arr_len );
    *p_arr = res_arr;
    *p_arr_len = res_arr_len;
}

/***************************************************
 ***************************************************/

static void __merge_rangearr(
    uint64_t  **   range_arr_1,
    unsigned       range_len_1,
    uint64_t  **   range_arr_2,
    unsigned       range_len_2,
    uint64_t  ** * p_arr,
    unsigned     * p_arr_len )
{
    unsigned i = 0;
    unsigned j = 0;
    unsigned len = range_len_1 + range_len_2;
    uint64_t ** tmp_arr;
    uint64_t ** res_arr = NULL;
    unsigned res_arr_len = 0;

    *p_arr = NULL;
    *p_arr_len = 0;

    tmp_arr = (uint64_t **)malloc(len * sizeof(uint64_t *));

    for (i = 0; i < range_len_1; i++)
       tmp_arr[j++] = range_arr_1[i];
    for (i = 0; i < range_len_2; i++)
       tmp_arr[j++] = range_arr_2[i];
    free(range_arr_1);
    free(range_arr_2);

    __sort_reduce_rangearr( tmp_arr,
                            len,
                            &res_arr,
                            &res_arr_len );
    *p_arr = res_arr;
    *p_arr_len = res_arr_len;
}

/***************************************************
 ***************************************************/

static void __parser_add_port_to_port_map(
    cl_qmap_t   * p_map,
    osm_physp_t * p_physp)
{
    if (cl_qmap_get(p_map, cl_ntoh64(osm_physp_get_port_guid(p_physp))) ==
        cl_qmap_end(p_map))
    {
        osm_qos_port_t * p_port = osm_qos_policy_port_create(p_physp);
        if (p_port)
            cl_qmap_insert(p_map,
                           cl_ntoh64(osm_physp_get_port_guid(p_physp)),
                           &p_port->map_item);
    }
}

/***************************************************
 ***************************************************/

static void __parser_add_guid_range_to_port_map(
    cl_qmap_t  * p_map,
    uint64_t  ** range_arr,
    unsigned     range_len)
{
    unsigned i;
    uint64_t guid_ho;
    osm_port_t * p_osm_port;

    if (!range_arr || !range_len)
        return;

    for (i = 0; i < range_len; i++) {
         for (guid_ho = range_arr[i][0]; guid_ho <= range_arr[i][1]; guid_ho++) {
             p_osm_port =
                osm_get_port_by_guid(p_qos_policy->p_subn, cl_hton64(guid_ho));
             if (p_osm_port)
                 __parser_add_port_to_port_map(p_map, p_osm_port->p_physp);
         }
         free(range_arr[i]);
    }
    free(range_arr);
}

/***************************************************
 ***************************************************/

static void __parser_add_pkey_range_to_port_map(
    cl_qmap_t  * p_map,
    uint64_t  ** range_arr,
    unsigned     range_len)
{
    unsigned i;
    uint64_t pkey_64;
    ib_net16_t pkey;
    osm_prtn_t * p_prtn;

    if (!range_arr || !range_len)
        return;

    for (i = 0; i < range_len; i++) {
         for (pkey_64 = range_arr[i][0]; pkey_64 <= range_arr[i][1]; pkey_64++) {
             pkey = cl_hton16((uint16_t)(pkey_64 & 0x7fff));
             p_prtn = (osm_prtn_t *)
                 cl_qmap_get(&p_qos_policy->p_subn->prtn_pkey_tbl, pkey);
             if (p_prtn != (osm_prtn_t *)cl_qmap_end(
                   &p_qos_policy->p_subn->prtn_pkey_tbl)) {
                 __parser_add_map_to_port_map(p_map, &p_prtn->part_guid_tbl);
                 __parser_add_map_to_port_map(p_map, &p_prtn->full_guid_tbl);
             }
         }
         free(range_arr[i]);
    }
    free(range_arr);
}

/***************************************************
 ***************************************************/

static void __parser_add_partition_list_to_port_map(
    cl_qmap_t  * p_map,
    cl_list_t  * p_list)
{
    cl_list_iterator_t    list_iterator;
    char                * tmp_str;
    osm_prtn_t          * p_prtn;

    /* extract all the ports from the partition
       to the port map of this port group */
    list_iterator = cl_list_head(p_list);
    while(list_iterator != cl_list_end(p_list)) {
        tmp_str = (char*)cl_list_obj(list_iterator);
        if (tmp_str) {
            p_prtn = osm_prtn_find_by_name(p_qos_policy->p_subn, tmp_str);
            if (p_prtn) {
                __parser_add_map_to_port_map(p_map, &p_prtn->part_guid_tbl);
                __parser_add_map_to_port_map(p_map, &p_prtn->full_guid_tbl);
            }
            free(tmp_str);
        }
        list_iterator = cl_list_next(list_iterator);
    }
    cl_list_remove_all(p_list);
}

/***************************************************
 ***************************************************/

static void __parser_add_map_to_port_map(
    cl_qmap_t * p_dmap,
    cl_map_t  * p_smap)
{
    cl_map_iterator_t map_iterator;
    osm_physp_t * p_physp;

    if (!p_dmap || !p_smap)
        return;

    map_iterator = cl_map_head(p_smap);
    while (map_iterator != cl_map_end(p_smap)) {
        p_physp = (osm_physp_t*)cl_map_obj(map_iterator);
        __parser_add_port_to_port_map(p_dmap, p_physp);
        map_iterator = cl_map_next(map_iterator);
    }
}

/***************************************************
 ***************************************************/

static int __validate_pkeys( uint64_t ** range_arr,
                             unsigned    range_len,
                             boolean_t   is_ipoib)
{
    unsigned i;
    uint64_t pkey_64;
    ib_net16_t pkey;
    osm_prtn_t * p_prtn;

    if (!range_arr || !range_len)
        return 0;

    for (i = 0; i < range_len; i++) {
        for (pkey_64 = range_arr[i][0]; pkey_64 <= range_arr[i][1]; pkey_64++) {
            pkey = cl_hton16((uint16_t)(pkey_64 & 0x7fff));
            p_prtn = (osm_prtn_t *)
                cl_qmap_get(&p_qos_policy->p_subn->prtn_pkey_tbl, pkey);

            if (p_prtn == (osm_prtn_t *)cl_qmap_end(
                  &p_qos_policy->p_subn->prtn_pkey_tbl))
                p_prtn = NULL;

            if (is_ipoib) {
                /*
                 * Be very strict for IPoIB partition:
                 *  - the partition for the pkey have to exist
                 *  - it has to have at least 2 full members
                 */
                if (!p_prtn) {
                    yyerror("IPoIB partition, pkey 0x%04X - "
                                       "partition doesn't exist",
                                       cl_ntoh16(pkey));
                    return 1;
                }
                else if (cl_map_count(&p_prtn->full_guid_tbl) < 2) {
                    yyerror("IPoIB partition, pkey 0x%04X - "
                                       "partition has less than two full members",
                                       cl_ntoh16(pkey));
                    return 1;
                }
            }
            else if (!p_prtn) {
                /*
                 * For non-IPoIB pkey we just want to check that
                 * the relevant partition exists.
                 * And even if it doesn't, don't exit - just print
                 * error message and continue.
                 */
                 OSM_LOG(p_qos_parser_osm_log, OSM_LOG_ERROR, "ERR AC02: "
			 "pkey 0x%04X - partition doesn't exist",
                         cl_ntoh16(pkey));
            }
        }
    }
    return 0;
}

/***************************************************
 ***************************************************/

