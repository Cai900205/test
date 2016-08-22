#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

#include "spark.h"
#include "idt/idt_intf.h"

#define IDT_TOOL_VERSION    "1.1"

typedef struct
{
    const char* cmd;
    int (*cli_func)(int argc, char* argv[]);
    const char* help;
} cmd_decl_t;

typedef struct
{
    const char* port_name;
    int lp_is_ppc;
} port_def_t;

typedef uint8_t PORT_BMP;
extern const cmd_decl_t cmd_root_tbl[];
extern const port_def_t port_def_tbl[IDT_MAX_DEV_ID];
extern PORT_BMP port_cur_bmp;

#define PORTBMP_ALL         (0x7e)  // port 1-6
#define PORTBMP_NONE        (0)
#define PORT_EXISTS(PBMP, P)    ((PBMP & (CPUBIT(P))) & PORTBMP_ALL)

#define __DUMP_REG_COUNTER(REGNAME) \
    value = idt_reg_r32(g_idtfd, REGNAME(pid)); \
    printf("    "#REGNAME"%*s : %s\n", (int)(35-strlen(#REGNAME)), " ", __format_counter(value, 0));

#define __DUMP_REG_HEX(REGNAME) \
    value = idt_reg_r32(g_idtfd, REGNAME(pid)); \
    printf("    "#REGNAME"%*s : %s\n", (int)(35-strlen(#REGNAME)), " ", __format_hex(value));

#define __DUMP_REG_BIT(VALUE, NAME, BIT, COUNT) \
    { \
        uint32_t bitvalue = VALUE >> DS2CPU(BIT+COUNT-1); \
        bitvalue &= (0x1 << (COUNT))-1; \
        printf("      "#NAME"%*s : 0x%x\n", (int)(18-strlen(#NAME)), " ", bitvalue); \
    }

#define __PARSE_ARG_PBMP(OUT, ARGC, ARGV) \
    if (ARGC <= 0) { \
        OUT = port_cur_bmp; \
    } else { \
        OUT = parse_port_bmp(ARGV[0]); \
    } \

#define PARSE_ARG_INT(RET) \
    RET = strtoul(argv[0], NULL, 0); argc--; argv++;

extern int g_idtfd;
extern char PORT_NAME[0xff][16];

extern void format_number(uint32_t number, char* outbuf);
extern char* __format_hex(uint32_t value);
extern char* __format_counter(uint32_t value, uint32_t span);
extern char* utl_strcmp(const char* cmd, char* input);

extern int print_cmd_table(const cmd_decl_t* cmd_tbl);
extern PORT_BMP parse_port_bmp(char* str);

extern int cli_port(int argc, char* argv[]);
extern int cli_rtbl(int argc, char* argv[]);
extern int cli_reg(int argc, char* argv[]);
extern int cli_ccsr(int argc, char* argv[]);
