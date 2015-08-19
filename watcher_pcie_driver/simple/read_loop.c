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
#include "../include/vga_pcie.h"
void help()
{
    printf("##=====support cmd ==============\n");
    printf("input dev can be flowings\n");
    printf("1: %s\n", RCMD);
    printf("2: %s\n", WCMD);
}
int main(int argc, char ** argv)
{
    unsigned int bar = 0;
    unsigned int max_addr = 0;
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

    for(int i = 1; i < argc; i++)
    {
        char * arg = argv[i];
        if(!strcmp(arg, "--bar") && i + 1 < argc)
        {
            bar = atoi(argv[++i]);
        }
        else if(!strcmp(arg, "--max_addr") && i + 1 < argc)
        {
            max_addr = atoi(argv[++i]);
        }
    }
    data_back  = 0;
    data = 0x30;
    addr = 0x0;
    printf("max_addr: 0x%x\n",max_addr);
    while (addr < max_addr )
    {
        if (pcie_user_write64(g_handle,bar, addr, data) != 0)
        {
             printf("write bar: 0x%02x , addr: 0x%016llx value : 0x%016llx error\n", bar, addr, data);
        }
        if (pcie_user_read64(g_handle, bar, addr, &data_back) != 0)
        {
             printf("read bar: 0x%02x , addr: 0x%016llx error\n", bar, addr);
        }
        if(data_back != data)
        {
            printf("################# %x read and write %x is different #################\n",addr,data);
        }
        data += 0x55;
        addr += 1024;
    }
    printf("################# 0x%x read and write 0x%x is ok #################\n",addr,data);
    return 0;
}
