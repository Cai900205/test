/*
 *author:ctx
 *date:2014/11/19
 *
 */

#ifndef __FVL_SSD_H__
#define __FVL_SSD_H__

#include "fvl_common.h"
#include  <sys/types.h>
#include  <sys/stat.h>
#define __USE_GNU
#include  <fcntl.h>
#include  <unistd.h>

int fvl_ssd_open(const char *pathname);
int fvl_ssd_write(int fd,const void *buf,unsigned int count);
unsigned int fvl_ssd_read(int fd,void *buf,unsigned int count);
int fvl_ssd_close(int fd);
#endif
