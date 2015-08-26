#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
#include <readline.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>

#define SEND_THREAD_NUM 2
#define RECEIVE_THREAD_NUM SEND_THREAD_NUM	/*ctx add*/
#define SRIO_SYS_ADDR		0x10000000	/* used for srio system addr */
#define SRIO_SYS_ADDR1		0x20000000	/* used for srio system addr */
#define SRIO_WIN_SIZE		0x1000000
#define THREAD_WIN_SIZE		(SRIO_WIN_SIZE/SEND_THREAD_NUM) /*ctx add*/
#define SRIO_POOL_PORT_SECT_NUM 4
#define SRIO_CMD_MIN_NUM	3
#define SRIO_POOL_PORT_OFFSET\
	(SRIO_WIN_SIZE * SRIO_POOL_PORT_SECT_NUM)
#define SRIO_POOL_SECT_SIZE	SRIO_WIN_SIZE /*POOLSIZE*/
#define SRIO_POOL_SIZE	0x8000000  /*dma pool size 64M*/
#define TEST_CMD_NUM		5
/*ctx add*/
#define SEND_NUM_OFFSET 0x7ffb00 /*reserve */
#define PACKET_LENGTH 32768	/*packet length*/
#define BUFFER_NUM 200		/*buffer number*/
#define SEND_TOTAL_NUM 100	/*SEND_NUMber*/


void usage()
{
    printf("[option]:  --time        Test time for this program.(no use)\n");
    printf("           --passes      Test times for this program.\n");
    printf("           --workers     Worker numbers of this program.(no use)\n");
    printf("           --bind        This program is or not bind.(no use)\n");
    printf("           --interval    Seconds to sleep between two times of test.(no use)\n");
    printf("           --test_type   The type of the test.\n");
    printf("           --data_type   The type of the data packet.\n");
    printf("           --help        The help of this program.\n");
    printf("           --version     The version of this program.\n");
}
