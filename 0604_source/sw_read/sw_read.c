#include "help.h"

#define READ_BYTES                      64

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

static uint64_t htoi(char s[])  
{  
    int i;  
    uint64_t n = 0;  
    if (s[0] == '0' && (s[1]=='x' || s[1]=='X'))  
    {  
        i = 2;  
    }  
    else  
    {  
        i = 0;  
    }  
    for (; (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') || (s[i] >='A' && s[i] <= 'Z');++i)  
    {  
        if (tolower(s[i]) > '9')  
        {  
            n = 16 * n + (10 + tolower(s[i]) - 'a');  
        }  
        else  
        {  
            n = 16 * n + (tolower(s[i]) - '0');  
        }  
    }  

    return n;  
} 

int main(int argc, char* argv[])
{
    int result = -1;
    int fd = 0;
    int passes = 1;
    int i2c_master = 0;
    int test_do = 1;
    int slave_addr = 0;
    int interval = 1;
    int time=10;
    int workers=1;
    int maxworkers=1;
    int bind=0;
    char time_str[30];
    char* read_buffer = malloc(READ_BYTES);
    assert(read_buffer);
    memset(read_buffer, 0, READ_BYTES);
    uint8_t reg = 0;
    uint8_t write_reg = 2;
    int i=0;
    /** Parse the arguments. */
    for ( i = 1; i < argc; i++)
    {
        char* arg = argv[i];

        if (!strcmp(arg, "--passes") && i + 1 < argc)
        {
            passes = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--interval") && i + 1 < argc)
        {
            interval = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--i2c") && i + 1 < argc)
        {
            i2c_master = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--slave") && i + 1 < argc)
        {
            slave_addr = htoi(argv[++i]);
        }
        else if (!strcmp(arg, "--time") && i + 1 < argc)
        {
            time = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--workers") && i + 1 < argc)
        {
            workers = atoi(argv[++i]);
            if(workers!=maxworkers)
            {
                 printf("Worker error!\n");
                 return -1;
            }
        }
        else if (!strcmp(arg, "--bind"))
        {
            bind=1;
        }
        else if (!strcmp(arg, "--help") )
        {
            usage();
            return 0;
        }
        else if (!strcmp(arg, "--version"))
        {
            printf("SW_TEST VERSION:POWERPC-SW_TEST-V1.00\n");
            return 0;
        }
        else
        {
            printf("Unkown option: %s\n", arg);
            usage();
            return -1;
        }
    }

    char i2c_dev[20];
    sprintf(i2c_dev, "/dev/i2c-%d", i2c_master);
    fd = open(i2c_dev, O_RDWR);
    assert(fd >= 0); 
    
    /** Select slave address 7-bit. */
    result = ioctl(fd, I2C_TENBIT, 0);
    if (result)
    {
        get_time(time_str);
        printf("[SW-TEST-%s]: Set the i2c address not 10 bits failed: %s\n", 
                time_str, strerror(errno));
        return -1;
    }
    
    /** 7-bit slave address 0x54. */
    result = ioctl(fd, I2C_SLAVE, slave_addr);
    if (result)
    {
        get_time(time_str);
        printf("[SW-TEST-%s]: Set the i2c address to 0x%02x failed: %s\n", 
                time_str, slave_addr, strerror(errno));
        return -1;
    }
    result = write(fd,&write_reg,1);
    if(result != 1)
    {
        printf("write error!\n");
        return -1;
    }
    double diff=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);

    while (test_do <= (passes + 1))
    {
        result = read(fd, read_buffer, 1);
        if (result != 1)
        {
            get_time(time_str);
            printf("[SW-TEST-%s]: Read the register of the i2c switch(0x%02x) failed: %s\n",
                    time_str, slave_addr, strerror(errno));
            return -1;
        }      
        else
        {
            if (test_do == 1)
            {
                reg = read_buffer[0];
            }
            else 
            {
                if (write_reg != read_buffer[0])
                {
                    get_time(time_str);
                    printf("[SW-TEST-%s]: Check the register of the i2c switch(0x%02x) failed: %s\n",
                            time_str, slave_addr, strerror(errno));
                    return -1;                    
                }
            } 
        }  

        gettimeofday(&tm_end,NULL);
        diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if(diff>time)
        {
            break;
        }

        test_do++;
        sleep(interval);
    }

    get_time(time_str);
    printf("[SW-TEST-%s]: Check the access to i2c switch(0x%02x) success\n", 
            time_str, slave_addr);

    return 0;
}
