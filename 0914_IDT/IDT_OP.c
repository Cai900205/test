#include "IDT_OP.h"
enum CPS_CMD
{
    Help,
    Read,
    Write,
    Route,
    Init,
    Print,
    Recovery,
};

static const char* const cmd_name[]={"help","read","write","route","init","print","recovery"};
void  cmd_translate(char cli_arv[][100],int len,int fd)
{
    int i,rvl=0;
    uint32_t offset=0,port_num=0;
    uint32_t dport=0,sport=0,did=0,data=0;
    for(i=0;i<IDT_CMD_NUM;i++)
    {
        if(!strcmp(cli_arv[0],cmd_name[i]))
        {
            break;
        }
    }
    len=len-1;
    switch(i)
    {
      case Help:
            rvl=CPS_Help();
            break;
      case Read:
            if(len !=1)
            {
                printf("CPS Read param error!\n");
                CPS_Help();
                break;
            }
            offset=strtoul(cli_arv[1],NULL,16);
            rvl=CPS_Read(fd,offset);
            if(rvl!=0)
            {
                printf("CPS_Read failed!\n");
            }
            break;
      case Write:
            if(len!=2)
            {
                printf("CPS Write param error!\n");
                CPS_Help();
                break;
            }
            offset=strtoul(cli_arv[1],NULL,16);
            data=strtoul(cli_arv[2],NULL,16); 
            rvl=CPS_Write(fd,offset,data);
            if(rvl!=0)
            {
                printf("CPS_Write failed!\n");
            }
            break;
      case Print:
            if(len!=1)
            {
                printf("CPS Print param error!\n");
                CPS_Help();
                break;
            }
            port_num=strtoul(cli_arv[1],NULL,16);
            rvl=CPS_Print(fd,port_num);            
            if(rvl!=0)
            {
                printf("CPS_Print failed!\n");
            }
            break;
      case Init:
            rvl=CPS_Init(fd);            
            if(rvl!=0)
            {
                printf("CPS_Init failed!\n");
            }
            break;
      case Route:
            if(len!=3)
            {
                printf("CPS Route param error!\n");
                CPS_Help();
                break;
            }
            sport=strtoul(cli_arv[1],NULL,16);
            dport=strtoul(cli_arv[2],NULL,16);
            did=strtoul(cli_arv[3],NULL,10);
            rvl=CPS_Route(fd,sport,dport,did);            
            if(rvl!=0)
            {
                printf("CPS_Route failed!\n");
            }
            break;
      case Recovery:
            if(len!=1)
            {
                printf("CPS Recovery param error!\n");
                CPS_Help();
                break;
            }
            port_num=strtoul(cli_arv[1],NULL,16);
            rvl=CPS_Recovery(fd,port_num);            
            if(rvl!=0)
            {
                printf("CPS_Recovery failed!\n");
            }
            break;
      default:
            printf("Unkown command!\n");
            CPS_Help();
    }
    
}
void get_cmd(char **p)
{
    fgets(*p,100,stdin);
}
int split_string(char *s,char _cmd[][100])
{
    char *p = s;
    int i = 0;
    int j = 0;
    while(*p != '\0')
    {
        if(*p == ' ')
        {
            _cmd[i][j]='\0';
            i++;
            j = 0;
            p++;
            while(*p == ' ')
            {
                p++;
            }
        }
        else
        {
            _cmd[i][j] = *p;
            p++;
            j++;
        }
    }
    return i+1;
}

void print_array(char _arr[][100],int _len)
{
    int i = 0;
    for(i = 0; i < _len ; i++)
    {
        printf("%s\n",_arr[i]);
    }
}
int CPS_Help()
{
    printf("***************************************************************************************\n");
    printf("*****CPS Operation Command such as:                                              ******\n");
    printf("*****print    <port_num>                       : output port_num status          ******\n");
    printf("*****read     <offset>                         : read address register value     ******\n");
    printf("*****write    <offset> <data>                  : write address register value    ******\n");
    printf("*****route    <src_port> <dest_port> <dest_id> : set route table                 ******\n");
    printf("*****recovery <port_num>                       : recovery port                   ******\n");
    printf("*****Init                                      : init CPS_1432                   ******\n");
    printf("***************************************************************************************\n");
    return 0;
}
int IDT_Read(int fd,uint32_t offset,uint32_t *read_buffer)
{
    char write_buffer[7];
    int i=0,result=-1;
    offset=offset>>2;
    write_buffer[0]=(offset>>16);
    write_buffer[1]=(offset>>8 & 0xff);
    write_buffer[2]=(offset & 0xff); 
    result = write(fd, write_buffer, 3);
    if (result != 3)
    {
        printf("[IDT]: write address the register of the IDT (0x%08x) failed: %s\n",
                     offset, strerror(errno));
        return -1;
    }
    usleep(10);
    result = read(fd, read_buffer, 4);
    if (result != 4)
    {
        printf("[IDT]: Read the register of the i2c switch(0x%08x) failed: %s\n",
                     offset, strerror(errno));
        return -1;
    }

    return 0;
}
int CPS_Write(int fd,uint32_t offset,uint32_t data)
{
    char write_buffer[7];
    int result =-1;
    offset=offset>>2;
    write_buffer[0]=(offset>>16);
    write_buffer[1]=(offset>>8 & 0xff);
    write_buffer[2]=(offset & 0xff);
    memcpy((write_buffer+3),&data,4);
    result = write(fd, write_buffer, 7);
    if (result != 7)
    {
            printf("[IDT]: write address the register of the IDT (0x%08x) failed: %s\n",
                    offset, strerror(errno));
            return -1;
    }
    return 0;
}

