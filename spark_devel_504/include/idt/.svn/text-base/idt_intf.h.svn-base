#ifndef __IDT_H__
#define __IDT_H__

#include "inttypes.h"

#define IDT_MOD_VER     "0.9.151230"

typedef uint8_t     PORT_ID;
typedef uint16_t    DEV_ID;

#define __IDT_MAX_PORT_NUM_1432 (14)
#define IDT_MAX_PORT_NUM        __IDT_MAX_PORT_NUM_1432
#define IDT_MAX_DEV_ID          (0x100)

#define IDT_PORT_DEFAULT        (0xDE)
#define IDT_PORT_DISCARD        (0xDF)

#define IDT_PORT_SRIO0          (0x01)
#define IDT_PORT_SRIO1          (0x04)

#define IDTR_DEV_IDENT_CAR                  (0x000000)
#define IDTR_DEV_INF_CAR                    (0x000004)

#define IDTR_RTE_DEFAULT_PORT_CSR           (0x000078)

#define IDTR_PORT_LINK_TO_CTL_CSR           (0x000120)

#define IDTR_PORTn_LINK_MAINT_REQ_CSR(P)    (0x000140 + 0x20*(P))
#define IDTR_PORTn_LINK_MAINT_RESP_CSR(P)   (0x000144 + 0x20*(P))
#define IDTR_PORTn_LOCAL_ACKID_CSR(P)       (0x000148 + 0x20*(P))
#define IDTR_PORTn_ERR_STAT_CSR(P)          (0x000158 + 0x20*(P))
    #define ERR_STAT_CSR__PORTOK        DSBIT(30)
#define IDTR_PORTn_CTL_1_CSR(P)             (0x00015C + 0x20*(P))
    #define CTL_1_CSR__PORT_DIS         DSBIT(8)
#define IDTR_PORTn_ERR_RATE_EN_CSR(P)       (0x001044 + 0x40*(P))
#define IDTR_PORTn_ERR_RATE_THRESH_CSR(P)   (0x00106C + 0x40*(P))

#define IDTR_PORTn_DEV_RTE_TABLE(P, DID)    (0xE10000 + 0x1000*(P) + 0x4*(DID))

#define IDTR_DEVICE_CTL_1                   (0xF2000C)
#define IDTR_DEVICE_RESET_CTL               (0xF20300)

#define IDTR_PORTn_OPS(P)                   (0xF40004 + 0x100*(P))
#define IDTR_PORTn_IMPL_SPEC_ERR_DET(P)     (0xF40008 + 0x100*(P))
#define IDTR_PORTn_IMPL_SPEC_ERR_RATE_EN(P) (0xF4000C + 0x100*(P))
#define IDTR_PORTn_VC0_PA_TX_CNTR(P)        (0xF40010 + 0x100*(P))
#define IDTR_PORTn_NACK_TX_CNTR(P)          (0xF40014 + 0x100*(P))
#define IDTR_PORTn_VC0_RTRY_TX_CNTR(P)      (0xF40018 + 0x100*(P))
#define IDTR_PORTn_VC0_PKT_TX_CNTR(P)       (0xF4001C + 0x100*(P))
#define IDTR_PORTn_TRACE_CNTR_0(P)          (0xF40020 + 0x100*(P))
#define IDTR_PORTn_TRACE_CNTR_1(P)          (0xF40024 + 0x100*(P))
#define IDTR_PORTn_TRACE_CNTR_2(P)          (0xF40028 + 0x100*(P))
#define IDTR_PORTn_TRACE_CNTR_3(P)          (0xF4002C + 0x100*(P))
#define IDTR_PORTn_FILTER_CNTR_0(P)         (0xF40030 + 0x100*(P))
#define IDTR_PORTn_FILTER_CNTR_1(P)         (0xF40034 + 0x100*(P))
#define IDTR_PORTn_FILTER_CNTR_2(P)         (0xF40038 + 0x100*(P))
#define IDTR_PORTn_FILTER_CNTR_3(P)         (0xF4003C + 0x100*(P))
#define IDTR_PORTn_VC0_PA_RX_CNTR(P)        (0xF40040 + 0x100*(P))
#define IDTR_PORTn_NACK_RX_CNTR(P)          (0xF40044 + 0x100*(P))
#define IDTR_PORTn_VC0_RTRY_RX_CNTR(P)      (0xF40048 + 0x100*(P))
#define IDTR_PORTn_VC0_CPB_TX_CNTR(P)       (0xF4004C + 0x100*(P))
#define IDTR_PORTn_VC0_PKT_RX_CNTR(P)       (0xF40050 + 0x100*(P))
#define IDTR_PORTn_TRACE_PW_CTL(P)          (0xF40058 + 0x100*(P))
#define IDTR_PORTn_VC0_PKT_DROP_RX_CNTR(P)  (0xF40064 + 0x100*(P))
#define IDTR_PORTn_VC0_PKT_DROP_TX_CNTR(P)  (0xF40068 + 0x100*(P))
#define IDTR_PORTn_VC0_TTL_DROP_CNTR(P)     (0xF4006C + 0x100*(P))
#define IDTR_PORTn_VC0_CRC_LIMIT_DROP_CNTR(P)   (0xF40070 + 0x100*(P))
#define IDTR_PORTn_RETRY_CNTR(P)            (0xF400CC + 0x100*(P))

#define IDTR_PLL_CTL_1(PLL)                 (0xFF0000 + 0x10*(PLL))
    #define PLL_CTL_1__PLL_DIV_SEL      DSBIT(31)

#define IDTR_LANEn_CTL(L)                   (0xff8000 + 0x100*(L))
    #define LANE_CTL__TXRATE_SHIFT      (DS2CPU(28))
    #define LANE_CTL__TXRATE            (DSBIT(27) | DSBIT(28))
    #define LANE_CTL__RXRATE_SHIFT      (DS2CPU(30))
    #define LANE_CTL__RXRATE            (DSBIT(29) | DSBIT(30))

#define IDTR_LANEn_ERR_RATE_EN(L)           (0xFF8010 + 0x100*(L))

#define DS2CPU(P)   (31-(P))
#define DSBIT(N)    (0x01<<(DS2CPU(N)))
#define CPUBIT(N)   (0x01<<(N))
#define H16(D)      (((D)>>16) & 0xffff)
#define L16(D)      (((D)>> 0) & 0xffff)

typedef struct
{
    uint16_t   dev_id;
    uint16_t   vendor_id;
    uint16_t   major;
    uint8_t    minor;
} idt_dev_info_t;

int idt_module_init(const char* log_cat);
int idt_dev_open(int i2c_bus, int i2c_addr);
void idt_dev_get_info(int fd, idt_dev_info_t* info);
void idt_dev_close(int fd);

uint32_t idt_reg_r32(int fd, uint32_t reg);
int idt_reg_w32(int fd, uint32_t reg, uint32_t value);
int idt_reg_wm32(int fd, uint32_t reg, uint32_t mask, uint32_t value);

int idt_routetbl_set(int fd, PORT_ID src_port, DEV_ID dst_id, PORT_ID dst_port);
PORT_ID idt_routetbl_get(int fd, PORT_ID src_port, DEV_ID dst_id);

int idt_port_recovery(int fd, PORT_ID port);

int idt_port_enable_counter(int fd, PORT_ID port, int enable);

#endif
