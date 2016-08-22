#include "idt_tool.h"

static int cli_quit(int argc, char* argv[])
{
    printf("  Au revoir!\n\n");
    exit(0);
    return(0);
}

static int cli_ver(int argc, char* argv[])
{
    printf("  version : %s\n", IDT_TOOL_VERSION);
    return(0);
}

const cmd_decl_t cmd_root_tbl[] =
{
    {"CCSR",        cli_ccsr,       "CCSR Register commands"},
    {"REGister",    cli_reg,        "IDT Register commands"},
    {"RouteTaBLe",  cli_rtbl,       "RouteTable commands"},
    {"Port",        cli_port,       "Port commands"},
    {"VERsion",     cli_ver,        "Show version"},
    {"Quit",        cli_quit,       "Quit"},
    {NULL},
};
