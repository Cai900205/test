/*****test.c****/
/*
 *
 It is a easy job to use this program:
 For diffrent platform there are diffrent methods to complie and link
 Windows: complie it with vc++ 6.0 IDE
 RedHat Linux: gcc -o test test.c
 Soloaris9:  gcc -o test test.c -lsocket -lresolv 
 FreeBsd:   gcc -o test test.c  
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
#define SLEEP(t) Sleep(1000*t)
#include <Winsock2.h>
#include <Windows.h>
#pragma comment(lib,"Ws2_32.lib")
#else
/*
for unix  platform Like Linux8,9 and Fedoral ,Solaris9 and freebsd
*/
#define SLEEP(t) sleep(t)
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#endif

#ifdef __FreeBSD__
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef __SunOS__
/*
you need to link this file with option -lsocket -lresolv
gcc -o test  test.c -lsocket -lresolv
*/
#endif
#define MSG_SIZE 1024
int initSndSock(struct sockaddr_in * addr,char * SERVER_IP,int port);
int SndToSock(int sockfd,struct sockaddr_in * addr,char buffer[]);
int RcvFrmSock(int sockfd, char buffer[]);
int initRcvSockAtPort(int port);
int main(void)
{  
    int sockfd;
        struct sockaddr_in addr;
        char buffer[MSG_SIZE];
        
        /*
        can be used an server to receive message 
        */
        /*
        
          sockfd = initRcvSockAtPort(9001);
          while(1)
          {
          memset(buffer, 0, MSG_SIZE);
          if(RcvFrmSock(sockfd, buffer)>=0)
          printf("Receive : %s", buffer);
          
           }
        */
        
        /*
        can be used as client to send messge
        to test receive functions ,comment code below and delete comment
           flags of codes above ,then run again
        */
        sockfd = initSndSock(&addr, "192.168.1.124",9001);
        while(1)
        {
                
                memset(buffer, 0, MSG_SIZE);
#ifdef _WIN32 
                sprintf(buffer, "%s", "I am windows message");
                
                printf("WIN 32 Sending once ..........\n");
#else 
                sprintf(buffer, "%s", "I am not Windows message");
                
                printf("Other Unix system Sending once ..........\n"); 
#endif
                SndToSock(sockfd, &addr, buffer);
                SLEEP(1);
        }
        
        
}
int initSndSock(struct sockaddr_in * addr,char * SERVER_IP,int port)
{
        int sockfd; 
        int a1,a2,a3,a4;
#ifdef WIN32
        WSADATA w;
        if(WSAStartup(0x0101, &w) != 0)
    {
                fprintf(stderr, "Could not open Windows connection.\n"); 
                exit(0);
    } 
#endif
        sockfd=socket(AF_INET,SOCK_DGRAM,0); 
#ifdef WIN32
    if(sockfd == INVALID_SOCKET)
        {
                fprintf(stderr, "Could not create socket.\n");
                WSACleanup();
                exit(0);
        }
    memset((void *)addr, '\0', sizeof(struct sockaddr_in)); 
#else 
        if(sockfd<0) 
        { 
                fprintf(stderr,"Socket Error:%s\n",strerror(errno)); 
                return -1;
        }  
        bzero(addr,sizeof(struct sockaddr_in));    
#endif
           addr->sin_family=AF_INET; 
        addr->sin_port=htons((u_short)port); 
#ifdef  _WIN32
        
        sscanf(SERVER_IP,"%d.%d.%d.%d",&a1,&a2,&a3,&a4);
        addr->sin_addr.S_un.S_un_b.s_b1 = (unsigned char)a1;
        addr->sin_addr.S_un.S_un_b.s_b2 = (unsigned char)a2;
        addr->sin_addr.S_un.S_un_b.s_b3 = (unsigned char)a3;
        addr->sin_addr.S_un.S_un_b.s_b4 = (unsigned char)a4;
    
#else
        if(inet_aton(SERVER_IP,&(addr->sin_addr))<0) 
        { 
                fprintf(stderr,"Ip error:%s\n",strerror(errno)); 
                return -1; 
        } 
#endif
        return sockfd;
}

int SndToSock(int sockfd,struct sockaddr_in * addr,char buffer[])
{
        int len = sizeof(struct sockaddr_in);
        sendto(sockfd,buffer,strlen(buffer),0,(struct sockaddr*)addr,len); 
        return 0;
}

int RcvFrmSock(int sockfd, char buffer[])
{
        int n,len,ll; 
        struct sockaddr_in addr;
        len = sizeof(struct sockaddr_in); 
    ll = len;  
    memset(buffer, '\0' , MSG_SIZE);
    n=recvfrom(sockfd, buffer, MSG_SIZE, 0, (struct sockaddr*)&addr, &ll);         
        if(n < 0)
        {
                return -1;
        }
        buffer[n] = '\0';  
        return n;
}

int initRcvSockAtPort(int port)
{
        int sockfd; 
    struct sockaddr_in addr; 
#ifdef _WIN32
        WSADATA w;
        if (WSAStartup(0x0101, &w) != 0)
        {
                fprintf(stderr, "Could not open Windows connection.\n");
                exit(0);
        }
#endif
        sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
        if(sockfd < 0) 
        { 
#ifdef _WIN32
                WSACleanup();
#endif
                fprintf(stderr, "Socket Error:%s\n", strerror(errno)); 
                return -1; 
        } 
        
        memset((void*)&addr, '\0', sizeof(struct sockaddr_in) );
        addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = htons(INADDR_ANY);
        
        addr.sin_port = htons((u_short )port);
        if(bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) 
        { 
                fprintf(stderr, "Bind Error  :%s\n", strerror(errno)); 
                return -1; 
        } 
        return sockfd;
}
