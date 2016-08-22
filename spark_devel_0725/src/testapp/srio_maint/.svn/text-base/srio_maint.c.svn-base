#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <time.h>

#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>

#include <readline/readline.h>
#include <readline/history.h>

#define SRIO_CFG_WND_ID     (1)
#define SRIO_CFG_WND_SZ     (0x1000000)
#define SRIO_DEF_HOP_CNT    (0)
#define SRIO_DEF_TARGET_ID  (0xff)

typedef struct {
    void*       va;
    dma_addr_t  pa;
    struct dma_mem* dmamem;
} srio_addr_t;

typedef struct {
    uint8_t port_id;
    uint8_t hop_cnt;
    uint8_t target_id;
    uint8_t rsvd;
    srio_addr_t swnd;
    srio_addr_t rx_cfg_region;
    srio_addr_t tx_cfg_region;
    struct dma_ch* dma_dev;
} srio_port_ctx_t;

typedef struct {
    const char* cmd;
    int (*cli_func)(srio_port_ctx_t* ctx, int argc, char* argv[]);
    const char* help;
} cmd_decl_t;

static struct srio_dev* srio_dev = NULL;

#define VERIFY_RET(FUNC, RET) \
    if (RET) { \
        printf("%s: ret=%d, assert!", #FUNC, RET); \
        assert(0); \
    }

#define PARSE_ARG_INT(RET) \
    RET = strtoul(argv[0], NULL, 0); argc--; argv++;

// CAUTION: printf/thread not safe
static char* __format_hex(uint32_t value)
{
    static char __fmt[128];
    int ptr = 0;

    sprintf(__fmt, "0x%08x ", value);
    ptr += strlen(__fmt);
    for (int i = 0; i < 32; i++) {
        if (!(i % 8))
            __fmt[ptr++] = '|';
        __fmt[ptr++] = (value & (0x01 << i)) ? '1' : '.';
        if (!((i + 5) % 8))
            __fmt[ptr++] = ' ';
    }
    __fmt[ptr++] = '|';

    return(__fmt);
}

static inline int __get_order(size_t size)
{
    int order = -1;
    size_t sz_cpy = size;

    do {
        size >>= 1;
        order++;
    } while (size);

    if(unlikely(sz_cpy != (0x01 << order))) {
        assert(0);
    }
    return order;
}

static int srio_do_dma(struct dma_ch* dmadev, uint64_t src_pa, uint64_t dest_pa, size_t size)
{
    int ret;

#if 0
    printf("srio_do_dma: dev=%p, spa=0x%lx, dpa=0x%lx, size=%zu\n",
           dmadev, src_pa, dest_pa, size);
#endif
    ret = fsl_dma_direct_start(dmadev, src_pa, dest_pa, size);
    if (!ret) {
        ret = fsl_dma_wait(dmadev);
    }

    return (ret);
}

static int srio_cfg_check_region(uint32_t reg, uint32_t size)
{
    if ((reg & 0x3) || (reg + size > SRIO_CFG_WND_SZ)) {
        printf("Invalid region: offset=0x%x, size=0x%lx\n",
               reg, sizeof(uint32_t));
        return(-1);
    }

    return(0);
}

static void* srio_dmamem_alloc(size_t pool_size, dma_addr_t* pa, struct dma_mem** dmamem)
{
    void* va = NULL;

    // FIXME: need protection
    dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
                                     NULL, pool_size);
    assert(dma_mem_generic);
    *dmamem = dma_mem_generic;

    va = __dma_mem_memalign(4 * 1024, pool_size / 2);
    assert(va);

    *pa = __dma_mem_vtop(va);
    memset(va, 0, pool_size);

    return(va);
}

static int srio_cfg_read(srio_port_ctx_t* ctx, uint32_t reg, uint32_t* data)
{
    int ret = srio_cfg_check_region(reg, sizeof(uint32_t));

    if (ret) {
        goto out;
    }

    memset(ctx->rx_cfg_region.va + reg, 0xfe, sizeof(uint32_t));
    ret = srio_do_dma(ctx->dma_dev,
                      ctx->tx_cfg_region.pa + reg,
                      ctx->rx_cfg_region.pa + reg,
                      sizeof(uint32_t));
    if (!ret) {
        *data = *(volatile uint32_t*)((char*)ctx->rx_cfg_region.va + reg);
    }

out:
    return(ret);
}

