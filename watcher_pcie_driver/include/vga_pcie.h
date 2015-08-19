#ifndef SIRIUS_PCIE_H
#define SIRIUS_PCIE_H

#define SIRIUS_PCIE_NAME "/dev/fpga_pcie1"


#define GENERIC_PCIE_BAR_INDEX 0

int pcie_user_read64(int fd,int bar, int offset, unsigned long long* value);
int pcie_user_write64(int fd, int bar, int offset, unsigned long long value);
int pcie_user_read32(int fd, int bar, int offset, unsigned long* value);
int pcie_user_write32(int fd, int bar, int offset, unsigned long value);

#endif
