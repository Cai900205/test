#ifndef L_COMMON_H
#define L_COMMON_H

typedef struct 
{
    int bar;
    int offset;
    unsigned long long data;
}iomsg_t, * piomsg_t;
#define GENERIC_PCIE_TYPE                0xA8
#define IOCTL_READ_ULLONG                _IOWR(GENERIC_PCIE_TYPE,1,piomsg_t)
#define IOCTL_WRITE_ULLONG               _IOWR(GENERIC_PCIE_TYPE,2,piomsg_t)
#define IOCTL_READ_ULONG                 _IOWR(GENERIC_PCIE_TYPE,3,piomsg_t)
#define IOCTL_WRITE_ULONG                _IOWR(GENERIC_PCIE_TYPE,4,piomsg_t)

#endif
