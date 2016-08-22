#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <assert.h>
#include "spark.h"
#include "zlog/zlog.h"
#include "idt/idt_intf.h"

zlog_category_t* idt_zc = NULL;
pthread_mutex_t idt_i2c_lock;

int idt_dev_open(int i2c_bus, int i2c_addr)
{
    char i2c_dev[32];
    int fd = -1;
    int ret;

    sprintf(i2c_dev, "/dev/i2c-%d", i2c_bus);
    fd = open(i2c_dev, O_RDWR);
    if (fd < 0) {
        return(fd);
    }
    
    ret = ioctl(fd, I2C_TENBIT, 0);
    if (ret < 0) {
        goto errout;
    }
    ret = ioctl(fd, I2C_SLAVE, i2c_addr);
    if (ret < 0) {
        goto errout;
    }
    
    idt_dev_info_t info;
    idt_dev_get_info(fd, &info);
    zlog_notice(idt_zc, "vendor_id = 0x%04x", info.vendor_id);
    zlog_notice(idt_zc, "device_id = 0x%04x", info.dev_id);
    zlog_notice(idt_zc, "revision  = %d.%c", info.major, info.minor + 'A');

    return(fd);

errout:
    if (fd > 0) {
        close(fd);
    }
    return(ret);
}

void idt_dev_close(int fd)
{
    if (fd > 0)
        close(fd);
}

#define IDT_REG2OFFSET(R, D)    \
    R >>= 2; \
    *((char*)(D)+0) = ((R)>>16 & 0xff); \
    *((char*)(D)+1) = ((R)>>8 & 0xff); \
    *((char*)(D)+2) = ((R)>>0 & 0xff);

uint32_t idt_reg_r32(int fd, uint32_t reg)
{
    char reg_offset[3];
    int ret;
    uint32_t value = -1;

    pthread_mutex_lock(&idt_i2c_lock);
    IDT_REG2OFFSET(reg, reg_offset);
    ret = write(fd, reg_offset, sizeof(reg_offset));
    if (ret == sizeof(reg_offset)) {
        usleep(10);
        ret = read(fd, (void*)(intptr_t)&value, sizeof(uint32_t));
    }
    pthread_mutex_unlock(&idt_i2c_lock);

    return value;
}

int idt_reg_w32(int fd, uint32_t reg, uint32_t value)
{
    char write_buffer[7];
    int ret =-1;

    pthread_mutex_lock(&idt_i2c_lock);
    IDT_REG2OFFSET(reg, write_buffer);
    memcpy((write_buffer+3), &value, sizeof(value));
    ret = write(fd, write_buffer, sizeof(write_buffer));
    pthread_mutex_unlock(&idt_i2c_lock);

    return ret;
}

int idt_reg_wm32(int fd, uint32_t reg, uint32_t mask, uint32_t value)
{
    uint32_t data = idt_reg_r32(fd, reg);
    data &= ~mask;
    data |= (value & mask);
    return(idt_reg_w32(fd, reg, data));
}

int idt_routetbl_set(int fd, PORT_ID src_port, DEV_ID dst_id, PORT_ID dst_port)
{
    uint32_t regaddr = IDTR_PORTn_DEV_RTE_TABLE(src_port, dst_id);
    
    zlog_notice(idt_zc, "route_tbl: set entry [%d] 0x%02x -> [%d]", src_port, dst_id, dst_port);

    return (idt_reg_w32(fd, regaddr, dst_port));
}

PORT_ID idt_routetbl_get(int fd, PORT_ID src_port, DEV_ID dst_id)
{
    uint32_t regaddr = IDTR_PORTn_DEV_RTE_TABLE(src_port, dst_id);
    return (idt_reg_r32(fd, regaddr));
}

int idt_port_recovery(int fd, PORT_ID port)
{

    idt_reg_w32(fd, IDTR_PORTn_ERR_RATE_EN_CSR(port), 0); // disable all
    idt_reg_w32(fd, IDTR_PORTn_ERR_RATE_THRESH_CSR(port), 0); // disable all trigger

    for(int i=0; i<4; i++) {
        idt_reg_w32(fd, IDTR_LANEn_ERR_RATE_EN(port*4+i), 0); // disable all
    }

    // PWIDTH_OVRD:0b110 (disable 2xmode) | INPUT_PORT_EN:1
    idt_reg_w32(fd, IDTR_PORTn_CTL_1_CSR(port), 0xd6600001);
    // CLR:1
    idt_reg_w32(fd, IDTR_PORTn_LOCAL_ACKID_CSR(port), 0x80000000);
    // CMD: 0b011 (Reset Device)
    idt_reg_w32(fd, IDTR_PORTn_LINK_MAINT_REQ_CSR(port), 0x00000003);
   
    while(!(idt_reg_r32(fd, IDTR_PORTn_LINK_MAINT_RESP_CSR(port)) &
          0x80000000/* VALID*/)) {
        usleep(100);
    }
    
    // DO_RESET: 1
    idt_reg_w32(fd, IDTR_DEVICE_RESET_CTL, (0x80000000 | (0x1<<port)));
    // CMD: 0b100 (Input Status)
    idt_reg_w32(fd, IDTR_PORTn_LINK_MAINT_REQ_CSR(port), 0x00000004);

    zlog_notice(idt_zc, "port recovery done: port_id=%u", port);

    return 0;
}

int idt_port_enable_counter(int fd, PORT_ID p, int enable)
{
#define IDTR_PORT_OPS__CNTRS_EN      (DSBIT(5))
    return(idt_reg_wm32(fd,
                        IDTR_PORTn_OPS(p),
                        IDTR_PORT_OPS__CNTRS_EN,
                        enable ? IDTR_PORT_OPS__CNTRS_EN : 0));
}

void idt_dev_get_info(int fd, idt_dev_info_t* info)
{
    uint32_t data;
    
    memset(info, 0, sizeof(idt_dev_info_t));
    data = idt_reg_r32(fd, IDTR_DEV_IDENT_CAR);
    info->dev_id = H16(data);
    info->vendor_id = L16(data);
    
    data = idt_reg_r32(fd, IDTR_DEV_INF_CAR);
    info->major = (data >> DS2CPU(10)) & 0x03;
    info->minor = (data >> DS2CPU(15)) & 0x1f;
}

int idt_module_init(const char* log_cat)
{
    if (idt_zc) {
        fprintf(stderr, "%s: already initialized!\n", __func__);
        return(SPKERR_BADSEQ);
    }

    idt_zc = zlog_get_category(log_cat?log_cat:"IDT");
    if (!idt_zc) {
        fprintf(stderr, "%s: failed to initialize log system!\n", __func__);
//        return(SPKERR_LOGSYS);
    }

    pthread_mutex_init(&idt_i2c_lock, NULL);    

    zlog_notice(idt_zc, "module initialized.");

    return(SPK_SUCCESS);
}
