#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>


#define UART_DEVICE							"/dev/ttyS1"
#define BLOCK_SIZE								3

#define DATA_LEN 16

typedef struct _message_request
{
    uint8_t start;
    uint8_t slave_addr;
    uint8_t netfn_rslun;
    uint8_t head_checksum;
    uint8_t master_addr;
    uint8_t rqseq_rqlun;
    uint8_t command;
    uint8_t data;
    uint8_t tail_checksum;
    uint8_t stop;
}request_t;

typedef struct _message_response
{
    uint8_t start;
    uint8_t slave_addr;
    uint8_t netfn_rslun;
    uint8_t head_checksum;
    uint8_t master_addr;
    uint8_t rqseq_rqlun;
    uint8_t command;
    uint8_t complete;
    uint8_t data[DATA_LEN];
    uint8_t tail_checksum;
    uint8_t stop;
}response_t;

#define TEMP_CMD            0x0A

#define STATUS_IDLE         0
#define STATUS_START        1
#define STATUS_MASTER_ADDR  2
#define STATUS_FUNC         3
#define STATUS_CHECKSUM     4
#define STATUS_SLAVE_ADDR   5
#define STATUS_RQSEQ        6
#define STATUS_COMMAND      7
#define STATUS_COMPLETE     8
#define STATUS_DATA         9
#define STATUS_STOP         10


#define UART_START  0xA0
#define UART_STOP   0xA5
#define UART_MASTER_ADDR 0x51
#define UART_SLAVE_ADDR  0x93
#define UART_COMPLETE    0x00

static inline void print_buf(uint8_t * message,int length)
{ 
    int i;
    for( i = 0; i < length; i++)
    {                   
            if(i && i % 8 == 0)
            {               
                        printf("\n");
            }               
            printf("0x%02x ",message[i]);
        }                   
    printf("\n");       
                        
}  

static int message_send(int fd,uint8_t * buf, ssize_t len, uint8_t slave_addr,uint8_t master_addr,uint8_t command)
{
    assert(len == 1);
    request_t * message = malloc(sizeof(request_t)); 
    assert(message);

    message->start = UART_START;
    message->slave_addr = slave_addr;
    message->netfn_rslun = 0x18;
    message->head_checksum = 0x00;
    message->master_addr = master_addr;
    message->rqseq_rqlun = 0x00;
    message->command = command;
    message->data = *buf;
    message->tail_checksum = 0x00;
    message->stop = UART_STOP;

    int result = write(fd,message,sizeof(request_t));
    print_buf((uint8_t*)message,sizeof(request_t));

    free(message);

    return result;
}
static int message_recv(int fd,uint8_t * buf, ssize_t len, uint8_t slave_addr,uint8_t master_addr,uint8_t command)
{
    int state = STATUS_START;
    uint8_t recv_data;
    int result;
    bool return_flag = false;
    int i = 0;

    while(1)
    {
        usleep(10 * 1000);
		result = read(fd, &recv_data, 1);
        if(result < 0)
        {
            return -1;
        }
		assert(result == 1);
        printf("recv_data:%02x\n",recv_data);
        switch(state)
        {
            case STATUS_START:
            if(recv_data == UART_START) 
            {
                state = STATUS_MASTER_ADDR;    
            }
            break;

            case STATUS_MASTER_ADDR:
            if(recv_data == master_addr)
            {
                state = STATUS_FUNC;
            }
            else if(recv_data == UART_START)
            {
                state = STATUS_MASTER_ADDR;
            }
            else
            {
                state = STATUS_START;
            }
            break;

            case STATUS_FUNC:
            if(recv_data == UART_START) 
            {
                state = STATUS_MASTER_ADDR;    
            }
            else
            {
                state = STATUS_CHECKSUM;
            }
            break;

            case STATUS_CHECKSUM:
            if(recv_data == UART_START) 
            {
                state = STATUS_MASTER_ADDR;    
            }
            else
            {
                state = STATUS_SLAVE_ADDR;
            }
            break;

            case STATUS_SLAVE_ADDR:
            if(recv_data == slave_addr)
            {
                state = STATUS_RQSEQ;
            }
            else if(recv_data == UART_START)
            {
                state = STATUS_MASTER_ADDR;
            }
            else
            {
                state = STATUS_START;
            }
            break;

            case STATUS_RQSEQ:
            if(recv_data == UART_START) 
            {
                state = STATUS_MASTER_ADDR;    
            }
            else
            {
                state = STATUS_COMMAND;
            }
            break;

            case STATUS_COMMAND:
            if(recv_data == command)
            {
                state = STATUS_COMPLETE;
            }
            else if(recv_data == UART_START)
            {
                state = STATUS_MASTER_ADDR;
            }
            else
            {
                state = STATUS_START;
            }
            break;

            case STATUS_COMPLETE:
            if(recv_data == UART_COMPLETE)
            {
                state = STATUS_DATA;
            }
            else if(recv_data == UART_START)
            {
                state = STATUS_MASTER_ADDR;
            }
            else
            {
                state = STATUS_START;
            }
            break;

            case STATUS_DATA:
            if(recv_data == UART_START)
            {
                state = STATUS_MASTER_ADDR;    
            }
            else if(recv_data == UART_STOP)
            {
                result = 0;
                return_flag = true; 
            }
            else
            {
                if(i == len)
                {
                    state = STATUS_STOP;
                }
                buf[i++] = recv_data;
            }
            break;

            case STATUS_STOP:
            if(recv_data == UART_STOP)
            {
                return_flag = true;
                result = -1;
            }
            else if(recv_data == UART_START)
            {
                i = 0;
                state = STATUS_MASTER_ADDR;    
            }
            break;

                      
        }

        if(return_flag == true)
        {
            return result;
        }
    }
}

