#include "idt_tool.h"

static int rtbl_show(int argc, char* argv[])
{
    PORT_ID pid;
    DEV_ID  did;
    PORT_BMP    pbmp;

    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("  <%s>\n", port_def_tbl[pid].port_name);
        for (did = 0; did < IDT_MAX_DEV_ID; did++) {
            PORT_ID dest_port;
            dest_port = idt_routetbl_get(g_idtfd, pid, did);
            if (dest_port != IDT_PORT_DEFAULT) {
                printf("    did: 0x%02x -> dest: %s\n",
                       did,
                       port_def_tbl[dest_port].port_name);
            }
        }
    }

    return(0);
}

static int rtbl_set(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_ID dport;
    DEV_ID  did;
    PORT_BMP    pbmp;
    int ret = -1;

    if (argc < 2) {
        goto out;
    }
    
    did = strtoul(argv[0], NULL, 0); argc--; argv++;
    dport = strtoul(argv[0], NULL, 0); argc--; argv++;
    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("  <%s>\n", port_def_tbl[pid].port_name);
        ret = idt_routetbl_set(g_idtfd, pid, did, dport);
        if (!ret) {
            printf("    did: 0x%02x -> dest: %s\n",
                   did,
                   port_def_tbl[dport].port_name);
            goto out;
        }
    }

out:
    return(ret);
}

static int rtbl_test(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_ID dport;
    DEV_ID  did;
    PORT_BMP    pbmp;
    int j;
    int ret = -1;

    if (argc < 2) {
        goto out;
    }
    
    did = strtoul(argv[0], NULL, 0); argc--; argv++;
    dport = strtoul(argv[0], NULL, 0); argc--; argv++;
    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("  <%s>\n", port_def_tbl[pid].port_name);
        for (j=did; j<IDT_MAX_DEV_ID; j++) {
            ret = idt_routetbl_set(g_idtfd, pid, j, dport);
            if (!ret) {
                printf("    did: 0x%02x -> dest: %s\n",
                       did,
                       port_def_tbl[dport].port_name);
                goto out;
            }
        }
    }

out:
    return(ret);
}


static int rtbl_reset(int argc, char* argv[])
{
    PORT_ID pid;
    DEV_ID  did;
    PORT_BMP    pbmp;

    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("  <%s> reset routetbl ...\n", port_def_tbl[pid].port_name);
        for (did = 0; did < IDT_MAX_DEV_ID; did++) {
            idt_routetbl_set(g_idtfd, pid, did, IDT_PORT_DEFAULT);
        }
    }

    return(0);
}

const cmd_decl_t cmd_rtbl_tbl[] =
{
    {"SetEntry", rtbl_set,    "Set table entry | Param: <DID> <PORT_ID> [PORT_BMP] "},
    {"Show",     rtbl_show,   "Show table | Param: [PORT_BMP]"},
    {"Reset",    rtbl_reset,  "Reset(clean) table | Param: [PORT_BMP]"},
    {"Test",     rtbl_test,   "Test function | Param: <PORT_ID> [PORT_BMP]"},

    {NULL},
};

int cli_rtbl(int argc, char* argv[])
{
    int ret = -1;
    int found = 0;
    
    const cmd_decl_t* cmd_tbl = cmd_rtbl_tbl;

    if (argc > 0) {
        for (int i=0; cmd_tbl[i].cmd; i++) {
            if (!utl_strcmp(cmd_tbl[i].cmd, argv[0])) {
                found = 1;
                ret = (*cmd_tbl[i].cli_func)(argc-1, argv+1);
                break;
            }
        }
    }

    if (!found || ret == -1) {
        print_cmd_table(cmd_tbl);
    }

    return(ret);
}
