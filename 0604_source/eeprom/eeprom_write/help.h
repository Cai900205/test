#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/time.h>
void usage()
{
    printf("[option]:  --time        Test time for this program.\n");
    printf("           --passes      Test times for this program.\n");
    printf("           --workers     Worker numbers of this program.(no use)\n");
    printf("           --bind        This program is or not bind.(no use)\n");
    printf("           --interval    Seconds to sleep between two times of test.\n");
    printf("           --i2c         I2c buses\n");
    printf("           --slave       Slave adress\n");
    printf("           --count       Read count \n");
    printf("           --word_offset 8bit read or 16bit read\n");
    printf("           --help        The help of this program.\n");
    printf("           --version     The version of this program.\n");
}