static int srio_cfg_write(srio_port_ctx_t* ctx, uint32_t reg, uint32_t data)
{
    int ret = srio_cfg_check_region(reg, sizeof(uint32_t));

    if (ret) {
        goto out;
    }

    *(volatile uint32_t*)((char*)ctx->rx_cfg_region.va + reg) = data;
    ret = srio_do_dma(ctx->dma_dev,
                      ctx->rx_cfg_region.pa + reg,
                      ctx->tx_cfg_region.pa + reg,
                      sizeof(uint32_t));

out:
    return(ret);
}

static int srio_cfg_set_targetid(srio_port_ctx_t* ctx, uint8_t target_id)
{
    ctx->target_id = target_id;

    return(fsl_srio_set_targetid(srio_dev, ctx->port_id, SRIO_CFG_WND_ID, target_id));
}

static int srio_cfg_set_hopcnt(srio_port_ctx_t* ctx, uint8_t hopcnt)
{
    int ret = -1;

    ctx->hop_cnt = hopcnt;

    uint64_t fake_addr = (uint64_t)hopcnt << 24;
//    fake_addr |= ((uint64_t)(ctx->target_id)) << 34;
    int wnd_order = __get_order(SRIO_CFG_WND_SZ) - 1;

    ret = fsl_srio_set_obwin(srio_dev, ctx->port_id, SRIO_CFG_WND_ID,
                             ctx->tx_cfg_region.pa, fake_addr, wnd_order);
    VERIFY_RET(fsl_srio_set_obwin, ret);
    
    // target id will be cleared after the function above
    // we should set it again
    ret = srio_cfg_set_targetid(ctx, ctx->target_id);
    VERIFY_RET(srio_cfg_set_targetid, ret);

    return(0);
}

static srio_port_ctx_t* srio_init_port(int port_id)
{
    srio_port_ctx_t* ctx = NULL;

    assert(srio_dev);
    int ret = -1;

    if (port_id < 0 || port_id > 1) {
        return (NULL);
    }

    ctx = malloc(sizeof(srio_port_ctx_t));
    assert(ctx);
    memset(ctx, 0, sizeof(srio_port_ctx_t));
    
    ctx->port_id = port_id;

    fsl_srio_connection(srio_dev, port_id);
    if (!(fsl_srio_port_connected(srio_dev) & (0x1 << port_id))) {
        printf("port#%d> Port is not connected\n", port_id);
        goto errout;
    }

    ret = fsl_srio_set_deviceid(srio_dev, port_id, 0);
    VERIFY_RET(fsl_srio_set_deviceid, ret);
    ret = fsl_srio_set_seg_num(srio_dev, port_id, 1, 0);
    VERIFY_RET(fsl_srio_set_seg_num, ret);
    ret = fsl_srio_set_subseg_num(srio_dev, port_id, 1, 0);
    VERIFY_RET(fsl_srio_set_subseg_num, ret);

    // acquire swnd
    struct srio_port_info srio_wnd;
    fsl_srio_get_port_info(srio_dev, port_id + 1, &srio_wnd, &ctx->swnd.va);
    assert(ctx->swnd.va);
    ctx->swnd.pa = srio_wnd.range_start;
    printf("port#%d> Acquire SWND: region_pa={0x%lx+0x%lx), va=%p\n",
           port_id, ctx->swnd.pa, srio_wnd.range_size, ctx->swnd.va);

    // alloc rx cfg_region
    ctx->rx_cfg_region.va = srio_dmamem_alloc(SRIO_CFG_WND_SZ,
                            &ctx->rx_cfg_region.pa,
                            &ctx->rx_cfg_region.dmamem);

    // bind tx cfg_region
    int wnd_order = __get_order(SRIO_CFG_WND_SZ) - 1;
    ctx->tx_cfg_region.pa = ctx->swnd.pa;
    ctx->tx_cfg_region.va = ctx->swnd.va;

    ret = srio_cfg_set_targetid(ctx, SRIO_DEF_TARGET_ID);
    VERIFY_RET(srio_cfg_set_targetid, ret);

    ret = srio_cfg_set_hopcnt(ctx, SRIO_DEF_HOP_CNT);
    VERIFY_RET(srio_cfg_set_hopcnt, ret);

    ret = fsl_srio_set_obwin_attr(srio_dev, port_id, SRIO_CFG_WND_ID,
                                  SRIO_ATTR_MAINTR, SRIO_ATTR_MAINTW);
    VERIFY_RET(fsl_srio_set_obwin_attr, ret);
    printf("port#%d> bind obw for config region: win_id=%d, "
           "pa=0x%lx, wnd_order=%d, ret=%d\n",
           port_id, SRIO_CFG_WND_ID, ctx->tx_cfg_region.pa,
           wnd_order, ret);

    ret = fsl_dma_chan_init(&ctx->dma_dev, port_id, 0);
    VERIFY_RET(fsl_dma_chan_init, ret);

    printf("port#%d> init dma chan: id=%d, dev=%p, ret=%d\n",
           port_id, 0, ctx->dma_dev,  ret);
    ret = fsl_dma_chan_basic_direct_init(ctx->dma_dev);
    VERIFY_RET(fsl_dma_chan_basic_direct_init, ret);

    ret = fsl_dma_chan_bwc(ctx->dma_dev, DMA_BWC_1024);
    VERIFY_RET(fsl_dma_chan_bwc, ret);

    return(ctx);

errout:
    if (ctx) {
        free(ctx);
        ctx = NULL;
    }

    return(NULL);
}

