#include "idt_tool.h"

PORT_BMP port_cur_bmp = PORTBMP_ALL;

typedef struct linkspd_def {
    const char* speed_desc;
    uint32_t speed;
    uint8_t rate;
    uint8_t pll_div_sel;
} linkspd_def_t;

static linkspd_def_t linkspd_tbl[] = {
    {"1.25G",  1, 0, 0},
    {"3.125G", 3, 1, 1},
    {"5G",     5, 2, 0},
    {"6.25G",  6, 2, 1},
};

static int port_get_index_speed(uint32_t speed)
{
    int ret = -1;
    int i;
    for (i=0; i<sizeof(linkspd_tbl)/sizeof(linkspd_tbl[0]); i++) {
        if (speed == linkspd_tbl[i].speed) {
            ret = i;
            break;
        }
    }

    return(ret);
}

static int port_get_index_conf(int rate, int pll_div_sel)
{
    int ret = -1;
    int i;
    for (i=0; i<sizeof(linkspd_tbl)/sizeof(linkspd_tbl[0]); i++) {
        if (rate == linkspd_tbl[i].rate &&
            pll_div_sel == linkspd_tbl[i].pll_div_sel) {
            ret = i;
            break;
        }
    }

    return(ret);
}

static int port_get_lane_start(int pid)
{
    return(pid * 4);
}

static int port_get_pll(int pid)
{
    return(pid);
}

static int port_show_stats(int argc, char* argv[])
{
    uint32_t value;
    PORT_ID pid;
    PORT_BMP    pbmp;

    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("  <%s>\n", port_def_tbl[pid].port_name);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_PA_TX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_NACK_TX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_RTRY_TX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_PKT_TX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_PA_RX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_NACK_RX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_RTRY_RX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_PKT_RX_CNTR);

        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_CPB_TX_CNTR);

        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_PKT_DROP_RX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_PKT_DROP_TX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_CPB_TX_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_VC0_CRC_LIMIT_DROP_CNTR);
        __DUMP_REG_COUNTER(IDTR_PORTn_RETRY_CNTR);
    }

    return(0);
}

static int port_show_summary(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_BMP    pbmp;
    uint32_t value;

    __PARSE_ARG_PBMP(pbmp, argc, argv);

    printf("Port    : ");
    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("%s ", port_def_tbl[pid].port_name);
    }

    printf("\nEnabled :");
    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        value = idt_reg_r32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid));
        int enabled = !(value & CTL_1_CSR__PORT_DIS);
        printf("%*.s", (int)strlen(port_def_tbl[pid].port_name)+1-3, " ");
        printf("%3s", enabled?"yes":"no");
    }

    printf("\nSpeed   :");
    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        int lane_start = port_get_lane_start(pid);
        value = idt_reg_r32(g_idtfd, IDTR_LANEn_CTL(lane_start));
        int tx_rate = (value & LANE_CTL__TXRATE) >> LANE_CTL__TXRATE_SHIFT;
        value = idt_reg_r32(g_idtfd, IDTR_PLL_CTL_1(port_get_pll(pid)));
        int pll_div_sel = (value & PLL_CTL_1__PLL_DIV_SEL);
        int index = port_get_index_conf(tx_rate, pll_div_sel);
        printf("%*.s", (int)strlen(port_def_tbl[pid].port_name)+1-8, " ");
        printf("%8s", (index>=0)?linkspd_tbl[index].speed_desc:"UNKNOWN");
    }

    printf("\nLinkup  :");
    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        value = idt_reg_r32(g_idtfd, IDTR_PORTn_ERR_STAT_CSR(pid));
        int linkup = !!(value & ERR_STAT_CSR__PORTOK);
        printf("%*.s", (int)strlen(port_def_tbl[pid].port_name)+1-4, " ");
        printf("%4s", linkup?"up":"down");
    }
    printf("\n");

    return(0);
}

