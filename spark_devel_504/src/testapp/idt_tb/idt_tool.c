#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <time.h>

#include <zlog/zlog.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "fica_opt.h"
#include "idt_tool.h"

int g_idtfd = -1;
const port_def_t port_def_tbl[IDT_MAX_DEV_ID] = {
    {NULL, 0},           // port 0
    {"PORT1-PPCS0",  1}, // port 1
    {"PORT2-P1EXT2", 0}, // port 2
    {"PORT3-P1EXT1", 0}, // port 3
    {"PORT4-PPCS1",  1}, // port 4
    {"PORT5-P1EXT3", 0}, // port 5
    {"PORT6-P1EXT0", 0}, // port 6
};

void format_number(uint32_t number, char* outbuf)
{
    char numstr[1024];
    char tempstr[1024];

    sprintf(numstr, "%u", number);
    outbuf[0] = '\0';

    while(strlen(numstr) > 0) {
        int cpylen = MIN(strlen(numstr), 3);
        sprintf(tempstr, "%s%s", numstr + (strlen(numstr) - cpylen), outbuf);
        strcpy(outbuf, tempstr);
        numstr[strlen(numstr) - cpylen] = '\0';
        if (strlen(numstr) > 0)
            sprintf(tempstr, ",%s", outbuf);
        strcpy(outbuf, tempstr);
    }
}

// printf/thread not safe
char* __format_hex(uint32_t value)
{
    static char __fmt[128];
    int ptr = 0;

    sprintf(__fmt, "0x%08x ", value);
    ptr += strlen(__fmt);
    for (int i = 0; i < 32; i++) {
        if (!(i % 8))
            __fmt[ptr++] = '|';
        __fmt[ptr++] = (value & DSBIT(i)) ? '1' : '.';
        if (!((i + 5) % 8))
            __fmt[ptr++] = ' ';
    }
    __fmt[ptr++] = '|';

    return(__fmt);
}

char* __format_counter(uint32_t value, uint32_t span)
{
    static char __fmt[128];
    char tempbuf[1024];

    format_number(value, tempbuf);
    sprintf(__fmt, "0x%08x (%s)", value, tempbuf);
    return(__fmt);
}

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

/* Read a string, and return a pointer to it.
   Returns NULL on EOF. */
char *rl_gets(const char* prompt)
{
    /* If the buffer has already been allocated,
       return the memory to the free pool. */
    if (line_read) {
        free (line_read);
        line_read = (char *)NULL;
    }

    /* Get a line from the user. */
    line_read = readline (prompt);

    /* If the line has any text in it,
       save it on the history. */
    if (line_read && *line_read)
        add_history (line_read);

    return (line_read);
}

char* utl_strcmp(const char* cmd, char* input)
{
    char cmd_short[1024];
    int i, cnt;

    if (!strcasecmp(cmd, input))
        return NULL;

    memset(cmd_short, 0, sizeof(cmd_short));
    cnt = 0;
    for (i=0; i<strlen(cmd); i++) {
        if (cmd[i] >= 'A' && cmd[i] <= 'Z') {
            cmd_short[cnt++] = cmd[i];
        }
    }
    if (strlen(cmd_short) > 0 && !strcasecmp(cmd_short, input))
        return NULL;

    return(input);
}

int print_cmd_table(const cmd_decl_t* cmd_tbl)
{
    while(cmd_tbl->cmd) {
        printf("\t%-12s- %s\n",
               cmd_tbl->cmd,
               cmd_tbl->help);
        cmd_tbl++;
    }
    return(0);
}

PORT_BMP parse_port_bmp(char* str)
{
    return(strtoul(str, NULL, 0));
}

int idt_i2c_bus = 2;
int idt_i2c_addr = 0x67;
char* log_conf = NULL;
char* script_file = NULL;

static void parse_args(int argc, char** argv)
{
    int parse_argc = 1;
    while(parse_argc < argc) {
        char* key = NULL;
        char* value = NULL;

        if(fica_shift_option(argv[parse_argc], &key, &value) && key) {
            if (!strcmp(key, "--i2c-bus") || !strcmp(key, "-b")) {
                idt_i2c_bus = strtoul(value, NULL, 0);
            } else if (!strcmp(key, "--i2c-addr") || !strcmp(key, "-a")) {
                idt_i2c_addr = strtoul(value, NULL, 0);
            } else if (!strcmp(key, "--log-conf") || !strcmp(key, "-l")) {
                log_conf = value;
            } else if (!strcmp(key, "--script-file") || !strcmp(key, "-f")) {
                script_file = value;
            } else if (!strcmp(key, "--version") || !strcmp(key, "-v")) {
                printf("  version : %s\n", IDT_TOOL_VERSION);
                exit(0);
            } else {
                fprintf(stderr, "WARN: Unknown option '%s'\n", key);
                exit(1);
            }
        }
        parse_argc++;
    }
}

int main(int main_argc, char* main_argv[])
{
    int ret = -1;
    int     argc;
    char*   argv[128];
    int     i;

    parse_args(main_argc, main_argv);

    if (log_conf) {
        zlog_init(log_conf);
    }
    ret = idt_module_init(NULL);

    printf("Open device: bus=%d, addr=0x%x\n", idt_i2c_bus, idt_i2c_addr);
    g_idtfd = idt_dev_open(idt_i2c_bus, idt_i2c_addr);
    if (g_idtfd <= 0) {
        fprintf(stderr, "FATAL: failed to open IDT device, quit!\n");
        exit(1);
    }

    idt_dev_info_t info;
    idt_dev_get_info(g_idtfd, &info);
    printf("  vendor_id : 0x%04x\n", info.vendor_id);
    printf("  device_id : 0x%04x\n", info.dev_id);
    printf("  revision  : %d.%c\n", info.major, info.minor + 'A');
    if (info.vendor_id == 0xffff) {
        fprintf(stderr, "FATAL: failed to open IDT device, quit!\n");
        exit(1);
    }

    FILE* fd_script = NULL;
    if (script_file) {
        fd_script = fopen(script_file, "r");
        if (!fd_script) {
            fprintf(stderr, "WARN: failed to open script file, run in CLI mode.\n");
        }
    }

    while(1) {
        char* cmd = NULL;
        char cmd_buffer[1024];

        if (!fd_script) {
            cmd = rl_gets("idt> ");
        } else {
            cmd = fgets(cmd_buffer, 1024, fd_script);
        }
        if (!cmd)
            break;

        int found = 0;
        argc = 0;

        // trim lf/cr
        if (cmd[strlen(cmd)-1] == '\n')
            cmd[strlen(cmd)-1] = '\0';

        if (strlen(cmd) <= 0 || cmd[0] == '#')
            continue;

        // parse cmd
        while((argv[argc]=strtok(cmd, " ")) != NULL) {
            argc++;
            cmd = NULL;
        }
        argv[argc] = NULL;

        // exec cmd
        if (argc > 0) {
            for (i=0; cmd_root_tbl[i].cmd; i++) {
                if (!utl_strcmp(cmd_root_tbl[i].cmd, argv[0])) {
                    found = 1;
                    ret = (*cmd_root_tbl[i].cli_func)(argc-1, argv+1);
                    printf(".return = %d\n", ret);
                    break;
                }
            }
        }
        if (!found) {
            print_cmd_table(cmd_root_tbl);
        }
    }

    idt_dev_close(g_idtfd);
    if (fd_script) {
        fclose(fd_script);
        fd_script = NULL;
    }

    return(ret);
}