static int cli_cfg_read(srio_port_ctx_t* ctx, int argc, char* argv[])
{
    uint32_t reg_offset;
    int count = 1;
    int ret;

    if (argc < 1) {
        return(-1);
    }

    PARSE_ARG_INT(reg_offset);
    if (argc > 0) {
        PARSE_ARG_INT(count);
    }

    for (int i = 0; i < count; i++) {
        uint32_t value;
        uint32_t offset = reg_offset + sizeof(uint32_t) * i;
        ret = srio_cfg_read(ctx, offset, &value);
        if (!ret) {
            printf("[0x%06x] : %s\n", offset, __format_hex(value));
        } else {
            printf("Failed to read from config region: offset=0x%x\n", offset);
            break;
        }
    }

    return(ret);
}

static int cli_cfg_write(srio_port_ctx_t* ctx, int argc, char* argv[])
{
    uint32_t reg_offset;
    uint32_t data, data_back;
    int ret = -1;

    if (argc < 2) {
        return(-1);
    }

    PARSE_ARG_INT(reg_offset);
    PARSE_ARG_INT(data);

    ret = srio_cfg_write(ctx, reg_offset, data);
    if (ret) {
        printf("Failed to write data to config region: offset=0x%x\n", reg_offset);
        goto out;
    }

    // read back
    ret = srio_cfg_read(ctx, reg_offset, &data_back);
    VERIFY_RET(srio_cfg_read, ret);
    if (!ret) {
        printf("[0x%06x] : %s\n", reg_offset, __format_hex(data_back));
    } else {
        printf("Failed to read from config region: offset=0x%x\n", reg_offset);
    }

out:
    return(ret);
}

static int cli_cfg_acc_hopcnt(srio_port_ctx_t* ctx, int argc, char* argv[])
{
    uint8_t hop_cnt;
    uint32_t data;
    int ret;

    if (argc < 1) {
        printf("hop_cnt=%d\n", ctx->hop_cnt);
        return(0);
    }

    PARSE_ARG_INT(hop_cnt);

    ret = srio_cfg_set_hopcnt(ctx, hop_cnt);
    VERIFY_RET(srio_cfg_set_hopcnt, ret);

    // read vendor/device id
    ret = srio_cfg_read(ctx, 0, &data);
    if (!ret) {
        printf("[0x%06x] : %s\n", 0, __format_hex(data));
    } else {
        printf("No response: hop_cnt=%d\n", hop_cnt);
    }

    return(ret);
}