int CPS_Read(int fd,uint32_t offset)
{
    uint32_t temp=0;
    IDT_Read(fd,offset,&temp);
    printf("Read Data:%08x\n",temp);
    return 0;
}

int CPS_Route(int fd,uint32_t src_port,uint32_t dst_port,uint32_t dst_id)
{
    uint32_t regaddr=0;
    regaddr=0xE10000 + src_port*0x1000+((dst_id&0xff)<<2);
    CPS_Write(fd,regaddr,dst_port);
    return 0;
}

void CPS1432_Routingtable(int fd)
{
    uint32_t temp=0;
    uint16_t dst_id,src_id,dst_port,src_port;
    uint32_t regaddr=0;
    int i=0;
//1
    dst_id=0x11;
    src_port=0x01;
    dst_port=0x06;
    CPS_Route(fd,dst_id,src_port,dst_port);
//4    
    dst_id=0x14;
    src_port=0x04;
    dst_port=0x03;
    CPS_Route(fd,dst_id,src_port,dst_port);
//3
    dst_id=0x14;
    src_port=0x03;
    dst_port=0x04;
    CPS_Route(fd,dst_id,src_port,dst_port);
 //6   
    dst_id=0x11;
    src_port=0x06;
    dst_port=0x01;
    CPS_Route(fd,dst_id,src_port,dst_port);
//other
    IDT_Read(fd,0xF2000C,&temp);
    printf("Device:%08x\n",temp);
    temp=temp|0x00000001;
    CPS_Write(fd,0xF2000C,temp);

    CPS_Write(fd,0x78,0xDF);
    for(i=1;i<=6;i++)
    {
        IDT_Read(fd,(0xf40004+i*0x100),&temp);
        temp=temp |(0x1<<26);
        CPS_Write(fd,(0xf40004+i*0x100),temp);

    }
}


int  CPS_Init(int fd)
{
    uint8_t i=0;
    uint32_t temp=0;
    for(i=1;i<=6;i++)
    {
        IDT_Read(fd,(0x15c+(i<<5)),&temp);
        temp=temp | (1<<23);
        CPS_Write(fd,(0x15c+(i<<5)),temp);
    }
    CPS1432_Routingtable(fd);    
    printf("Enable PORT!\n");
    CPS_Write(fd,0x120,0x00005500);
    for(i=1;i<=6;i++)
    {
        IDT_Read(fd,(0x15c+(i<<5)),&temp);
        temp=temp&(~(1<<23));
        CPS_Write(fd,(0x15c+(i<<5)),temp);
    }
    for(i=1;i<=6;i++)
    {
        IDT_Read(fd,(0x158+(i<<5)),&temp);
        temp=temp &(~(1<<30));
        CPS_Write(fd,(0x158+(i<<5)),temp);
    }
    CPS_Write(fd,0xf20300,0x8000007e);
    for(i=1;i<=6;i++)
    {
        IDT_Read(fd,(0x15c+(i<<5)),&temp);
        temp=temp | (3<<25);
        CPS_Write(fd,(0x15c+(i<<5)),temp);
    }
    printf("Reset port\n");
    for(i=1;i<=6;i++)
    {
        IDT_Read(fd,(0xf40004+i*0x100),&temp);
        temp=temp |(0x1<<26);
        CPS_Write(fd,(0xf40004+i*0x100),temp);
    }
    return 0;
}

