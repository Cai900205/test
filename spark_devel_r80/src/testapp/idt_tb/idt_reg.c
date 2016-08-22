#include "idt_tool.h"

static int reg_read32(int argc, char* argv[])
{
    uint32_t reg;
    int count = 1;
    
    if (argc < 1) {
        goto out;
    }
    PARSE_ARG_INT(reg);
    if (argc > 0) {
        PARSE_ARG_INT(count);
    }
    
    if (reg & 0x03 || reg + count*sizeof(uint32_t) >= 0x01000000) {
        goto out;
    }

    for(; count; count--,reg-=sizeof(uint32_t)) {
        uint32_t value = idt_reg_r32(g_idtfd, reg);
        printf("    [IDT:0x%06x] %s\n", reg, __format_hex(value));
    }
    return(0);
out:
    return(-1);
}

static int reg_write32(int argc, char* argv[])
{
    uint32_t reg;
    uint32_t value;
    
    if (argc < 2) {
        goto out;
    }
    PARSE_ARG_INT(reg);
    PARSE_ARG_INT(value);
    
    if (reg & 0x03 || reg >= 0x01000000) {
        goto out;
    }

    idt_reg_w32(g_idtfd, reg, value);

    return(0);
out:
    return(-1);
}

const cmd_decl_t cmd_reg_tbl[] =
{
    {"Read",   reg_read32,    "Read register | Param: <REG> [COUNT]"},
    {"Write",  reg_write32,   "Write register | Param: <REG> <VALUE>"},
    {NULL},
};

int cli_reg(int argc, char* argv[])
{
    int ret = -1;
    int found = 0;

    const cmd_decl_t* cmd_tbl = cmd_reg_tbl;
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

static int popen_exec(char* cmd, char* buf, int* buf_sz)
{
    FILE* fd = NULL;
    int ret = -1;
    
    fd = popen(cmd, "r");
    if (fd) {
        ssize_t read_sz = 0;
        if (buf && buf_sz && *buf_sz > 0) {
            read_sz = fread(buf, sizeof(char), *buf_sz, fd);
            if (read_sz >= 0) {
                *buf_sz = read_sz;
                ret = 0;
            }
        }
        ret = 0;
        fclose(fd);
    }

    return(ret);
}

const char* sysfs_ccsr = "/sys/fvppc/ccsr";
int __ccsr_read32(uint32_t reg, uint32_t* value)
{
    char cmd[1024];
    char buf[1024];
    int buf_sz = 1024;
    int ret = -1;
    uint32_t reg_off;
    
    sprintf(cmd, "echo 0x%08x > %s/reg ; cat %s/data", reg, sysfs_ccsr, sysfs_ccsr);
    buf[0] = '\0';
    ret = popen_exec(cmd, buf, &buf_sz);
    if (!ret) {
        if (buf_sz > 1) {
            ret = sscanf(buf, "[0x%x] 0x%08x", &reg_off, value);
            if (ret > 0) {
                ret = 0;
            }
        }
    }
    
    return(ret);
}

static int ccsr_read32(int argc, char* argv[])
{
    uint32_t reg;
    int ret = -1;
    
    if (argc < 1) {
        goto out;
    }
    PARSE_ARG_INT(reg);
    
    if (reg & 0x03 || reg >= 0x01000000) {
        goto out;
    }
    
    uint32_t value;
    ret = __ccsr_read32(reg, &value);
    if (!ret) {
        printf("    [CCSR:0x%06x] %s\n", reg, __format_hex(value));
    }

    return(ret);
out:
    return(-1);
}

int __ccsr_write32(uint32_t reg, uint32_t value)
{
    char cmd[1024];
    
    sprintf(cmd, "echo 0x%08x > %s/reg ; echo 0x%08x > %s/data", reg, sysfs_ccsr, value, sysfs_ccsr);
    return(popen_exec(cmd, NULL, NULL));
}

static int ccsr_write32(int argc, char* argv[])
{
    uint32_t reg;
    uint32_t value;
    
    if (argc < 2) {
        goto out;
    }
    PARSE_ARG_INT(reg);
    PARSE_ARG_INT(value);
    
    if (reg & 0x03 || reg >= 0x01000000) {
        goto out;
    }

    return(__ccsr_write32(reg, value));
out:
    return(-1);
}

#if 0
int port_init(int argc, char* argv[]);
static int ccsr_test(int argc, char* argv[])
{
    uint32_t value;
__ccsr_write32(0x131000, 0x002f0020);
__ccsr_write32(0x131008, 0x0010fe15);
usleep(1);
__ccsr_write32(0x131008, 0x0030fe15);

    int pid = 1;
        printf("  initializing <%s>\n", PORT_NAME[pid]);

        // ref. CPS-1432 datasheet 201462(p.192)
        // 9.2.3 Register Initialization

        // Reset to default value
        idt_reg_w32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid), 0xD0400001);
        idt_reg_w32(g_idtfd, IDTR_PORTn_IMPL_SPEC_ERR_DET(pid), 0x0);

        // 1. Disable port
