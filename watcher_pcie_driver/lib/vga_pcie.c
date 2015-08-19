/*************************************************************************
	> File Name: vga_pcie.c
	> Author: 
	> Mail: 
	> Created Time: Wed 19 Nov 2014 09:06:01 PM CST
 ************************************************************************/
#include<stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "../include/common.h"
#include "../include/vga_pcie.h"
#define COMMON_EREAD        -2001
#define COMMON_EWRITE       -2002
int pcie_user_read64(int fd, int bar, int offset, unsigned long long* value)
{
	int rv = -1;
	iomsg_t iomsg;
	memset(&iomsg, 0, sizeof(iomsg_t));
	iomsg.bar = bar;
	iomsg.offset = offset;
	rv = ioctl(fd, IOCTL_READ_ULLONG, &iomsg);
	if (rv)
	{
		return COMMON_EREAD;
	}
	*value = iomsg.data;
	return 0;
}
int pcie_user_write64(int fd, int bar, int offset, unsigned long long value)
{
	int rv = -1;
	iomsg_t iomsg;
	memset(&iomsg, 0, sizeof(iomsg_t));
	iomsg.bar = bar;
	iomsg.offset = offset;
	iomsg.data = value;
	rv = ioctl(fd, IOCTL_WRITE_ULLONG, &iomsg);
	if (rv)
	{
		return COMMON_EWRITE;
	}
	return 0;
}
int pcie_user_read32(int fd, int bar, int offset, unsigned long* value)
{
	int rv = -1;
	iomsg_t iomsg;
	memset(&iomsg, 0, sizeof(iomsg_t));
	iomsg.bar = bar;
	iomsg.offset = offset;
	rv = ioctl(fd, IOCTL_READ_ULONG, &iomsg);
	if (rv)
	{
		return COMMON_EREAD;
	}
	*value = iomsg.data;
	return 0;
}
int pcie_user_write32(int fd, int bar, int offset, unsigned long value)
{
	int rv = -1;
	iomsg_t iomsg;
	memset(&iomsg, 0, sizeof(iomsg_t));
	iomsg.bar = bar;
	iomsg.offset = offset;
	iomsg.data = value;
	rv = ioctl(fd, IOCTL_WRITE_ULONG, &iomsg);
	if (rv)
	{
		return COMMON_EWRITE;
	}
	return 0;
}
