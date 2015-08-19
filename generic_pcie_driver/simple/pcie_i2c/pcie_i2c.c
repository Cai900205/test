/*************************************************************************
	> File Name: pcie_i2c.c
	> Author: 
	> Mail: 
	> Created Time: Wed 17 Dec 2014 06:34:38 PM CST
 ************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include <netdb.h>
#include <malloc.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include "../../include/sirius_pcie.h"

#define I2C_RST                  0x0000
#define CLK_CFG                  0x0008
#define TXDATA                   0x0018
#define RX_STATE                 0x0020
#define RX_DATA                  0x0028
#define GPIO_CTRL                0x0038


#define TX_DONE_MASK             0x200
#define TX_ADDR_MASK             0x100
#define RX_DATA_VALID            0x10000
#define SETUP_TIME               0x3f
#define FREQ_DIVIDED             0x1

static int init = 0;
static int g_fd = 0;

static int Pcie_user_write64(int g_handle, int addr, unsigned long long data)
{
    if (pcie_user_write64(g_handle, addr, data) != 0)
    {
        printf("write bar: 0x%02x , addr: 0x%016llx value : 0x%016llx error\n", 0, addr, data);
        exit(-1);
    }

}
static int Pcie_user_read64(int g_handle, int addr, unsigned long long * data)
{
    if (pcie_user_read64(g_handle, addr, data) != 0)
    {
        printf("read bar: 0x%02x , addr: 0x%016llx error\n", addr);
        exit(-1);
    }
}

int init_i2c_device(void)
{
    unsigned long long result = 1;
    if(init == 0)
    {
        /* 0. open device */
        int g_handle = open(SIRIUS_PCIE_NAME, O_RDWR);
        if (g_handle < 0)
        {
            printf("Can not open the board.\n");
            return 0;
        }

        /* 1. reset device */
        Pcie_user_write64(g_handle, I2C_RST, 0x1);
        while(result != 0)
        {
            Pcie_user_read64(g_handle, I2C_RST, &result);
#ifdef L_DEBUG
            printf("[%s] waiting for i2c device initializing ...\n",__func__);
#endif
        }

        /* 2. set the clock */
        Pcie_user_write64(g_handle, CLK_CFG, SETUP_TIME << 8 | FREQ_DIVIDED);

        /* 3. reset the gpio ctrl */
        Pcie_user_write64(g_handle, GPIO_CTRL, 0xf);
        usleep(100);
        Pcie_user_write64(g_handle, GPIO_CTRL, 0x0);
        usleep(100);
        Pcie_user_write64(g_handle, GPIO_CTRL, 0x1);
        init = 1;
        g_fd = g_handle;
    }
    return 0;
}

int pcie_i2c_write_switch(uint32_t slave_addr, uint32_t data)
{
    init_i2c_device();
    slave_addr &= 0xff;
    slave_addr <<= 1;
    slave_addr |= TX_ADDR_MASK;

    Pcie_user_write64(g_fd, TXDATA, slave_addr);

    data &= 0xff;
    data |= TX_DONE_MASK;
    Pcie_user_write64(g_fd, TXDATA, data);
}
int pcie_i2c_write(uint32_t slave_addr, uint32_t offset, uint32_t * data, int length)
{
    init_i2c_device();
    int i = 0;
    slave_addr &= 0xff;
    slave_addr <<= 1;
    slave_addr |= TX_ADDR_MASK;
    Pcie_user_write64(g_fd, TXDATA, slave_addr);

    offset &= 0xff;
    Pcie_user_write64(g_fd, TXDATA, offset);

    for(i = 0; i < length - 1; i++)
    {
        data[i] &= 0xff;
        Pcie_user_write64(g_fd, TXDATA, data[i]);
    }

    data[i] &= 0xff;
    data[i] |= TX_DONE_MASK;
    Pcie_user_write64(g_fd, TXDATA, data[i]);

    return length;
    
}

int pcie_i2c_read_swicth(uint32_t slave_addr, uint32_t * value)
{
    init_i2c_device();
    uint32_t data = slave_addr;
    data &= 0xff;
    data <<= 1;
    data |= TX_DONE_MASK | TX_ADDR_MASK | 0x1;
    Pcie_user_write64(g_fd, TXDATA, data);

    unsigned long long length;
    Pcie_user_read64(g_fd, RX_STATE, &length);
    length &= 0xffff;
    assert(length == 1);

    unsigned long long temp;
    Pcie_user_read64(g_fd, RX_DATA, &temp);
    if(temp & RX_DATA_VALID)
    {
        *value = temp;
        return 1;
    }
    else
    {
        return -1;
    }
    
}
int pcie_i2c_read(uint32_t slave_addr, uint32_t offset, uint32_t * data, int length)
{
    init_i2c_device();
    slave_addr &= 0xff;
    slave_addr <<= 0x1;
    slave_addr |= TX_ADDR_MASK | 0x1;
    Pcie_user_write64(g_fd, TXDATA, slave_addr);

    offset &= 0xff;
    offset |= TX_DONE_MASK;
    Pcie_user_write64(g_fd, TXDATA, offset);

    unsigned long long len;
    Pcie_user_read64(g_fd, RX_STATE, &len);
    len &= 0xffff;
    assert(len == 1);

    len = len < length ? len : length;


    for(int i = 0; i < len; i++)
    {
        unsigned long long temp;
        Pcie_user_read64(g_fd, RX_STATE, &temp);
        if(temp & RX_DATA_VALID)
        {
            data[i] = temp;
        }
        else
        {
            return -1;
        }
    }
    return len;
}