static int cli_cfg_acc_tid(srio_port_ctx_t* ctx, int argc, char* argv[])
{
    uint8_t target_id;
    int ret;

    if (argc < 1) {
        printf("target_id=0x%02x\n", ctx->target_id);
        return(0);
    }

    PARSE_ARG_INT(target_id);

    ret = srio_cfg_set_targetid(ctx, target_id);
    VERIFY_RET(srio_cfg_set_targetid, ret);

    return(ret);
}

static int cli_quit(srio_port_ctx_t* ctx, int argc, char* argv[])
{
    printf("Au revoir!\n\n");
    // FIXME: do cleanup
    exit(0);
    return(0);
}

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

/* Read a string, and return a pointer to it.
   Returns NULL on EOF. */
static char *rl_gets(const char* prompt)
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

static char* utl_strcmp(const char* cmd, char* input)
{
    char cmd_short[1024];
    int i, cnt;

    if (!strcasecmp(cmd, input))
        return NULL;

    memset(cmd_short, 0, sizeof(cmd_short));
    cnt = 0;
    for (i = 0; i < strlen(cmd); i++) {
        if (cmd[i] >= 'A' && cmd[i] <= 'Z') {
            cmd_short[cnt++] = cmd[i];
        }
    }
    if (strlen(cmd_short) > 0 && !strcasecmp(cmd_short, input))
        return NULL;

    return(input);
}

static int print_cmd_table(const cmd_decl_t* cmd_tbl)
{
    while(cmd_tbl->cmd) {
        printf("\t%-12s- %s\n",
               cmd_tbl->cmd,
               cmd_tbl->help);
        cmd_tbl++;
    }
    return(0);
}
const cmd_decl_t cmd_root_tbl[] = {
    {"TargetID",  cli_cfg_acc_tid,    "Set/Get target id | Param: [TARGET_ID]"},
    {"HopCnt",    cli_cfg_acc_hopcnt, "Set/Get hop count | Param: [HOP_COUNT]"},
    {"Write",     cli_cfg_write,      "Write long | Param: <REG_OFFSET> <DATA>"},
    {"Read",      cli_cfg_read,       "Read long | Param: <REG_OFFSET> [COUNT=1]"},
    {"Quit",      cli_quit,           "Quit"},
    {NULL},
};

int main()
{
    int ret = -1;
    int     argc;
    char*   argv[128];
    int     i;

    of_init();
    ret = fsl_srio_uio_init(&srio_dev);
    VERIFY_RET(fsl_srio_uio_init, ret);

    srio_port_ctx_t* ctx = srio_init_port(0);
    assert(ctx);

    while(1) {
        char* cmd = rl_gets("srio_maint> ");
        if (cmd) {
            int found = 0;
            argc = 0;

            // trim lf/cr
            if (cmd[strlen(cmd) - 1] == '\n')
                cmd[strlen(cmd) - 1] = '\0';

            if (strlen(cmd) <= 0 || cmd[0] == '#')
                continue;

            // parse cmd
            while((argv[argc] = strtok(cmd, " ")) != NULL) {
                argc++;
                cmd = NULL;
            }
            argv[argc] = NULL;

            // exec cmd
            if (argc > 0) {
                for (i = 0; cmd_root_tbl[i].cmd; i++) {
                    if (!utl_strcmp(cmd_root_tbl[i].cmd, argv[0])) {
                        found = 1;
                        ret = (*cmd_root_tbl[i].cli_func)(ctx, argc - 1, argv + 1);
                        printf(".return = %d\n", ret);
                        break;
                    }
                }
            }
            if (!found || ret == -1) {
                print_cmd_table(cmd_root_tbl);
            }
        }
    }
    // never reach
    assert(0);

    return(0);
}