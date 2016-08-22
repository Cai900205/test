    /*************************************************** 
     * �ļ�����pthread_server.c 
     * �ļ��������������߳������տͻ��˵����� 
    ***************************************************/  
     #include <sys/types.h>  
     #include <sys/socket.h>  
     #include <stdio.h>  
     #include <netinet/in.h>  
     #include <arpa/inet.h>  
     #include <unistd.h>  
     #include <stdlib.h>  
     #include <pthread.h> 

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
            server_sockfd = socket(AF_INET, SOCK_DGRAM, 0);//�����׽���  
        
            server_address.sin_family = AF_INET;  
            server_address.sin_addr.s_addr =  htonl(INADDR_ANY);  
            server_address.sin_port = htons(9734);  
            server_len = sizeof(server_address);  
            int on=1; 
            setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));        
            bind(server_sockfd, (struct sockaddr *)&server_address, server_len);//���׽���  
            templen = sizeof(struct sockaddr);  

            pthread_t thread[4];//������ͬ�����߳�������ͬ�Ŀͻ���  
            for(i=0;i<4;i++){  
                   if(pthread_create(&thread[i], NULL, rec_data, &server_sockfd)!=0)//�������߳�  
                   {  
                          perror("pthread_create");  
                          break;  
                   }  
            }  

	        for(i=0;i<4;i++) {
	            pthread_join(thread[i],NULL);
            }  

            shutdown(server_sockfd,2);  
            return 0;
	 } 
	/***************************************** 
     * �������ƣ�rec_data 
     * �������������ܿͻ��˵����� 
    * �����б�fd���������׽��� 
    * ���ؽ����void 
     *****************************************/  
#define MAX_SIZE 0x4000
     void *rec_data(void *fd)  
     {  
            int client_sockfd;  
            int i,byte;  
            char char_recv[MAX_SIZE];//�������  
            client_sockfd=*((int*)fd);
            struct sockaddr_in client_address;  
            int len=MAX_SIZE;
            for(;;)  
            {  
                   if((byte=recvfrom(client_sockfd,char_recv,MAX_SIZE,0,(struct sockaddr *)&client_address,&len)==-1))  
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