static void get_time(char *str_t)
{
    time_t now;
    char str[30];
    memset(str,0,sizeof(str));
    time(&now);
    strftime(str,30,"%Y-%m-%d %H:%M:%S",localtime(&now));
    int len = strlen(str);
    str[len] = '\0';
    strcpy(str_t,str);
}

int init_uart_device()
{
	int fd = -1;
    int result = -1;
	char* device = UART_DEVICE;
	struct termios ts;
	char time_str[30];
	int rbaud = 115200;
	/** Open the device. */
	fd = open(device, O_RDWR);
	if (fd < 0)
	{
		get_time(time_str);
		printf("[Console-host-%s]: Open the device %s failed: %s\n",
				time_str, device, strerror(errno));
		return -1;
	}

	/** Set the speed to 115200. */
	result = tcgetattr(fd, &ts);
	if (result)
	{
		get_time(time_str);
		printf("[Console-host-%s]: Get the attribute of device %s failed: %s\n",
				time_str, device, strerror(errno));
		return -1;		
	}

	tcflush(fd, TCIOFLUSH);
	
	result = cfsetispeed(&ts, B115200); 
	if (result)
	{
		get_time(time_str);
		printf("[Console-host-%s]: Set the input Baul rate %d failed: %s\n",
				time_str, rbaud, strerror(errno));
		return -1;
	}

	result = cfsetospeed(&ts, B115200); 
	if (result) 
	{
		get_time(time_str);
		printf("[Console-host-%s]: Set the output Baul rate %d failed: %s\n",
				time_str, rbaud, strerror(errno));
		return -1;
	}

	tcsetattr(fd, TCSANOW, &ts);

	tcflush(fd,TCIOFLUSH);

	/** Set parity of the uart. */
	result = tcgetattr(fd, &ts);
	assert(!result);

	ts.c_cflag &= ~(CSIZE | PARENB | CRTSCTS | CSTOPB);
    ts.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL | INPCK);
    ts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | NOFLSH);
    ts.c_oflag &= ~(OPOST | ONLCR | OCRNL);

    ts.c_cflag |= (CREAD | CS8);
    tcflush(fd,TCIFLUSH);
    ts.c_cc[VTIME] = 0; 
    ts.c_cc[VMIN] = BLOCK_SIZE;
	tcsetattr(fd, TCSANOW, &ts);	

	get_time(time_str);
	printf("[Console-host-%s]: Host is ready to test the rbaud: %d\n",
			time_str, rbaud);

    return fd;

}

int get_temperature(int fd,int times, int * cpu_tp,int * board_tp)
{
	int result = -1;
	fd_set readfds;
	char time_str[30];
	struct timeval timeout;
    uint8_t temperature[4] = {0};

    uint8_t buf = 0x0;
    result = message_send(fd,&buf,1,UART_SLAVE_ADDR,UART_MASTER_ADDR,TEMP_CMD);
	if (result < 0)
	{
		get_time(time_str);
		printf("[Console-host-%s]: Host send the %d-nth packets failed: %s\n",
				time_str, times, strerror(errno));
		return -1;			
	}

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	result = select(fd + 1, &readfds, NULL, NULL, &timeout);
	if (result == -1)
	{
		get_time(time_str);
		printf("[Console-host-%s]: Host monitor the read file error: %s\n",
				time_str, strerror(errno));
		return -1;
	}
	else if (result == 0)
	{
		get_time(time_str);
		printf("[Console-host-%s]: Host read the packets-%d timeout\n",
				time_str, times + 1);
		return -1;
	}
	else if (result == 1)
	{
        result = message_recv(fd,temperature,4,UART_SLAVE_ADDR,UART_MASTER_ADDR,TEMP_CMD);
		assert(result == 0);
	}

    * cpu_tp = temperature[0];
    * board_tp = (temperature[1] + temperature[2]) / 2;

    return 0;
}



