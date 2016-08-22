    /*************************************************** 
     * �ļ�����pthread_server.c 
     * �ļ��������������߳������տͻ��˵����� 
    ***************************************************/  
     #include <sys/types.h>  
     #include <sys/socket.h>  
     #include <stdio.h>  
     #include <netinet/in.h>  
     #include <arpa/inet.h>  
     #define __USE_GNU
     #include <unistd.h>  
     #include <stdlib.h>  
     #include <pthread.h> 
     #include <fcntl.h>

     void *rec_data(void *fd);  
     int main(int argc,char *argv[])  
     {  
            int server_sockfd;  
            int *client_sockfd;  
            int server_len, client_len;  
            struct sockaddr_in server_address;  
            struct sockaddr_in client_address;  
            struct sockaddr_in tempaddr;  
            int i,byte,ret;  
            char char_recv,char_send;  
            socklen_t templen;  
            server_sockfd = socket(AF_INET, SOCK_STREAM, 0);//�����׽���  
            int on=1;                                                                                             
            setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));     
/*            int sock_buf_size=0x100000; 
            ret = setsockopt(server_sockfd,SOL_SOCKET,SO_SNDBUF,(char *)&sock_buf_size,sizeof(sock_buf_size)); 
            ret = setsockopt(server_sockfd,SOL_SOCKET,SO_RCVBUF,(char *)&sock_buf_size,sizeof(sock_buf_size)); 
*/        
            server_address.sin_family = AF_INET;  
            server_address.sin_addr.s_addr =  htonl(INADDR_ANY);  
            server_address.sin_port = htons(9720);  
            server_len = sizeof(server_address);  
             
            bind(server_sockfd, (struct sockaddr *)&server_address, server_len);//���׽���  
            templen = sizeof(struct sockaddr);  
        
            ret = listen(server_sockfd,20);
            if (ret < 0) {
                 perror("listen err");
                 return -1;
            }

            printf("server waiting for connect/n");  
            while(1){  
                   pthread_t thread;//������ͬ�����߳�������ͬ�Ŀͻ���  
                   client_sockfd = (int *)malloc(sizeof(int));  
                   client_len = sizeof(client_address);  
                   *client_sockfd = accept(server_sockfd,(struct sockaddr *)&client_address, (socklen_t *)&client_len);  
                   if(-1==*client_sockfd){  
                          perror("accept");  
                          continue;  
                   }  
                   if(pthread_create(&thread, NULL, rec_data, client_sockfd)!=0)//�������߳�  
                   {  
                          perror("pthread_create");  
                          break;  
                   }  
            }  
            shutdown(*client_sockfd,2);  
            shutdown(server_sockfd,2);  
     }  
     /***************************************** 
     * �������ƣ�rec_data 
     * �������������ܿͻ��˵����� 
    * �����б�fd���������׽��� 
    * ���ؽ����void 
     *****************************************/  
#define MAX_SIZE 0x20000
     void *rec_data(void *fd)  
     {  
            int client_sockfd;  
            int i,byte,err;  
            char char_recv[MAX_SIZE];//�������  
            client_sockfd=*((int*)fd);
            int cpu_base= ((client_sockfd+12)%24);
	        cpu_set_t cpuset;
	        CPU_ZERO(&cpuset);
	        CPU_SET(cpu_base,&cpuset);
	        err=pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
            if (err < 0)
                error(EXIT_FAILURE, -err, "%s():", __func__);
            for(;;)  
            {  
                   if((byte=recv(client_sockfd,char_recv,MAX_SIZE,0))==-1)  
                   {  
                          perror("recv");  
                          exit(EXIT_FAILURE);   
                   }  
       //            if(strcmp(char_recv, "exit")==0)//���ܵ�exitʱ������ѭ��  
        //                  break;  
         //          printf("receive from client is %s/n",char_recv);//��ӡ�յ�������  
            }

            free(fd);  
            close(client_sockfd);  
            pthread_exit(NULL);  
     } 

