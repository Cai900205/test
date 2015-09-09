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
/*#include <gxio/gpio.h>*/
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static int READ_BYTES = 64;

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

#if 0
static unsigned char bib_check[READ_BYTES] = {
    0x69, 0x73, 0x20, 0x50, 0x43, 0x49, 0x45, 0x20, 
    0x43, 0x61, 0x72, 0x64, 0x02, 0x00, 0x0f, 0x00, 
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x1c, 
    0x02, 0x01, 0x0f, 0x00, 0x02, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x51, 0x1c, 0x02, 0x00, 0x3a, 0x00, 

#if 0
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x02, 0x00, 0x38, 0x00, 0x7a, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x02, 0x10, 0x38, 0x00, 
#endif
};
#endif

static unsigned char * bib_check;

int main(int argc, char* argv[])
{
    int result = -1;
    int fd = 0;
    int passes = 1;
    int offset = 0;
    int i2c_master = 0;
    int slave_addr = 0;
    int interval = 1;
    int test_do = 1;
    int word_offset = 0;
    char time_str[30];
    int i;
    /** Parse the arguments. */
    for (i = 1; i < argc; i++)
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
        else if (!strcmp(arg, "--offset") && i + 1 < argc)
        {
            offset = htoi(argv[++i]);
        }
        else if (!strcmp(arg, "--i2c") && i + 1 < argc)
        {
            i2c_master = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--slave") && i + 1 < argc)
        {
            slave_addr = htoi(argv[++i]);
        }
        else if (!strcmp(arg, "--count") && i + 1 < argc)
        {
            READ_BYTES = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--word_offset"))
        {
            word_offset = 1;
        }
        else
        {
            printf("Unkown option: %s\n", arg);
            return -1;
        }
    }
    char* read_buffer = malloc(READ_BYTES);
    assert(read_buffer);
    bib_check = malloc(READ_BYTES);
    char * write_buffer = malloc(READ_BYTES + 2);
    memset(read_buffer, 0, READ_BYTES);

    for (i = 0; i < READ_BYTES; i++)
    {
        bib_check[i] = i;
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
        printf("[BIB-READ-%s]: Set the i2c address not 10 bits failed: %s\n", 
                time_str, strerror(errno));
        return -1;
    }
    
    /** 7-bit slave address 0x54. */
    result = ioctl(fd, I2C_SLAVE, slave_addr);
    if (result)
    {
        get_time(time_str);
        printf("[BIB-READ-%s]: Set the i2c address to 0x%02x failed: %s\n", 
                time_str, slave_addr, strerror(errno));
        return -1;
    }

    while (test_do <= passes)
    {
        if(word_offset == 1)
        {
#if 1
            /** We just write 64 bytes at the first of the rom. */
            write_buffer[0] = offset >> 8;
            write_buffer[1] = (offset & 0xFF); 

            memcpy((write_buffer + 2), bib_check, READ_BYTES);

            result = write(fd, write_buffer, READ_BYTES + 2);
            if (result != (READ_BYTES+2))
            {
                get_time(time_str);
                printf("[BIB-READ-%s]: Write the data offset to 0x%04x failed: %s\n", 
                        time_str, offset, strerror(errno));
                printf("line 181 result %d \n",result);
                return -1;                  
            }

            sleep(interval);
            /** Read the data back and check. */
            write_buffer[0] = offset >> 8;
            write_buffer[1] = (offset & 0xFF); 

            result = write(fd, write_buffer, 2);
            if (result != 2)
            {
                get_time(time_str);
                printf("[BIB-READ-%s]: Write the data offset to 0x%04x failed: %s\n", 
                        time_str, offset, strerror(errno));
                printf("line 195 result %d \n",result);
                return -1;        
            }    

            /** Read the data at the Zero offset. */
            result = read(fd, read_buffer, READ_BYTES);
            if (result != READ_BYTES)
            {
                get_time(time_str);
                printf("[BIB-READ-%s]: Read the data from 0x%04x[64] failed: %s\n", 
                        time_str, offset, strerror(errno));
                printf("line 206 result %d \n",result);
                return -1;         
            } 
#endif
#if 1 
            else 
            {
                for ( i = 0; i < READ_BYTES; i++)
                {
                    if(i && i % 8 == 0)
                    {
                        printf("\n");
                    }
                    printf("0x%02x ", ((unsigned char*)read_buffer)[i]);
                }
                printf("\n");

                for ( i = 0; i < READ_BYTES; i++)
                {
                    if (((unsigned char*)read_buffer)[i] != bib_check[i])
                    {
                        get_time(time_str);
                        printf("[BIB-READ-%s]: Check the data failed %d -- 0x%02x <---> 0x%02x\n", 
                                time_str, i, ((unsigned char*)read_buffer)[i], ((unsigned char *)bib_check)[i]);
                        return -1;                      
                    }
                }
            } 
#endif    

            test_do++;   
            sleep(interval);  

        }
        else
        {
            /** We just write 64 bytes at the first of the rom. */
            write_buffer[0] = offset;

            memcpy((write_buffer + 1), bib_check, READ_BYTES);

            result = write(fd, write_buffer, READ_BYTES + 1);
            if (result != READ_BYTES + 1)
            {
                get_time(time_str);
                printf("[BIB-READ-%s]: Write the data offset to 0x%04x failed: %s\n", 
                        time_str, offset, strerror(errno));
                return -1;                  
            }

            sleep(interval);
            /** Read the data back and check. */
            write_buffer[0] = offset;

            result = write(fd, write_buffer, 1);
            if (result != 1)
            {
                get_time(time_str);
                printf("[BIB-READ-%s]: Write the data offset to 0x%04x failed: %s\n", 
                        time_str, offset, strerror(errno));
                return -1;        
            }    

            /** Read the data at the Zero offset. */
            result = read(fd, read_buffer, READ_BYTES);
            if (result != READ_BYTES)
            {
                get_time(time_str);
                printf("[BIB-READ-%s]: Read the data from 0x%04x[READ_BYTES] failed: %s\n", 
                        time_str, offset, strerror(errno));
                return -1;         
            } 
#if 1 
            else 
            {
                for ( i = 0; i < READ_BYTES; i++)
                {
                    printf("0x%02d ", ((unsigned char*)read_buffer)[i]);
                }
                printf("\n");

                for ( i = 0; i < READ_BYTES; i++)
                {
                    if (((unsigned char*)read_buffer)[i] != bib_check[i])
                    {
                        get_time(time_str);
                        printf("[BIB-READ-%s]: Check the data failed %d-0x%02x\n", 
                                time_str, i, ((unsigned char*)read_buffer)[i]);
                        return -1;                      
                    }
                }
            } 
#endif    

            test_do++;   
            sleep(interval);  
                                                               
        }
    }

    get_time(time_str);
    printf("[BIB-READ-%s]: Read the data from 0x%04x[64] and check it success\n", 
            time_str, offset);

    return 0;
}
