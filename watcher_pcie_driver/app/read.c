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
    unsigned long addr;
    unsigned long data; //write by ...
    unsigned long data_back;
    help();
    int g_handle = open(VGA_PCIE1_NAME, O_RDWR);
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
    }
    data_back  = 0;
    data = 0x30;
    addr = 0x0;
//    if (pcie_user_write32(g_handle,bar, addr, data) != 0)
  //  {
    //    printf("write bar: 0x%02x , addr: 0x%016llx value : 0x%016llx error\n", bar, addr, data);
  //  }
    if (pcie_user_read32(g_handle, bar, addr, &data_back) != 0)
    {
        printf("read bar: 0x%02x , addr: 0x%016llx error\n", bar, addr);
    }
    if(data_back != data)
    {
        printf("################# %x read %x and write %x is different #################\n",addr,data_back,data);
    }
    printf("################# 0x%x read and write 0x%x is ok #################\n",addr,data);
    return 0;
}
