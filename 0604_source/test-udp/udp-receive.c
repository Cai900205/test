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
	int fd;
	int len;
	struct sockaddr_in adr;
	pthread_t id;
	int cpu;
};

void *t_send(void *arg)
{
	
	int z=0;
	char buf[40000];
	struct task_type *send=arg;
	int sockfd=send->fd;
	struct sockaddr_in adr_srvr=send->adr;
	int opt=1;
	int size = send->len;
	cpu_set_t cpuset;
    struct timeval tm_start,tm_end;
	
	CPU_ZERO(&cpuset);
	CPU_SET(send->cpu,&cpuset);

	if((z=pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset))>0)
	{
		printf("cpu error!\n");
		exit(1);
	}
    gettimeofday(&tm_start,NULL);
    uint64_t total_count=0;
    int len = sizeof(struct sockaddr_in);
	while(1)
	{
        z=recvfrom(sockfd,buf,size,0,(struct sockaddr *)&adr_srvr,(socklen_t *)&len);
		if(z<0)
		{
			printf("send error!\n");
			exit(1);
		}
        total_count+=z;
        gettimeofday(&tm_end,NULL);
        double diff = (tm_end.tv_sec-tm_start.tv_sec)+((tm_end.tv_usec-tm_start.tv_usec)/1000000.0);
        if(diff>5)
        {
            double du_la=((total_count)/diff)/1024/1024;
            printf("thread: %d length(byte):%-15u time(s):%-15f avg MB/s %-15f total_count:%lld\n",send->cpu,size,diff,du_la,total_count);
            total_count=0;
            gettimeofday(&tm_start,NULL);
        }
	}
	pthread_exit(NULL);
}

int main(int argc,char **argv)
{
	int sock_fd;
	int i=0;
	int z,err,opt=1;
	struct sockaddr_in adr_srvr,user_addr;
	int thread_num=0;
	int packet_length=100;
	int cpu=0;
	int port=8888;
	if(argc < 7)
	{
		printf("ENTER ERROR!\n");
		printf("./udp thread_num begin_CPU packet_length target_ip user_ip port_num\n");
		return 0;
	}
	thread_num = atoi(argv[1]);	
	cpu = atoi(argv[2]);
	port = atoi(argv[6]);
	packet_length = atoi(argv[3]);
	struct task_type task_send[SEND_THREAD_NUM];
	adr_srvr.sin_family=AF_INET;
	adr_srvr.sin_port = htons(port);
	adr_srvr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(argv[4]);
	
	user_addr.sin_family=AF_INET;
	user_addr.sin_port = htons(port);
	user_addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(argv[5]);
	
	sock_fd = socket(AF_INET,SOCK_DGRAM,0);
	if(sock_fd == -1)
	{
		printf("socket error!\n");
		exit(1);
	}
	if((z = setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)))<0)
	{
		printf("setsocketopt error!\n");
		exit(1);
	}
	if((z = bind(sock_fd,(struct sockaddr *)&user_addr,sizeof(user_addr))) == -1)
	{
		printf("bind error!\n");
		exit(1);
	}	
	for(i=0;i<thread_num;i++)
	{
		adr_srvr.sin_port = htons(port);
		task_send[i].fd = sock_fd;
		task_send[i].adr.sin_family = adr_srvr.sin_family;
		task_send[i].adr.sin_port = adr_srvr.sin_port;
		task_send[i].adr.sin_addr.s_addr = adr_srvr.sin_addr.s_addr;
		task_send[i].cpu = cpu;
		task_send[i].len= packet_length;
		err = pthread_create(&task_send[i].id,NULL,t_send,&task_send[i]);
		if (err) {
			printf("Port %d : Receive thread failed!\n",2-port);
			exit(1);
		} 
		cpu++;
		port++;
	}

	for(i=0;i<thread_num;i++)
	{
		pthread_join(task_send[i].id,NULL);
	}
	close(sock_fd);
	return 0;
}
