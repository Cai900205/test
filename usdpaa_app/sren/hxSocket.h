/*
 * hxSocket.h
 *
 *  Created on: 2014-10-27
 *      Author: sren
 */

#ifndef HXSOCKET_H_
#define HXSOCKET_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct hx_socket{
	char* ip;
	int port;

	int sock;
	struct sockaddr_in dest;
}hx_socket;

int socketInit(hx_socket* sock);
int socketServerInit(hx_socket* sock);

int sockSend(hx_socket* sock, void * buf, int size);




#endif /* HXSOCKET_H_ */