static int port_show_status(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_BMP    pbmp;
    uint32_t value;

    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("\n  <%s>\n", port_def_tbl[pid].port_name);

        int lane_start = port_get_lane_start(pid);
        for (int lane = lane_start; lane < lane_start+4; lane++) {
            printf("  - LANE#%d\n", lane);
            __DUMP_REG_HEX(IDTR_LANEn_CTL);
            value = idt_reg_r32(g_idtfd, IDTR_LANEn_CTL(lane));
            __DUMP_REG_BIT(value, TXRATE, 27, 2);
            __DUMP_REG_BIT(value, RXRATE, 29, 2);
        }

        printf("  - PLL#%d\n", port_get_pll(pid));
        __DUMP_REG_HEX(IDTR_PLL_CTL_1);
        value = idt_reg_r32(g_idtfd, IDTR_PLL_CTL_1(port_get_pll(pid)));
        __DUMP_REG_BIT(value, PLL_DIV_SEL, 31, 1);

        printf("  - DEBUG\n");
        __DUMP_REG_HEX(IDTR_PORTn_ERR_STAT_CSR);
        value = idt_reg_r32(g_idtfd, IDTR_PORTn_ERR_STAT_CSR(pid));
        __DUMP_REG_BIT(value, PORT_OK, ERR_STAT_CSR__PORTOK, 1);
        __DUMP_REG_BIT(value, PORT_UNINIT, 31, 1);

        __DUMP_REG_HEX(IDTR_PORTn_CTL_1_CSR);
        value = idt_reg_r32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid));
        __DUMP_REG_BIT(value, PWIDTH, 0, 2);
        __DUMP_REG_BIT(value, INIT_PWIDTH, 2, 3);
        __DUMP_REG_BIT(value, PWIDTH_OVRD, 5, 3);
        __DUMP_REG_BIT(value, PORT_DIS, 8, 1);
        __DUMP_REG_BIT(value, OUTPUT_PORT_EN, 9, 1);
        __DUMP_REG_BIT(value, INPUT_PORT_EN, 10, 1);
        __DUMP_REG_BIT(value, STOP_ON_FAIL_EN, 28, 1);
        __DUMP_REG_BIT(value, DROP_PKT_EN, 29, 1);
        __DUMP_REG_BIT(value, PORT_LOCKOUT, 30, 1);
        __DUMP_REG_BIT(value, PORT_TYPE, 31, 1);

        __DUMP_REG_HEX(IDTR_PORTn_OPS);
        __DUMP_REG_HEX(IDTR_PORTn_IMPL_SPEC_ERR_DET);
        __DUMP_REG_HEX(IDTR_PORTn_IMPL_SPEC_ERR_RATE_EN);
    }

    return(0);
}

static int port_enable_counters(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_BMP    pbmp;
    int enable;

    if (argc < 1) {
        goto out;
    }
    PARSE_ARG_INT(enable);
    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        idt_port_enable_counter(g_idtfd, pid, enable);
    }
    return(0);
out:
    return(-1);
}

static int port_recovery(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_BMP    pbmp;

    __PARSE_ARG_PBMP(pbmp, argc, argv);

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;

        if (pid == IDT_PORT_SRIO0 || pid == IDT_PORT_SRIO1)
            continue;

        printf("  recovery <%s>\n", port_def_tbl[pid].port_name);
        idt_port_recovery(g_idtfd, pid);
    }
    return(0);
}

static int port_acc_current_bmp(int argc, char* argv[])
{
    PORT_BMP    pbmp;
    if (argc < 1) {
        printf("  current PBMP : 0x%02x\n", port_cur_bmp);
        goto out;
    }
    __PARSE_ARG_PBMP(pbmp, argc, argv);
    port_cur_bmp = pbmp & PORTBMP_ALL;

out:
    return(0);
}

int __ccsr_write32(uint32_t reg, uint32_t value);
int port_init(int argc, char* argv[])
{
    PORT_ID pid;
    PORT_BMP    pbmp;
    uint32_t link_speed = 0;
    int linkspd_idx = -1;
    uint32_t value;

    if (argc < 1) {
        goto out;
    }
    PARSE_ARG_INT(link_speed);
    __PARSE_ARG_PBMP(pbmp, argc, argv);

    linkspd_idx = port_get_index_speed(link_speed);
    if (linkspd_idx < 0) {
        printf("ERROR: Invalid link speed, please specify 1/3/5/6\n");
        goto out;
    }

    for(pid = 0; pid < IDT_MAX_PORT_NUM; pid++) {
        if (!PORT_EXISTS(pbmp, pid))
            continue;
        printf("  initializing <%s> ... \n", port_def_tbl[pid].port_name);

        // ref. CPS-1432 datasheet 201462(p.192)
        // 9.2.3 Register Initialization
        //---------------------------------------------------------------------
        // 1. Disable port
        //---------------------------------------------------------------------
#define CTL_1_CSR__PORT_DIS         DSBIT(8)
#define CTL_1_CSR__PORT_LOCKOUT     DSBIT(30)
        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
                     CTL_1_CSR__PORT_DIS | CTL_1_CSR__PORT_LOCKOUT,
                     CTL_1_CSR__PORT_DIS | CTL_1_CSR__PORT_LOCKOUT);
        //---------------------------------------------------------------------
        // 2. Change the lane to port mapping (SKIP: H/W does it for us)
        //---------------------------------------------------------------------

        //---------------------------------------------------------------------
        // 3. Change the lane to port speed setting
        //---------------------------------------------------------------------
        int index = linkspd_idx;
        if (pid == IDT_PORT_SRIO0 || pid == IDT_PORT_SRIO1) {
            // force link speed to 5G when port is connected to host
            index = port_get_index_speed(5);
        }
        int pll_div_sel = linkspd_tbl[index].pll_div_sel;
        int rate = linkspd_tbl[index].rate;
        printf("    speed : %s\n", linkspd_tbl[index].speed_desc);
        // 3a. Change the speed per-lane
        int lane_start = port_get_lane_start(pid);
        for (int lane = lane_start; lane < lane_start+4; lane++) {
            idt_reg_wm32(g_idtfd, IDTR_LANEn_CTL(lane),
                         LANE_CTL__TXRATE | LANE_CTL__RXRATE,
                         (rate << LANE_CTL__TXRATE_SHIFT) |
                         (rate << LANE_CTL__RXRATE_SHIFT));
        }
        // 3b. Change the PLL speed
        idt_reg_wm32(g_idtfd, IDTR_PLL_CTL_1(port_get_pll(pid)),
                     PLL_CTL_1__PLL_DIV_SEL,
                     pll_div_sel);
        // 3c. Reset the PLLs
#define DEVICE_RESET_CTL__PLL_SEL    DS2CPU(13)
        idt_reg_w32(g_idtfd,
                    IDTR_DEVICE_RESET_CTL,
                    0x80000000 |
                    (0x01 << (port_get_pll(pid)+DEVICE_RESET_CTL__PLL_SEL)) | // PLL_SEL
                    (0x01 << pid)); // PORT_SEL

        //---------------------------------------------------------------------
        // 4. Configuration
        //---------------------------------------------------------------------
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

        //---------------------------------------------------------------------
        // 5. Enable port
        //---------------------------------------------------------------------
        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
                     CTL_1_CSR__PORT_DIS | CTL_1_CSR__PORT_LOCKOUT,
                     0);
        //---------------------------------------------------------------------
        // 6. Disable IDLE2 functions
        //---------------------------------------------------------------------
        idt_reg_w32(g_idtfd, IDTR_PORTn_ERR_STAT_CSR(pid), 0x80000000);

        //---------------------------------------------------------------------
        // 7. Reset port
        //---------------------------------------------------------------------
        idt_reg_w32(g_idtfd, IDTR_DEVICE_RESET_CTL, 0x80000000 | (0x01 << pid));

        //---------------------------------------------------------------------
        // 8. Configure port width (SKIP: No override)
        //---------------------------------------------------------------------
