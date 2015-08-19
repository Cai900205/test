/*************************************************************************
	> File Name: fpga_mem_test.c
	> Author: 
	> Mail: 
	> Created Time: Wed 10 Dec 2014 11:12:31 AM CST
 ************************************************************************/
#include <stdio.h>
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
#include <pthread.h>
#include <netdb.h>
#include <malloc.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include "../include/sirius_pcie.h"
#define PTTRN_SEL_MASK   0x7
#define RND_LNGTH_MASK   0x300
#define REPORT_MASK      0x3
#define END_AND_NOFAULT  0x2
#define END_AND_FAULT    0x3
#define FPGA_REVERSION   0x0000
#define DDR3_PTTRN_SEL   0x1008
#define DDR3_PTTRN_CFG_0 0x1010
#define DDR3_RND_LNGTH   0x1018
#define DDR3_REPORT      0x10B0
#define DDR3_ERRADDR     0x1020
#define DDR3_ERRDAT_0    0x1028
#define DDR3_EXPDAT_0    0x1068

#define SET_TEST_STATE     0x200
#define RESET_TEST_STATE   0x0
static inline void PCIE_user_write64(int fd, unsigned long long addr, uint64_t data)
{
    if(pcie_user_write64(fd, addr, data) != 0)
    {
        printf("write pcie regfile error.\n");
        exit(-1);
    }
}
static void PCIE_user_read64(int fd, unsigned long long addr, unsigned long long * data)
{
    if(pcie_user_read64(fd, addr, data) != 0)
    {
        printf("read pcie regfile error.\n");
        exit(-1);
    }
}
static void set_random_pattern_cfg(int fd, int i);
static void set_alter_pattern_cfg(int fd, int i);
typedef struct 
{
    char * describe;
    int    mode;
    int   do_or_not;
    void (* set_pattern_cfg)(int fd, int i);
}pattren_t;
pattren_t patterns[] = 
{
    {"Random number", 0,1,set_random_pattern_cfg},
    {"One-hot-way",    1,1,NULL},
    {"Zero-hot-way",     2,1,NULL},
    {"Alternating RW",3,1,set_alter_pattern_cfg},
    {"BAR2_DMA",      4,0,},
    {NULL,0},
};
static uint64_t * randnums;
static uint64_t alternums[][2] = 
{
    {0x0000000000000000UL, 0xffffffffffffffffUL},
    {0x3333333333333333UL, 0xccccccccccccccccUL},
    {0x9999999999999999UL, 0x6666666666666666UL},
    {0x5555555555555555UL, 0xaaaaaaaaaaaaaaaaUL}
};
static uint32_t err_cnt;
static uint32_t err_addr;
static int init_randnums(int passes)
{
    srand((unsigned int)time(NULL));
    randnums = malloc(sizeof(uint64_t) * passes * 2);
    for(int i = 0; i < passes * 2; i++)
    {
        randnums[i] = rand();
        randnums[i] = randnums[i] << 32 | rand();
    }
    return 0;
}
static void set_random_pattern_cfg(int fd, int i)
{
   PCIE_user_write64(fd, DDR3_PTTRN_CFG_0, randnums[i * 2]);
   PCIE_user_write64(fd, DDR3_PTTRN_CFG_0, randnums[i * 2 + 1]);
}
static void set_alter_pattern_cfg(int fd, int i)
{
   PCIE_user_write64(fd, DDR3_PTTRN_CFG_0, alternums[i % 4][0]);
   PCIE_user_write64(fd, DDR3_PTTRN_CFG_0, alternums[i % 4][1]);
}
static void dump_fault_memory(int fd)
{
    unsigned long long data;
    unsigned long long err_data;
    unsigned long long exp_data;
    PCIE_user_read64(fd, DDR3_ERRADDR, &data);
    err_cnt  = data >> 32;
    err_addr = data & 0xffffffff; 
    

    for(int i = 0; i < 3 ; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            PCIE_user_read64(fd, DDR3_ERRDAT_0 + j * 8, &err_data);
            PCIE_user_read64(fd, DDR3_EXPDAT_0 + j * 8, &exp_data);
            printf("[%08x] 0x%016llx  ->  0x%016llx\n",err_addr, err_data, exp_data);
            err_addr += 8;
        }
        PCIE_user_read64(fd, DDR3_ERRADDR, &data);
        err_addr = data & 0xffffffff; 
    }
}
int main(int argc, char ** argv)
{
    int fd = -1;
    int passes = 3;
    unsigned long long data;

    for(int i = 0; i < argc; i++)
    {
        char * arg = argv[i];
        if(!strcmp(arg, "--passes") && i + 1 < argc)
        {
            passes = atoi(argv[++i]);
        }
    }
    init_randnums(passes);
    /*
     * 0. open the board.
     * */
    fd = open(SIRIUS_PCIE_NAME, O_RDWR);
    if(fd < 0)
    {
        printf("Can not open the board.\n");
        return 0;
    }
    /*
     * 1. read the fpga version and display.
     * */
    
    PCIE_user_read64(fd, FPGA_REVERSION, &data);
    printf("The FPGA revesion is : 0x%016llx\n",data);
    /*
     * 2. set the test pattren.
     * */
    for(int i = 0; patterns[i].describe != NULL; i++)
    {
        if(!patterns[i].do_or_not)
        {
            continue;
        }
        for(int j = 0; j < passes; j++)
        {
            PCIE_user_write64(fd, DDR3_PTTRN_SEL, patterns[i].mode);
            if(patterns[i].set_pattern_cfg != NULL)
            {
                patterns[i].set_pattern_cfg(fd, j);
            }
            /* Start Test ...*/
            data = SET_TEST_STATE;
            PCIE_user_write64(fd, DDR3_RND_LNGTH, data);
            printf("Start Test:[%s] passes: %d\n",patterns[i].describe,j);
            data = 0x0;
            while(((data & REPORT_MASK) != END_AND_NOFAULT) && ((data & REPORT_MASK) != END_AND_FAULT))
            {
                PCIE_user_read64(fd, DDR3_REPORT, &data);
                //printf("DDR3_REPORT: 0x%16llx\n",data);
                sleep(1);
            }
            switch(data & REPORT_MASK)
            {
                case END_AND_NOFAULT:
                    printf("End   Test:[%s] passes: %d SUCCESSED\n",patterns[i].describe,j);
                    break;
                case END_AND_FAULT:
                    printf("End   Test:[%s] passes: %d FAILED\n",patterns[i].describe,j);
                    dump_fault_memory(fd);
                    break;
                default:
                    printf("Impossibity state\n");
                    return -1;
                    break;
            }
            data = RESET_TEST_STATE;
            PCIE_user_write64(fd, DDR3_RND_LNGTH, data);
            usleep(10 * 1000);
            PCIE_user_write64(fd, DDR3_RND_LNGTH, data);
            usleep(10 * 1000);
            PCIE_user_write64(fd, DDR3_RND_LNGTH, data);
            usleep(10 * 1000);
        }
    }
    /*
     * 3. 
     * */
}
