/*
 * ./app_test cmd gpio_number bit_number led_times
 * 
 * gpio_test
 *
 */

#include "help.h"
#define DEVICE_NAME "/dev/gpio_test"

#define INPUT  0
#define SET 1
#define CLEAR 2
#define TOGGLE 3

 struct param
 {
    unsigned char gpionum;
    unsigned char bitnum;
    unsigned char  value;
 };

int main(int argc,char **argv)
{
    int fd;
    int ret;
    int cmd=0,gpio_number=-1,bit_number=-1;
    uint64_t test_times=0,i=0,delay=0;
    int time=10,workers=1,bind=0,interval=1;
    struct param ioargs;
    int maxworkers=1;
    for(i=1;i<argc;i++)
    {
        char *arg=argv[i];
        if(!strcmp(arg,"--cmd")&&i+1<argc)
        {
           cmd = atoi(argv[++i]);
           if(cmd <0 || cmd >2)
           {
                printf("Operation cmd error!\n");
                return -1;
           }
        }
        else if(!strcmp(arg,"--gpionum")&&i+1<argc)
        {
           gpio_number = atoi(argv[++i]);
           if(gpio_number<0 || gpio_number >3)
           {
                printf("GPIO NUMBER error!\n");
                return -1;
           }
        }
        else if(!strcmp(arg,"--bitnum")&&i+1<argc)
        {
           bit_number = atoi(argv[++i]);
           if(bit_number <0 || bit_number >31)
           {
                printf("GPIO BITNUMBER error!\n");
                return -1;
           }
        }
        else if(!strcmp(arg,"--passes")&&i+1<argc)
        {
           test_times = strtoul(argv[++i],NULL,10);
        }
        else if(!strcmp(arg,"--interval")&&i+1<argc)
        {
           interval = strtoul(argv[++i],NULL,10);
        }
        else if(!strcmp(arg,"--time")&&i+1<argc)
        {
           time = strtoul(argv[++i],NULL,10);
        }
        else if(!strcmp(arg,"--workers")&&i+1<argc)
        {
           workers = strtoul(argv[++i],NULL,10);
           if(workers!=maxworkers)
           {
                printf("workers error!\n"); 
	            return -1;
           }
        }
        else if(!strcmp(arg,"--bind"))
        {
           bind=1;
        }
        else if(!strcmp(arg,"--help"))
        {
            usage();
            return 0;
        }else if(!strcmp(arg,"--version"))
        {
            printf("GPIO_APP VERSION:POWERPC-GPIO_APP-V1.00\n");
            return 0;
        }else
        {
            printf("Unkown option: %s\n",arg);
            usage();
            return -1; 
        }
    }

    ioargs.gpionum=gpio_number;
    ioargs.bitnum = bit_number;
    ioargs.value = -1;
    fd = open(DEVICE_NAME, O_RDWR);
    int value;
    if (fd == -1)
    {
	    printf("open device %s error\n",DEVICE_NAME);
        return -1;
    }
    delay = 400000;
    double diff=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
    if(1 == cmd)
    {
        for(i=0;i<test_times;i++)
        {
	       ioctl(fd,SET,&ioargs);
	       usleep(delay);
	       ioctl(fd,CLEAR,&ioargs);
	       usleep(delay);
               gettimeofday(&tm_end,NULL);
               diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
               if(diff>time)
               {
                 break;
               }
        }
    }
    else if( 2== cmd )
    {
	    ioctl(fd,SET,&ioargs);
    }
    else if( 0 == cmd )
    {
	    ioctl(fd,CLEAR,&ioargs);
    }
    ret = close(fd);
    
    return 0;
}