int CPS_Print(int fd,int port_num)
{
    uint32_t temp;
    int i=port_num;
    printf("******************************************\n");
    printf("PORT %d encounter Status\n",i);
    IDT_Read(fd,(0x158+(0x20*i)),&temp);
    printf("Error and Status CSR:%08x\r\n",temp);
    IDT_Read(fd,(0x15C+(0x20*i)),&temp);
    printf("Control 1 CSR:%08x\r\n",temp);
    IDT_Read(fd,(0x1040+(0x40*i)),&temp);
    printf("Error Detect CSR:%08x\r\n",temp);
    IDT_Read(fd,(0xF40008+(0x100*i)),&temp);
    printf("Specific Error Detect:%08x\r\n",temp);
    IDT_Read(fd,(0xf4001c+(i<<8)),&temp);
    printf("Send(have Send):%d\r\n",temp);
    IDT_Read(fd,(0xf40040+(i<<8)),&temp);
    printf("Send(have acknowledge):%d\r\n",temp);
    IDT_Read(fd,(0xf40044+(i<<8)),&temp);
    printf("Send(not acknowledge):%d\r\n",temp);
    IDT_Read(fd,(0xf40048+(i<<8)),&temp);
    printf("Send(need retry):%d\r\n",temp);
    IDT_Read(fd,(0xf40010+(i<<8)),&temp);
    printf("receive(have acknowledge):%d\r\n",temp);
    IDT_Read(fd,(0xf40014+(i<<8)),&temp);
    printf("receive(not acknowledge):%d\r\n",temp);
    IDT_Read(fd,(0xf40018+(i<<8)),&temp);
    printf("receive(need retry):%d\r\n",temp);
    IDT_Read(fd,(0xf40050+(i<<8)),&temp);
    printf("receive(have receive):%d\r\n",temp);
    IDT_Read(fd,(0xf40064+(i<<8)),&temp);
    printf("receive(have drop):%d\r\n",temp);
    IDT_Read(fd,(0xf4004c+(i<<8)),&temp);
    printf("forward(have forward):%d\r\n",temp);
    IDT_Read(fd,(0xf40068+(i<<8)),&temp);
    printf("Send(have drop):%d\r\n",temp);
    IDT_Read(fd,(0xf4006c+(i<<8)),&temp);
    printf("TTL(drop):%d\r\n",temp);
    IDT_Read(fd,(0xf40070+(i<<8)),&temp);
    printf("CRC(have drop):%d\r\n",temp);
    printf("******************************************\n");

    return 0;
}

int CPS_Recovery(int fd,int i)
{
    uint32_t temp=0;
    int j=0,k=0;
    printf("recover port:%d\n",i);
    CPS_Write(fd,(0x1044+0x40*i),0);
    CPS_Write(fd,(0x106c+0x40*i),0);
    for(j=0;j<=3;j++)
    {
        k=i*4+j;
        CPS_Write(fd,(0xFF8010+0x100*k),0);
    }
    CPS_Write(fd,(0x15C+0x20*i),0xd6600001);
    CPS_Write(fd,(0x148+0x20*i),0x80000000);
    CPS_Write(fd,(0x140+0x20*i),0x00000003);
   
    do {
        IDT_Read(fd,(0x144+0x20*i),&temp);
        if(temp&0x80000000)
        {
            CPS_Write(fd,0xF20300,(0x80000000|(1<<i)));
            CPS_Write(fd,(0x140+0x20*i),0x00000004);
        }
    }while(!(temp&0x80000000));
    
    printf("recover port complete!\n");
    return 0;
}

//important
int CPS_Hot_Extration(int fd,int i)
{
    uint32_t temp=0;
	int j=0,k=0;
	CPS_Write(fd,(0x1068+0x40*i),0);
	CPS_Write(fd,(0x106c+0x40*i),0x01000000);
    for(j=0;j<=3;j++)
    {
        k=i*4+j;
        CPS_Write(fd,(0xFF8010+0x100*k),0x00000003);
    }
    CPS_Write(fd,(0x15C+0x20*i),0xd660000C);
    CPS_Write(fd,(0xf40004+0x100*i),0x18000000);
    CPS_Write(fd,(0x031044+0x40*i),0x80000000);
    CPS_Write(fd,(0x03104c+0x40*i),0x80000000);
//drop     
    CPS_Write(fd,(0x15C+0x20*i),0xd660000F);
    
    CPS_Write(fd,(0xF2000c),0x00000000);
     
    return 0;
}

int CPS_Hot_Insetion(int fd,int i)
{
    uint32_t temp=0;
	int j=0,k=0;
	temp=(i|(i<4)|(i<8)|(i<16));
	CPS_Write(fd,0xF20300,(0x8000000|temp));
    CPS_Write(fd,(0x15C+0x20*i),0xd660000F);
    
	CPS_Write(fd,(0x031044+0x40*i),0x80000000);
    CPS_Write(fd,(0x03104c+0x40*i),0x00000020);
    CPS_Write(fd,(0xf40004+0x100*i),0x18000000);

    return 0;
}
// end
int CPS_Unexpected_Extration(int fd,int i)
{
    uint32_t temp=0;
	int j=0,k=0;
	CPS_Write(fd,(0x1068+0x40*i),0);
	CPS_Write(fd,(0x106c+0x40*i),0x01000000);
    for(j=0;j<=3;j++)
    {
        k=i*4+j;
        CPS_Write(fd,(0xFF8010+0x100*k),0x00000003);
    }
    CPS_Write(fd,(0x15C+0x20*i),0xd660000C);
    CPS_Write(fd,(0xf40004+0x100*i),0x18000000);
    CPS_Write(fd,(0x031044+0x40*i),0x80000000);
    CPS_Write(fd,(0x03104c+0x40*i),0x80000000);
    
    return 0;
}


