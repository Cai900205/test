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
#define SEND_THREAD_NUM 24


struct task_type
{
	int len;
        int check;
	struct sockaddr_in adr;
        struct sockaddr_in user;
	pthread_t id;
	int cpu;
};

void *t_receive(void *arg)
{
	
	int z=0;
	char  buf[40000];
	struct task_type *send=arg;
	int sockfd=0;
	struct sockaddr_in adr_srvr=send->adr;
        struct sockaddr_in user_addr=send->user;
	int opt=1;
	int size = send->len;
        int check= send->check;
	cpu_set_t cpuset;
        struct timeval tm_start,tm_end;
	
	CPU_ZERO(&cpuset);
	CPU_SET(send->cpu,&cpuset);

	if((z=pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset))>0)
	{
		printf("cpu error!\n");
                fflush(stdout);
		exit(1);
	}
        gettimeofday(&tm_start,NULL);
        uint64_t total_count=0,k;
        uint64_t error_count=0,err=0;
        uint8_t data=0,sucess_count=0;
        int len = sizeof(struct sockaddr_in);

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
	while(1)
	{
             z=recvfrom(sockfd,buf,size,MSG_DONTWAIT,(struct sockaddr *)&adr_srvr,(socklen_t *)&len);
	     if(z<0&&!check)
	     {
                 error_count++;
                 usleep(1);
                 if(error_count == 1000000)
                 {
                     printf("recvfrom error!\n");
                     fflush(stdout);
                     error_count=0;
                 }
             }
             else
             {
                 error_count=0;
                 if(check)
                 {
			z=sendto(sockfd,buf,size,0,(struct sockaddr *)&adr_srvr,sizeof(adr_srvr));
		        if(z<0)
	   		{
			    printf("send error!\n");
                            fflush(stdout);
			    exit(1);
			}
                 }
                 else
                 {
                       total_count++;
                       if((total_count+1)%300000==0)
                       {
				printf("receive data sucess!\n");
                                fflush(stdout);
                                total_count=0;
		       }
                 }
                 data++;
            }
        }
        close(sockfd);
	pthread_exit(NULL);
}
void usage()
{
    printf("[option]:--threadnum Thread numbers\n");
    printf("         --startcpu  Start CPU\n");
    printf("         --length    Packet length\n");
    printf("         --port      Port number\n");
    printf("         --targetip  Target IP\n");
    printf("         --sourceip  Source IP\n");
    printf("         --check     Is or not check data\n");
    printf("         --help      APP Help \n");
    printf("         --version   APP version\n");
}

int main(int argc,char **argv)
{
	int i=0;
	int z,err,opt=1;
	struct sockaddr_in adr_srvr,user_addr;
	int thread_num=0;
	int packet_length=100;
	int cpu=0,check=0;
	int port=8888;
/*	if(argc < 8)
	{
		printf("ENTER ERROR!\n");
		printf("./udp thread_num begin_CPU packet_length target_ip user_ip port_num checkdata\n");
                fflush(stdout);
		return 0;
	}*/
        adr_srvr.sin_addr.s_addr = inet_addr("192.168.10.3");
	user_addr.sin_addr.s_addr = inet_addr("192.168.10.33");
    for(i=1;i<argc;i++)
    {
        char *arg=argv[i];
        if(!strcmp(arg,"--threadnum")&&(i+1)<argc)
        {
	        thread_num = atoi(argv[++i]);	
        }else if(!strcmp(arg,"--startcpu")&&(i+1)<argc)
        {
	        cpu = atoi(argv[++i]);
        }else if(!strcmp(arg,"--length")&&(i+1)<argc)
        {
	        packet_length = atoi(argv[++i]);
        }else if(!strcmp(arg,"--port")&&(i+1)<argc)
        {
	        port = atoi(argv[++i]);    
        }else if(!strcmp(arg,"--check"))
        {
	        check=1;
        }else if(!strcmp(arg,"--targetip")&&(i+1)<argc)
        {
	        adr_srvr.sin_addr.s_addr = inet_addr(argv[++i]);
        }else if(!strcmp(arg,"--sourceip")&&(i+1)<argc)
        {
	        user_addr.sin_addr.s_addr = inet_addr(argv[++i]);
        }else if(!strcmp(arg,"--help"))
        {
            usage();
            return 0;
        }else if(!strcmp(arg,"--version"))
        {
            printf("UDP RECEIVE VERSION:POWERPC-NET-V1.00\n");
            fflush(stdout);
            return 0;
        }else
        {
            printf("Unkown option: %s\n",arg);
            fflush(stdout);
            return -1;
        }
    }
	struct task_type task_send[SEND_THREAD_NUM];
	adr_srvr.sin_family=AF_INET;
	adr_srvr.sin_port = htons(port);
	adr_srvr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(argv[4]);

	user_addr.sin_family=AF_INET;
	user_addr.sin_port = htons(port);	
	for(i=0;i<thread_num;i++)
	{
		adr_srvr.sin_port = htons(port);
                user_addr.sin_port = htons(port);
		task_send[i].adr.sin_family = adr_srvr.sin_family;
		task_send[i].adr.sin_port = adr_srvr.sin_port;
		task_send[i].adr.sin_addr.s_addr = adr_srvr.sin_addr.s_addr;
		task_send[i].user.sin_family = user_addr.sin_family;
		task_send[i].user.sin_port = user_addr.sin_port;
		task_send[i].user.sin_addr.s_addr = user_addr.sin_addr.s_addr;
		task_send[i].cpu = cpu;
                task_send[i].check = check;
		task_send[i].len= packet_length;
		err = pthread_create(&task_send[i].id,NULL,t_receive,&task_send[i]);
		if (err) {
			printf("Port %d : Receive thread failed!\n",2-port);
                        fflush(stdout);
			exit(1);
		} 
		cpu++;
		port++;
	}

	for(i=0;i<thread_num;i++)
	{
		pthread_join(task_send[i].id,NULL);
	}
	return 0;
}
