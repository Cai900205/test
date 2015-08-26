#include "fvl_ssd.h"

int fvl_ssd_open(const char *pathname)
{
    int fd;
    fd = open(pathname,O_CREAT|O_RDWR|O_DIRECT|O_SYNC,S_IRUSR|S_IWUSR);
    if(fd <0)
    {
        printf("failed to open or create file!\n");
        return -1;
    }

    return fd;
}

int fvl_ssd_write(int fd,const void *buf,unsigned int count)
{
    unsigned int ret;
    ret = write(fd,buf,count);
    if(ret != count)
    {
        printf("failed to write!\n");
        return -1;
    }

    return 0;
}

unsigned int  fvl_ssd_read(int fd,void *buf,unsigned int count)
{
    unsigned int ret;
    ret = read(fd,buf,count);
    if(ret<0)
    {
        printf("failed read file!\n");
        return 0;
    }

    return ret;
}

int fvl_ssd_close(int fd)
{
    int ret;
    ret=close(fd);
    if(ret<0)
    {
        printf("failed close file!\n");
        return -1;
    }

    return ret;
}