#define CTL_1_CSR__PORT_DIS         DSBIT(8)
#define CTL_1_CSR__PORT_LOCKOUT     DSBIT(30)
        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
                     CTL_1_CSR__PORT_DIS | CTL_1_CSR__PORT_LOCKOUT,
                     CTL_1_CSR__PORT_DIS | CTL_1_CSR__PORT_LOCKOUT);
        // 2. Change the lane to port mapping (SKIP: H/W does it for us)

        // 3. Change the lane to port speed setting (TBD: to be config)

        int port_sel = (0x01 << pid);

        // 3c. Reset the PLLs
        idt_reg_w32(g_idtfd, IDTR_DEVICE_RESET_CTL, 0x83fc0000 | port_sel);

        // 4a. Program the route
        idt_reg_w32(g_idtfd, IDTR_RTE_DEFAULT_PORT_CSR, IDT_PORT_DISCARD);
        // 4b. Change Port {0..13} Lane Synchronization Register[VMIN] (SKIP: we are Rev.C)
        // 4c. Change Port Link Timeout Control CSR[TIMEOUT]
        idt_reg_w32(g_idtfd, IDTR_PORT_LINK_TO_CTL_CSR, 0x00005500);
        // 4e. Program other configuration settings
#define DEVICE_CTL_1__CUT_THRU_EN   DSBIT(19)
#define DEVICE_CTL_1__PORT_RST_CTL  DSBIT(31)
        idt_reg_wm32(g_idtfd, IDTR_DEVICE_CTL_1,
                    DEVICE_CTL_1__CUT_THRU_EN | DEVICE_CTL_1__PORT_RST_CTL,
                    DEVICE_CTL_1__CUT_THRU_EN | DEVICE_CTL_1__PORT_RST_CTL);
        // 4d. Enable all counters and error management
        idt_port_enable_counter(g_idtfd, pid, 1);

int seq = 0;
    // 2. Set PnCCSR[PD]
seq = 2;
printf("seq=%d\n", seq);
    __ccsr_read32(0xc015c, &value);
    __ccsr_write32(0xc015c, value | DSBIT(8));

    // 3. Set PnPCR[OBDEN]
seq = 3;
printf("seq=%d\n", seq);
    __ccsr_read32(0xd0140, &value);
    __ccsr_write32(0xd0140, value | DSBIT(29));

    // 4. Clear PnPCR[OBDEN]
seq = 4;
printf("seq=%d\n", seq);
    __ccsr_read32(0xd0140, &value);
    __ccsr_write32(0xd0140, value & ~DSBIT(29));
    
    // 5. Config PnCCSR[PWO]
seq = 5;
printf("seq=%d\n", seq);
    __ccsr_read32(0xc015c, &value);
    __ccsr_write32(0xc015c, value | (0x06 << 24));
    
    // 6. Clear PnCCSR[PD]
seq = 6;
printf("seq=%d\n", seq);
    __ccsr_read32(0xc015c, &value);
    __ccsr_write32(0xc015c, value & ~DSBIT(8));

        // 5. Enable port
        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
                     CTL_1_CSR__PORT_DIS | CTL_1_CSR__PORT_LOCKOUT,
                     0);

    // 7. Polling PnESCSR[PO]
seq = 7;
printf("seq=%d\n", seq);
    while(1) {
        __ccsr_read32(0xc0158, &value);
        if (value & DSBIT(30))
            break;
    }

        // 6. Disable IDLE2 functions
        idt_reg_w32(g_idtfd, IDTR_PORTn_ERR_STAT_CSR(pid), 0x80000000);

        // 7. Reset port
        idt_reg_w32(g_idtfd, IDTR_DEVICE_RESET_CTL, 0x80000000 | port_sel);

        // 8. Configure port width (SKIP: No override)
#define CTL_1_CSR__PWIDTH_OVRD_SHIFT    DS2CPU(7)
#define CTL_1_CSR__PWIDTH_OVRD          DSBIT(5) | DSBIT(6) | DSBIT(7)

        // 9. Enable the ports to receive and transmit packets
#define CTL_1_CSR__OUTPUT_PORT_EN   DSBIT(9)
#define CTL_1_CSR__INPUT_PORT_EN    DSBIT(10)
        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
                     CTL_1_CSR__OUTPUT_PORT_EN | CTL_1_CSR__INPUT_PORT_EN | CTL_1_CSR__PWIDTH_OVRD,
                     CTL_1_CSR__OUTPUT_PORT_EN | CTL_1_CSR__INPUT_PORT_EN | (0x6<<CTL_1_CSR__PWIDTH_OVRD_SHIFT));

    // 10. Polling PnESCSR[OES]
seq = 10;
printf("seq=%d\n", seq);
    __ccsr_read32(0xc0158, &value);
    __ccsr_write32(0xc0158, value | DSBIT(15));
    while(1) {
        __ccsr_read32(0xc0158, &value);
        if (!(value & DSBIT(15)))
            break;
    }

    return(0);
}
#endif

const cmd_decl_t cmd_ccsr_tbl[] =
{
    {"Read",   ccsr_read32,   "Read CCSR register | Param: <REG>"},
    {"Write",  ccsr_write32,   "Write CCSR register | Param: <REG> <VALUE>"},
    {NULL},
};

int cli_ccsr(int argc, char* argv[])
{
    int ret = -1;
    int found = 0;

    const cmd_decl_t* cmd_tbl = cmd_ccsr_tbl;
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
