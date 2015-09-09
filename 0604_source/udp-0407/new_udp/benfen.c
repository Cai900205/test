/*
 *
 *Date:09/16/2014
 *
 *Fuction:Ethernet Send Packets
 *
 *Param:thread_num begin_CPU packet_length target_ip source_ip port_num
 *
 *Author:ctx
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

#define __USE_GNU
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include "help.h"

#define SEND_THREAD_NUM 24

static const char * const config_info[][5]=
{{"fm1-mac5","00:12:13:44:55:66","192.168.1.1","192.168.1.11","8100"},
 {"fm2-mac5","00:12:13:14:55:66","192.168.1.2","192.168.1.22","8200"},
 {"fm1-mac9","00:12:22:21:49:55","192.168.2.1","192.168.2.11","8300"},
 {"fm1-mac10","00:12:22:21:43:55","192.168.2.2","192.168.2.22","8400"},
 {"fm2-mac9","00:12:22:25:41:55","192.168.3.2","192.168.3.22","8500"},
 {"fm2-mac10","00:12:22:26:42:55","192.168.3.3","192.168.3.33","8600"}
};

uint8_t get_option(char *interface)
{
    int i=0;
    for(i=0;i<6;i++)
    {
    	if(!strcmp(interface,config_info[i][0]))
    	{
        	return i;
    	}
    }
    return -1;
}

void config_route(uint8_t option0,uint8_t option1 )
{
    char tmp[200];
    sprintf(tmp,"ifconfig %s hw ether %s",config_info[option0][0],config_info[option0][1]);
    system(tmp);
    sprintf(tmp,"ifconfig %s hw ether %s",config_info[option1][0],config_info[option1][1]);
    system(tmp);
    sprintf(tmp,"ifconfig %s %s netmask 255.255.255.0",config_info[option0][0],config_info[option0][2]);
    system(tmp);
    sprintf(tmp,"ifconfig %s %s netmask 255.255.255.0",config_info[option1][0],config_info[option1][2]);
    system(tmp);
    sprintf(tmp,"route add %s dev %s",config_info[option0][3],config_info[option0][0]);
    system(tmp);
    sprintf(tmp,"route add %s dev %s",config_info[option1][3],config_info[option1][0]);
    system(tmp);
    sprintf(tmp,"arp -i  %s -s %s %s",config_info[option0][0],config_info[option0][3],config_info[option1][1]);
    system(tmp);
    sprintf(tmp,"arp -i  %s -s %s %s",config_info[option1][0],config_info[option1][3],config_info[option0][1]);
    system(tmp);
    sprintf(tmp,"iptables -t nat -F");
    system(tmp);
    sprintf(tmp,"iptables -t nat -A POSTROUTING -s %s -d %s -j SNAT --to-source %s",config_info[option0][2],config_info[option0][3],config_info[option1][3]);
    system(tmp);
    sprintf(tmp,"iptables -t nat -A PREROUTING -s %s -d %s -j DNAT --to-destination %s",config_info[option1][3],config_info[option0][3],config_info[option1][2]);
    system(tmp);
    sprintf(tmp,"iptables -t nat -A POSTROUTING -s %s -d %s -j SNAT --to-source %s",config_info[option1][2],config_info[option1][3],config_info[option0][3]);
    system(tmp);
    sprintf(tmp,"iptables -t nat -A PREROUTING -s %s -d %s -j DNAT --to-destination %s",config_info[option0][3],config_info[option1][3],config_info[option0][2]);
    system(tmp);
}

struct task_type
{
	int len;
    int check;
	struct sockaddr_in adr;
    struct sockaddr_in user;
	struct sockaddr_in se_adr;
    struct sockaddr_in se_user;
    pthread_t id;
	uint32_t passes;
    uint32_t time;
    int bind;
};

void *t_receive(void *arg)
{
	
    int z=0;
    char  buf[40000];
	char  read_buf[40000];
    struct task_type *send=arg;
    int sockfd=0;
    int result=-1;
    struct sockaddr_in adr_srvr=send->adr;
    struct sockaddr_in user_addr=send->user;
    int se_sockfd=0;
    struct sockaddr_in se_srvr=send->se_adr;
    struct sockaddr_in se_user=send->se_user;
	int opt=1;
	int size = send->len; 
    int check= send->check;
    double diff=0;
    struct timeval tm_start,tm_end;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if(send->bind)
    {
        if(sched_getaffinity(0,sizeof(cpuset),&cpuset) == -1)
        {
           printf("warning: cound not get cpu affinity!\n");
           return (void*)-1;
        }
	    result = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
        if (result)
	    {
		   printf("[NET-TEST]: Bind cpu  failed\n");
           fflush(stdout);
		   return (void*)-1;
	    }

    }

    gettimeofday(&tm_start,NULL);
    uint32_t i,k;
    uint64_t error_count=0,sucess_count=0;
    uint8_t  data=0,count=0;
    int len = sizeof(struct sockaddr_in);
    uint32_t passes=send->passes;

	se_sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(se_sockfd == -1)
	{
		printf("socket error!\n");
        fflush(stdout);
		exit(-1);
	}
	if((z = setsockopt(se_sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)))<0)
	{
		printf("setsocketopt error!\n");
        fflush(stdout);
		exit(-1);
	}
	if((z = bind(se_sockfd,(struct sockaddr *)&se_user,sizeof(se_user))) == -1)
	{
		printf("bind error!\n");
        fflush(stdout);
		exit(-1);
	}
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd == -1)
	{
		printf("socket error!\n");
                fflush(stdout);
		exit(1);
	}
	if((z = setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)))<0)
	{
		printf("setsocketopt error!\n");
                fflush(stdout);
		exit(1);
	}
	if((z = bind(sockfd,(struct sockaddr *)&user_addr,sizeof(user_addr))) == -1)
	{
		printf("bind error!\n");
                fflush(stdout);
		exit(1);
	}	
	for(i=0;(i<passes||(!passes));i++)
	{
		memset(buf,data,size);
		z=sendto(se_sockfd,buf,size,0,(struct sockaddr *)&se_srvr,sizeof(se_srvr));
		if(z<0)
		{
			printf("send error!\n");
            fflush(stdout);
			exit(-1);
        }
        z=recvfrom(sockfd,read_buf,size,MSG_DONTWAIT,(struct sockaddr *)&adr_srvr,(socklen_t *)&len);
	    if(z<0)
	    {
            count++;
            usleep(1);
            if(count == 1000000)
            {
                printf("recvfrom timeout !\n");
                fflush(stdout);
                error_count=0;
            }
        }
        else
        {
            count=0;
            if(z!=size)
            {
               error_count++;    
            }
            else
            {
               sucess_count++;
            }
            if(error_count==100000)
            {
                printf("Data is error!\n");
                fflush(stdout);
                error_count=0;
            }
            if(sucess_count==1000000)
            {
                printf("Data is right!\n");
                fflush(stdout);
                sucess_count=0;
            }
        }
        gettimeofday(&tm_end,NULL);
        diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if((diff > send->time)&&(passes))
        {
            printf("NET TEST time sucessful!\n");
            fflush(stdout);
            break;
        }
    }
    close(sockfd);
    close(se_sockfd);
	pthread_exit(NULL);
}

int main(int argc,char **argv)
{
	int i=0;
	int z,err,opt=1;
    char inter1[20],inter2[20];
	int workers=1;
	int packet_length=1000;
	int cpu=0,check=0;
	int port=8000;
    int inter1_op=-1,inter2_op=-1;
    uint32_t  passes=32,time=10;
    int interval=1,bind=0;

    for (i = 1; i < argc; i++)
	{
		char* arg = argv[i];
		if (!strcmp(arg, "--passes") && i + 1 < argc)
		{
			passes = strtoull(argv[++i],NULL,10);
		}
		else if (!strcmp(arg, "--workers") && i + 1 < argc)
		{
			workers = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--interval") && i + 1 < argc)
		{
			interval = atoi(argv[++i]);
		}
		else if (!strcmp(arg, "--interface_c") && i + 1 < argc)
		{
			strcpy(inter1,argv[++i]);
		}
		else if (!strcmp(arg, "--interface_s") && i + 1 < argc)
		{
			strcpy(inter2,argv[++i]);
		}
		else if (!strcmp(arg, "--bind"))
		{
            bind=1;
		}
		else if (!strcmp(arg, "--time") && i + 1 < argc) 
        {
            time = strtoull(argv[++i],NULL,10);
        }
		else if (!strcmp(arg, "--help")) 
        { 
            usage();
            fflush(stdout);
            return 0;
        }
		else if ( !strcmp(arg,"--version")) 
        { 
            printf("NET TEST VERSION:POWERPC-NET_TEST-V1.00\n");
            fflush(stdout);
            return 0;
        }
        else
        {
			printf("Unknown option: %s\n", arg);
            usage();
            fflush(stdout);
			return -1;
		}
	}
    inter1_op=get_option(inter1);
    inter2_op=get_option(inter2);
    if(inter1_op<0 || inter2_op <0)
    {
        printf("input interface error!\n");
        usage();
        fflush(stdout);
        return -1;
    }
    config_route(inter1_op,inter2_op);
	struct task_type task_send[SEND_THREAD_NUM];

    if(workers>23||workers<0)
    {
        printf("workers number is error!\n");
        fflush(stdout);
        return -1;
    }

    port= strtoull(config_info[inter1_op][4],NULL,10);
	for(i=0;i<workers;i++)
	{
		task_send[i].adr.sin_family =AF_INET; 
		task_send[i].adr.sin_port = htons(port);
		task_send[i].adr.sin_addr.s_addr = inet_addr(config_info[inter2_op][3]); 
		task_send[i].user.sin_family = AF_INET;
		task_send[i].user.sin_port = htons(port);
		task_send[i].user.sin_addr.s_addr = inet_addr(config_info[inter2_op][2]);
		task_send[i].se_adr.sin_family = AF_INET;
		task_send[i].se_adr.sin_port = htons(port);
		task_send[i].se_adr.sin_addr.s_addr = inet_addr(config_info[inter1_op][3]);
		task_send[i].se_user.sin_family = AF_INET;
		task_send[i].se_user.sin_port = htons(port);
		task_send[i].se_user.sin_addr.s_addr = inet_addr(config_info[inter1_op][2]);
		task_send[i].time = time;
		task_send[i].bind = bind;
        task_send[i].check = check;
		task_send[i].len= packet_length;
		task_send[i].passes= passes;
		err = pthread_create(&task_send[i].id,NULL,t_receive,&task_send[i]);
		if (err) {
			printf("Port %d : Receive thread failed!\n",2-port);
                        fflush(stdout);
			exit(1);
		}
        port++;
	}

	for(i=0;i<workers;i++)
	{
		pthread_join(task_send[i].id,NULL);
	}
	return 0;
}
