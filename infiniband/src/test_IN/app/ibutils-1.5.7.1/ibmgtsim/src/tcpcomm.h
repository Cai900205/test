/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef IBMS_TCPCOMM_H
#define IBMS_TCPCOMM_H

#include <pthread.h>

/****h* IBMS/TCP-IP Communication
* NAME
*	TCP/IP Communication.
*
* DESCRIPTION
*	Basic functionality of TCP/IP server (multi threaded) and Client.
*
*  There are basically two objects:
*  GenServer and GenClient. They are acting as echo servers if not
*  specialized by providing new implementation of
*  GenServer::processClientMessage
*
* AUTHOR
*	Eitan Zahavi, Mellanox
*
*********/

#include <list>

/****************************************************/
/***************** class GenServer ******************/
/****************************************************/
/*
 * The server:
 *  allows multiple clients to connect and handles each one by
 *  a separate thread
 */
class GenServer {
protected:
    /* a lock used to synchronize insertions into the map and client threads */
    pthread_mutex_t lock;

private:
    /* the port we server on */
    unsigned short int serverPort;

    /* the maximal single message length in bytes */
    int maxMsgBytes;

    /* all client thread ids are stored in here */
    std::list< pthread_t > clientThreadsList;

    /* the server thread gets the port number and a pointer back to the object */
    struct ServerThreadArgs {
        class GenServer *pServer;
    };

    struct ClientThreadArgs {
        class GenServer *pServer;
        int              clientSock;
    };

    /* the server thread */
    pthread_t serverThreadId;

    /* the server socket */
    int serverSock;

    /* the worker function of the server thread */
    static void *serverThreadMain(void *);

    /* create the tcp server socket */
    int createServerSocket(unsigned short portNum);

    /* the worker function of the client thread */
    static void *clientThreadMain(void *threadArgs);

 public:
    /* construct and initialize the server */
    GenServer(unsigned short portNum, int maxMsgLen);

    /* destructor */
    virtual ~GenServer();

    /* return 1 if the server is well */
    int isAlive() {
        if (serverSock > 0)
            return 1;
        else
            return 0;
    };

    /* handle client request - this is the function to override . */
    virtual int proccessClientMsg(int clientSock,
            int reqLen, char request[],
            int &resLen, char *(pResponse[]));

    /* virtual function called when a client is closed - under a lock */
    virtual int closingClient(int clientSock) {
        return(0);
    };
};


/****************************************************/
/***************** class GenClient ******************/
/****************************************************/
/*
 * The client:
 *  connects to a server
 */
class GenClient {
private:
    /* host name of the server */
    char *hostName;

    /* the server port number */
    unsigned short int serverPort;

    /* the socket to communicate through */
    int sock;

    /* the maximal single response length in bytes */
    int maxResponseBytes;

 public:
    /* construct and initialize the client connection */
    GenClient(char *pHostName, unsigned short portNum, int maxRespLen);

    /* destruct a client connection */
    ~GenClient();

    /*
        send a message and wait for result
        The response buffer should be pre-allocated
     */
    int sendMsg(int reqLen, char request[],
            int &resLen, char response[]);
};


#endif /* IBMS_TCPCOMM_H */

