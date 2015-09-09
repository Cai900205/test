#ifndef PSOC_TEMP_H
#define PSOC_TEMP_H

int init_uart_device();
int get_temperature(int fd,int times, int * cpu_tp,int * board_tp);
#endif
