#include "IDT_OP.h"
/*
cps_op_t cps_op[]=
{
    { "help",    CPS_Help        },
    { "recovery",CPS_Recovery    },
    { "init",    CPS_Init        },
    { "read",    CPS_Read        },
    { "write",   CPS_Write       },
    { "print",   CPS_Print       },
    { "route",   CPS_Route       },
    {  NULL,     NULL            }
};
*/
const char Idt_prompt[]= "IDT_OP>";

int main(int argc, char* argv[])
{
    int result = -1;
    int fd = 0;
    uint32_t  offset = 0;
    int i2c_master = 2;
    int test_do = 1;
    int slave_addr = 0x67;
    int type = 0;
    char time_str[30];
    uint32_t temp;
    int i=0;
    uint32_t data=0;
    uint32_t port_num=0;
    
    for ( i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        if (!strcmp(arg, "--i2c") && i + 1 < argc)
        {
            i2c_master = strtoul(argv[++i],NULL,16);
        }
        else if (!strcmp(arg, "--slave") && i + 1 < argc)
        {
            slave_addr = strtoul(argv[++i],NULL,16);
        }
        else
        {
            printf("Unkown option: %s\n", arg);
            return -1;
        }
    }

    char i2c_dev[20];
    sprintf(i2c_dev, "/dev/i2c-%d", i2c_master);
    fd = open(i2c_dev, O_RDWR);
    assert(fd >= 0); 
    
    result = ioctl(fd, I2C_TENBIT, 0);
    if (result)
    {
        printf("[IDT]: Set the i2c address not 10 bits failed: %s\n", 
               strerror(errno));
        return -1;
    }
    
    result = ioctl(fd, I2C_SLAVE, slave_addr);
    if (result)
    {
        printf("[IDT]: Set the i2c address to 0x%02x failed: %s\n", 
               slave_addr, strerror(errno));
        return -1;
    }
    char *cli;
    char (*cli_argv)[100];
    int len,cmd;
    cli = (char *)malloc(sizeof(char)*100);
    cli_argv = (char (*)[])malloc(sizeof(cli));
    char *tmp;
    int done;
    tmp = (char *)NULL;
    while(1)
    {
        tmp = readline (Idt_prompt);
      /* Test for EOF. */
        if (!tmp)
     	    exit (1);
        if (*tmp)
     	{
	      add_history (tmp);
	}

        len = split_string(tmp,cli_argv);
        
        print_array(cli_argv,len);   
        if(cli_argv[0]==NULL|| len==0)
        {
            continue;
        }
        if(!strncmp(cli_argv[0],"q",1))
        {
		    free(tmp);
            break;
        }

        cmd_translate(cli_argv,len,fd);
        free(tmp);
    }

    return 0;
}      
