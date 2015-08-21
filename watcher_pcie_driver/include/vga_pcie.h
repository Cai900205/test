#ifndef VGA_PCIE_H
#define VGA_PCIE_H

#define VGA_PCIE2_NAME "/dev/vga_pcie2"
#define VGA_PCIE1_NAME "/dev/vga_pcie1"


#define GENERIC_PCIE_BAR_INDEX 0

int pcie_user_read64(int fd,int bar, int offset, unsigned long long* value);
int pcie_user_write64(int fd, int bar, int offset, unsigned long long value);
int pcie_user_read32(int fd, int bar, int offset, unsigned long* value);
int pcie_user_write32(int fd, int bar, int offset, unsigned long value);

#endif
