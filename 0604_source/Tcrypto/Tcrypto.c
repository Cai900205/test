#include "help.h"


int main(int argc, char** argv)
{
    int result = -1;
    int fd;
    int interval = 1;
    int passes = 1;
    int time=10;
    int workers=1;
    int bind=0;
    unsigned char access;
    unsigned char pkey;
    char recv[256]; 
    char crypto_check[256];   
    char write[256];
    cm_t cm;
    int maxworkers=1;
    
    int crypto = 0; // Crypto the user zone
    int verify = 0; // Verify password before dumping the user zone or not
    int i;    
    for ( i = 1; i < argc; i++)
    {
	    char* arg = argv[i];
 
        if (!strcmp(arg, "--crypto"))
        {
            crypto = 1;
        }
        else if (!strcmp(arg, "--verify"))
        {
            verify = 1;
        }
        else if (!strcmp(arg, "--interval") && i + 1 < argc)
        {
            interval = atoi(argv[++i]);
        }
        else if (!strcmp(arg, "--passes") && i + 1 < argc)
        {
            passes = atoi(argv[++i]);
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
        else if(!strcmp(arg,"--help"))  
        {
            usage();
            return 0;
        }
        else if(!strcmp(arg,"--version"))
        {
            printf("Tcrypto VERSION:POWERPC-Tcrypto-V1.00\n");
            return 0;
        }else
        {
            usage();
            printf("Unknown option: %s\n",arg);
            return -1;
        }
    }
    
    
    
    // set the i2c address 7-bits
    
    double diff=0;
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
    int test_do = 1;
    while (test_do <= (passes + 1))
    {
        memset(&cm, 0, sizeof(cm_t));
        fd = open(I2C_DEV, O_RDWR);
        if (fd < 0)
       {
        printf("Open the device failed: %s\n", strerror(errno));
        return -1;
       }
       result = ioctl(fd, I2C_TENBIT, 0);
       assert(!result);
       cm.fd = fd;
       for ( i = 0; i < 256; i++)
           write[i] = 0x00 + i % 128;

       result = at808_unlock(&cm, 10);
       assert(!result);
    
       at808_set_manufactur_code(&cm, write+8);
      
       at808_set_identification(&cm, write+8);
    
       at808_set_issuer_code(&cm, write+8);
    
    // Set the password set[0] "00 01 02" for reading and writing.
       at808_set_password(&cm, 0, write, 1);
       at808_set_password(&cm, 0, write, 0);
    
       if (crypto == 1)
       {
           access = 0x3f;//0x7f;//0xff;//
           pkey = 0xf8;//0xf8;//0xff;//
       }
       else
       {
           access = 0xff;
           pkey = 0xff;
       }
    
    // Zone-0 write password and read password bind to password set[0]. 
       at808_set_access(&cm, 0, &access);   
       at808_set_pkey(&cm, 0, &pkey);
    
    // We just set the rw-password of zone-0 "0x00 0x01 0x02"
       char password[3] = { 0x00, 0x01, 0x02 };   
    
        if (crypto == 1 && verify == 1)
        {
        // Verify password before writing user zone and write
           at808_verify_password(&cm, 0, password, 0, 5);
        }


        result = at808_write_zone(&cm, 0, &write[8], 0);

        assert(result != -1);
    
        if (crypto == 1 && verify == 1)
        {
        // Verify password before reading user zone and read
            at808_verify_password(&cm, 0, password, 0, 5);

        }
        result = at808_read_zone(&cm, 0, recv, 0);
        if (result == -1)
        {
            printf("Read the user zone-0 failed: %s\n", strerror(errno));
            return -1;
        }
        else
        {
            if (test_do == 1)
            {
                printf("============== Dump the user zone-0 ==============");
                for ( i = 0; i < 8; i++)
                {
                    if (i % 16 == 0)
                      printf("\n");    
                      printf("%02x ", ((unsigned char*)recv)[i]);
                }
                printf("\n");
                memcpy(crypto_check, recv, 8);
            }
            else
            {
                for ( i = 0; i < 8; i++)
                {
                    if (((unsigned char*)recv)[i] != crypto_check[i])
                    {
                        printf("[CRYPTO-TEST]: Check the data failed %d-0x%02x\n", 
                                 i, ((unsigned char*)recv)[i]);
                        return -1;                      
                    }
                }
                printf("[CRYPTO-TEST]: Check the data right\n"); 
            }
        }
        close(fd);
        gettimeofday(&tm_end,NULL);
        diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if((diff>time))
        {
            break;
        }
        test_do++;
        sleep(interval); 
    }

    return 0;   
                             
}
