/*************************************************************************
	> File Name: watcher.c
	> Author: 
	> Mail: 
	> Created Time: Tue 31 Mar 2015 04:04:56 PM CST
 ************************************************************************/
/* common headers*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
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
#include "../include/sirius_pcie.h"
#include "../include/common.h"


#define TEST_TIMES 20000
#define TEST_LENGTH 4096


int main(int argc, char ** argv)
{
    uint32_t size = 0x10000;
    int result = -1;
    char * dev = NULL;
    for(int i = 1; i < argc; i++)
    {
        char * arg = argv[i];
        if(!strcmp(arg,"--size") && i + 1 < argc)
        {
            size = atoi(argv[++i]);
        }
        if(!strcmp(arg,"--dev") && i + 1 < argc)
        {
            dev = argv[++i];
        }
        
    }
    int i = 0;
    int fd = open(dev, O_RDWR);
    if (fd < 0)
    {
        printf("Can not open the board.\n");
        return -4;
    }
    result = ioctl(fd, IOCTL_DMA_WRITE, 0);
    if(result != 0)
    {
        printf("Wathcer DMA WRITE AND READ test Failed.\n");
        return -3;
    }
    else
    {
        printf("Wathcer DMA WRITE AND READ test OK.\n");
    }

    struct timeval old,new;

    gettimeofday(&old,NULL);
    while(i++ < TEST_TIMES)
    {
        result = ioctl(fd, IOCTL_SPEED_DMA_WRITE, 0);
        if(result != 0)
        {
            printf("Wathcer DMA WRITE test Failed.\n");
            return -3;
        }
    }
    gettimeofday(&new,NULL);
    uint64_t period = -(old.tv_usec + old.tv_sec * 1000000)  + (new.tv_usec + new.tv_sec * 1000000);
    double delay = period / 1000000.0;
    printf("[WATCHER-DMA] WRITE speed %.2f Mbps\n",(TEST_TIMES * TEST_LENGTH / delay) / 125000);
    printf("Wathcer DMA WRITE test OK.\n");

    i = 0;
    gettimeofday(&old,NULL);
    while(i++ < TEST_TIMES)
    {
        result = ioctl(fd, IOCTL_SPEED_DMA_READ, 0);
        if(result != 0)
        {
            printf("Wathcer DMA READ test Failed.\n");
            return -2;
        }
    }
    gettimeofday(&new,NULL);
    period = -(old.tv_usec + old.tv_sec * 1000000)  + (new.tv_usec + new.tv_sec * 1000000);
    delay = period / 1000000.0;
    printf("[WATCHER-DMA] READ speed %.2f Mbps\n",(TEST_TIMES * TEST_LENGTH / delay) / 125000);
    printf("Wathcer DMA READ test OK.\n");

    return 0;
}
