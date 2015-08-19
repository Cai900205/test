#ifndef L_PCIE_I2C_H
#define L_PCIE_I2C_H
#include <stdint.h>
int pcie_i2c_write_switch(int slave_addr, uint32_t data);
int pcie_i2c_write(uint32_t slave_addr, uint32_t offset, uint32_t * data, int length);

int pcie_i2c_read_swicth(uint32_t slave_addr, uint32_t * value);
int pcie_i2c_read(uint32_t slave_addr, uint32_t offset, uint32_t * data, int length);
int init_i2c_device(void);
#endif
