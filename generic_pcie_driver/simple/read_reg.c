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
#include <pthread.h>
#include <netdb.h>
#include <malloc.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#define RCMD	"R%d %02x %016llx"
#define WCMD	"W%d %02x %016llx %016llx"
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
    char buffer[1024];
    unsigned int bar;
    unsigned long long addr;
    unsigned long long data; //write by ...
    help();
    int g_handle = open(SIRIUS_PCIE_NAME, O_RDWR);
    if (g_handle < 0)
    {
        printf("Can not open the board.\n");
        return 0;
    }
    int flags;
    while (1)
    {
        if (NULL != fgets(buffer, 1024, stdin))
        {
            if (sscanf(buffer, RCMD,&flags, &bar, &addr) ==3)
            {
                if(flags == 64)
                {
                    if (pcie_user_read64(g_handle, addr, &data) != 0)
                    {
                        printf("read bar: 0x%02x , addr: 0x%016llx error\n", bar, addr);
                    }
                    else
                    {
                        printf("read bar: 0x%02x , addr: 0x%016llx value: 0x%016llx\n", bar, addr, data);
                    }
                }
                else if(32  == flags)
                {
                    if (pcie_user_read32(g_handle, addr, &data) != 0)
                    {
                        printf("read bar: 0x%02x , addr: 0x%016llx error\n", bar, addr);
                    }
                    else
                    {
                        printf("read bar: 0x%02x , addr: 0x%016llx value: 0x%016llx\n", bar, addr, data);
                    }
                }
                else
                {
                    printf("input error cmd\n");
                    help();
                }
            }
            else if (sscanf(buffer, WCMD, &flags, &bar, &data, &addr) == 4)
            {
                if(flags == 64)
                {
    		        printf("----------The buffer is:%s\n", buffer);
    		        printf("----------The addr is:0x%016llx\n", addr);
                    if (pcie_user_write64(g_handle, addr, data) != 0)
                    {
                        printf("write bar: 0x%02x , addr: 0x%016llx value : 0x%016llx error\n", bar, addr, data);
                    }
                    else
                    {
                        printf("write bar: 0x%02x , addr: 0x%016llx value: 0x%016llx success\n", bar, addr, data);
                    }
                }
                else if(32  == flags)
                {
    		        printf("----------The buffer is:%s\n", buffer);
    		        printf("----------The addr is:0x%016llx\n", addr);
                    if (pcie_user_write32(g_handle, addr, data) != 0)
                    {
                        printf("write bar: 0x%02x , addr: 0x%016llx value : 0x%016llx error\n", bar, addr, data);
                    }
                    else
                    {
                        printf("write bar: 0x%02x , addr: 0x%016llx value: 0x%016llx success\n", bar, addr, data);
                    }
                }
                else
                {
                    printf("input error cmd\n");
                    help();
                }
            }
            else
            {
                printf("input error cmd\n");
                help();
            }
        }
        else
        {
            printf("get error cmd quit\n");
            return -1;
        }
    }
    return 0;
}