//#define CTL_1_CSR__PWIDTH_OVRD_SHIFT    DS2CPU(7)
//#define CTL_1_CSR__PWIDTH_OVRD          DSBIT(5) | DSBIT(6) | DSBIT(7)

        //---------------------------------------------------------------------
        // 9. Enable the ports to receive and transmit packets
        //---------------------------------------------------------------------
#define CTL_1_CSR__OUTPUT_PORT_EN   DSBIT(9)
#define CTL_1_CSR__INPUT_PORT_EN    DSBIT(10)
//        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
//         CTL_1_CSR__OUTPUT_PORT_EN | CTL_1_CSR__INPUT_PORT_EN | CTL_1_CSR__PWIDTH_OVRD,
//         CTL_1_CSR__OUTPUT_PORT_EN | CTL_1_CSR__INPUT_PORT_EN | (0x6<<CTL_1_CSR__PWIDTH_OVRD_SHIFT));
        idt_reg_wm32(g_idtfd, IDTR_PORTn_CTL_1_CSR(pid),
                     CTL_1_CSR__OUTPUT_PORT_EN | CTL_1_CSR__INPUT_PORT_EN ,
                     CTL_1_CSR__OUTPUT_PORT_EN | CTL_1_CSR__INPUT_PORT_EN);

        //---------------------------------------------------------------------
        // do recovery
        //---------------------------------------------------------------------
        if (pid == IDT_PORT_SRIO0) {
            // reset host's ackID
            __ccsr_write32(0xc0148, 0);            // SRIO1_P2LASCSR
        } else if (pid == IDT_PORT_SRIO1) {
            // reset host's ackID
            __ccsr_write32(0xc0168, 0);            // SRIO2_P2LASCSR
        } else {
            uint32_t value;
            value = idt_reg_r32(g_idtfd, IDTR_PORTn_ERR_STAT_CSR(pid));
            if (value & DSBIT(30)) {
                // port ok
                idt_port_recovery(g_idtfd, pid);
            }
        }
        value = idt_reg_r32(g_idtfd, IDTR_PORTn_ERR_STAT_CSR(pid));
        printf("    link_status : %s\n", !!(value & DSBIT(30))?"UP":"DOWN");
    }

    return(0);
out:
    return(-1);
}

const cmd_decl_t cmd_port_tbl[] = {
    {"INIT",        port_init,              "Init port | Param: <LINK_SPEED> [PORT_BMP]"},
    {"Recovery",    port_recovery,          "Recovery port | Param: [PORT_BMP]"},
    {"EnableCntr",  port_enable_counters,   "Enable port-level counters | Param: <ENABLE> [PORT_BMP]"},
    {"Showsummary", port_show_summary,      "Show ports summary | Param: [PORT_BMP]"},
    {"ShowStatus",  port_show_status,       "Show port status | Param: [PORT_BMP]"},
    {"ShowSTatics", port_show_stats,        "Show port statistics | Param: [PORT_BMP]"},
    {"PBMP",        port_acc_current_bmp,   "Access current pbmp | Param: [PORT_BMP]"},
    {NULL},
};

int cli_port(int argc, char* argv[])
{
    int ret = -1;
    int found = 0;

    const cmd_decl_t* cmd_tbl = cmd_port_tbl;
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
