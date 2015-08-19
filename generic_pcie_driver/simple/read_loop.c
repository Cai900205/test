/******************************************************************************
*				Include file
******************************************************************************/
/* common headers*/
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
#include <netdb.h>
#include <malloc.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#define RCMD	"R %02x %016llx"
#define WCMD	"W %02x %016llx %016llx"
#include "../include/sirius_pcie.h"
void help()
{
    printf("##=====support cmd ==============\n");
    printf("input dev can be flowings\n");
    printf("1: %s\n", RCMD);
    printf("2: %s\n", WCMD);
}
int main()
{
    unsigned int bar;
    unsigned long long addr;
    unsigned long long data; //write by ...
    unsigned long long data_back;
    help();
    int g_handle = open(SIRIUS_PCIE_NAME, O_RDWR);
    if (g_handle < 0)
    {
        printf("Can not open the board.\n");
        return 0;
    }
    bar = 0;
    data_back  = 0;
    data = 0x30;
    addr = 0xea8;
    while (1)
    {
        if (pcie_user_write64(g_handle, addr, data) != 0)
        {
             printf("write bar: 0x%02x , addr: 0x%016llx value : 0x%016llx error\n", bar, addr, data);
        }
        else
        {
             printf("write bar: 0x%02x , addr: 0x%016llx value: 0x%016llx success\n", bar, addr, data);
        }
        if (pcie_user_read64(g_handle, addr, &data) != 0)
        {
             printf("read bar: 0x%02x , addr: 0x%016llx error\n", bar, addr);
        }
        else
        {
             printf("read bar: 0x%02x , addr: 0x%016llx value: 0x%016llx\n", bar, addr, data);
        }
        if(data_back != data)
        {
            printf("################# read and write is different #################\n");
        }
        data += 0x55;
    }
    return 0;
}
