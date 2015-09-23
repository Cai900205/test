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

#include <algorithm>
#include <sys/socket.h>     /* for socket(), bind(), and connect() */
#include <arpa/inet.h>      /* for sockaddr_in and inet_ntoa() */
#include <string.h>         /* for memset() */
#include <netdb.h>          /* for get host by name */
#include <unistd.h>         /* for close() */
#include "tcpcomm.h"
#include "msgmgr.h"


/****************************************************/
/***************** class GenServer ******************/
/****************************************************/
/*
* The basic flow:
*
* GenServer(portNum, maxMsgSize)
*  createServerSocket(portNum)
*  Create a server thread : open up a server socket
*   serverThreadMain : simply wait for client connections
*    Create client thread for every connection
*     clientThreadMain  : wait for incoming messages
*      recv - wait for data on the socket
*      while message size > 0 and no send error
*       proccessClientMsg : handle incoming message and return the response.
*       send - send the response
*      close the socket cleaning up reg in active threads list
*/


/* maximal number of pending socket connections to handle -
 * the simulator server allowing for multiple clients to connect */
#define MAXPENDING 5


/* create the tcp server socket */
/* return the socket number or -1 if error */
int GenServer::createServerSocket(unsigned short port_num)
{
    int sock;                           /* socket to create */
    struct sockaddr_in servAddr;        /* Local address */

    MSGREG(errMsg1, 'E', "Fail to open socket", "server");
    MSGREG(errMsg2, 'V', "Fail to bind socket for port:$", "server");
    MSGREG(errMsg3, 'E', "Fail to listen to socket", "server");
    MSGREG(verbMsg1, 'V', "Server is listening on port:$ socket:$", "server");

    /* Create socket for incoming connections */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        MSGSND(errMsg1);
        return -1;
    }

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));         /* Zero out structure */
    servAddr.sin_family = AF_INET;                  /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);   /* Any incoming interface */
    servAddr.sin_port = htons(port_num);            /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        MSGSND(errMsg2, port_num);
        return -1;
    }

    /* Mark the socket so it will listen for incoming connections */
    if (listen(sock, MAXPENDING) < 0) {
        MSGSND(errMsg3);
        return -1;
    }

    MSGSND(verbMsg1, port_num, sock );
    return sock;
}


/*
  handle client request -
  either create a new client conn or pass it there

  PARAMETERS:
  [in] request      : is a buffer allocated by the calling code
  [in] reqLen       : the length of the received request
  [out] resLen      : the length of the result
  [out] response    : internally allocated buffer for the result.

  return 1 on error 0 otherwise
*/
int
GenServer::proccessClientMsg(int clientSock,
        int reqLen, char request[],
        int &resLen, char *(pResponse[]))
{
    MSGREG(info1, 'I', "Received message:$", "server");
    MSGSND(info1, request);

    /* for now only echo */
    resLen = reqLen;
    *pResponse = new char[reqLen];
    strcpy(*pResponse, request);

    return 0;
}


/*
   the client thread worker function - basically loops to handle
   new client messages.
   Obtains a pointer to the server object and the new client socket in
   the given args.
*/
void *
GenServer::clientThreadMain(void *threadArgs)
{
    MSGREG(err2 ,'E' ,"Fail to send message (sent:$ should:$)", "server");
    MSGREG(msg1 ,'V' ,"Closed connection with client:$", "server");
    MSGREG(msg2 ,'V' ,"Waiting for messages from client:$", "server");

    /* Guarantees that thread resources are deallocated upon return */
    pthread_t threadID = pthread_self();
    pthread_detach(threadID);

    ClientThreadArgs *clientThreadArgs =
            static_cast<ClientThreadArgs*>(threadArgs);

    /* Extract socket file descriptor from argument */
    GenServer *pServer = clientThreadArgs->pServer;
    int clientSocket = clientThreadArgs-> clientSock;

    /* Deallocate memory for argument */
    delete clientThreadArgs;

    /* we store the received message in this buffer */
    char request[pServer->maxMsgBytes];
    char *response;

    int recvMsgSize;            /* Size of received message */
    int responseMsgSize;        /* size of the response */
    int sentMsgSize;            /* Size of sent message */
    int errOnSend = 0;          /* failing to send should quit */

    MSGSND(msg2 , clientSocket);

    /* Receive message from client */
    recvMsgSize = recv(clientSocket, request, pServer->maxMsgBytes, 0);
    while ((recvMsgSize != 0) && (!errOnSend)) {
        /* Handle the request */
        if (!pServer->proccessClientMsg(clientSocket,
                recvMsgSize, request, responseMsgSize, &response)) {
            sentMsgSize =
                    send(clientSocket, response, responseMsgSize, 0);
            delete [] response;

            /* if we fail to send - it is probably cause we lost the socket */
            if (sentMsgSize != responseMsgSize) {
                MSGSND(err2, sentMsgSize, responseMsgSize);
                errOnSend = 1;
            }
        }

        recvMsgSize = recv(clientSocket, request, pServer->maxMsgBytes, 0);
    }

    /* Close client socket */
    close(clientSocket);

    /* obtain the lock when cleaning up */
    pthread_mutex_lock(&pServer->lock);

    /* callback that can be used by server extensions for cleanup */
    pServer->closingClient(clientSocket);

    /* remove the client thread from the list of threads */
    std::list< pthread_t >::iterator lI =
            std::find(pServer->clientThreadsList.begin(),
                    pServer->clientThreadsList.end(),
                    threadID);
    if (lI != pServer->clientThreadsList.end())
        pServer->clientThreadsList.erase(lI);

    pthread_mutex_unlock(&pServer->lock);

    MSGSND(msg1 , clientSocket);
    return (NULL);
}


/* the server thread worker function */
/* obtains a pointer to the server object as its arg */
void *
GenServer::serverThreadMain(void *args)
{
    ServerThreadArgs *pArgs = static_cast<ServerThreadArgs *>(args);
    GenServer *pServer = pArgs->pServer;
    delete pArgs;

    MSGREG(errMsg1, 'E', "Fail to accept client", "server");
    MSGREG(verbMsg1, 'V', "Handling client $", "server");

    for (;;) {      /* run forever */
        int clntSock;                   /* Socket descriptor for client */
        struct sockaddr_in clntAddr;    /* Client address */
        unsigned int clntLen;           /* Length of client address data struct*/
        struct ClientThreadArgs *threadArgs;

        /* Set the size of the in-out parameter */
        clntLen = sizeof(clntAddr);

        /* Wait for a client to connect */
        clntSock = accept(pServer->serverSock,
                (struct sockaddr *)&clntAddr, &clntLen);
        if (clntSock < 0) {
            MSGSND(errMsg1);
            continue;
        }

        /* clntSock is connected to a client! */
        MSGSND(verbMsg1, inet_ntoa(clntAddr.sin_addr));

        /* Create separate memory for client argument */
        threadArgs = new ClientThreadArgs;
        if (!threadArgs) {
            MSGSND(errMsg1);
            exit(1);
        }

        threadArgs->pServer = pServer;
        threadArgs->clientSock = clntSock;

        /* Create client thread */
        pthread_t threadID;
        if (pthread_create(&threadID,
                NULL,
                GenServer::clientThreadMain,
                (void *)threadArgs) != 0) {
            MSGSND(errMsg1);
        }

        /* we probably want to register the client thread in the list */
        pthread_mutex_lock(&pServer->lock);
        pServer->clientThreadsList.push_back(threadID);
        pthread_mutex_unlock(&pServer->lock);
    }
}


/* construct the server */
/* if the server is not initialized correctly the serverSock is -1 */
GenServer::GenServer(unsigned short portNum, int maxMsgLen)
{
    MSGREG(errMsg1, 'F', "Fail to create server thread", "server");
    MSGREG(verbMsg1, 'V', "Started server thread", "server");

    serverPort = portNum;
    maxMsgBytes = maxMsgLen;

    /* initialize the lock object */
    pthread_mutex_init(&lock, NULL);

    /* setup the server listening  socket */
    serverSock = createServerSocket(portNum);

    /* we might have failed to gen the server - so avoid generating the thread */
    if (serverSock > 0) {
        /* we malloc the args as we want the server thread to deallocate them */
        ServerThreadArgs *pServerArgs = new ServerThreadArgs;
        pServerArgs->pServer = this;

        /* start the server thread providing it the server main loop function */
        if (pthread_create(&serverThreadId,
                NULL,
                GenServer::serverThreadMain,
                (void *)pServerArgs) != 0) {
            MSGSND(errMsg1);
            exit(1);
        }
    }
    MSGSND(verbMsg1);
}


/* server destructor */
GenServer::~GenServer()
{
    MSGREG(inf1, 'V', "Closing server on port:$", "server");
    MSGREG(inf2, 'V', "Cancelling server thread:$", "server");
    MSGREG(inf3, 'V', "Cancelling client thread:$", "server");

    MSGSND(inf1, serverPort);

    /* cleanup threads */
    pthread_mutex_lock(&lock);
    MSGSND(inf2, serverThreadId);

    /* we only have a thread if the socket was opened */
    if (isAlive()) {
        pthread_cancel(serverThreadId);

        for (std::list< pthread_t >::iterator tI = clientThreadsList.begin();
                tI != clientThreadsList.end();
                ++tI) {
            MSGSND(inf3, (*tI));
            pthread_cancel((*tI));
        }
    }
    pthread_mutex_unlock(&lock);
}


/****************************************************/
/***************** class GenClient ******************/
/****************************************************/
/* construct or die */
GenClient::GenClient(char *pHostName,
        unsigned short portNum,
        int maxRespLen)
{
    struct sockaddr_in servAddr;    /* server address */
    struct hostent *pHostEntry;     /* to be filled by gethostbyname */
    struct in_addr in;

    MSGREG(err1, 'F', "Fail to create socket", "client");
    MSGREG(err2, 'F', "Fail to gethostbyname:$", "client");
    MSGREG(err3, 'F', "No address list for host:$", "client");
    MSGREG(err4, 'F', "connect() failed for host:$ port:$", "client");
    MSGREG(inf1, 'I', "Connecting to host:$ ip:$ port:$", "client");

    hostName = new char[strlen(pHostName)+1];
    strcpy(hostName, pHostName);
    serverPort = portNum;
    maxResponseBytes = maxRespLen;

    /* Create a reliable, stream socket using TCP */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        MSGSND(err1);
        exit(1);
    }

    pHostEntry = gethostbyname(hostName);
    if (!pHostEntry) {
        MSGSND(err2, hostName);
        exit(1);
    }

    if (!pHostEntry->h_addr_list) {
        MSGSND(err3, hostName);
        exit(1);
    }

    memcpy(&in.s_addr, pHostEntry->h_addr_list[0], sizeof(in.s_addr));
    MSGSND(inf1, hostName, inet_ntoa(in), portNum);

    /* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));                 /* Zero out structure */
    servAddr.sin_family      = AF_INET;                     /* Internet address family */
    servAddr.sin_addr.s_addr = inet_addr(inet_ntoa(in));    /* Server IP address */
    servAddr.sin_port        = htons(portNum);              /* Server port */


    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        MSGSND(err4, hostName, portNum);
        exit(1);
    }
}


/* destructor - closing the socket */
GenClient::~GenClient()
{
    MSGREG(inf1, 'I', "Closing connection to server:$ port:$", "client");
    MSGSND(inf1, hostName, serverPort);
    close(sock);
}


/*
   send a message and wait for result
   The result buffer should be provided by the caller.
*/
int
GenClient::sendMsg(int reqLen, char request[],
        int &resLen, char response[])
{
    MSGREG(err1, 'E', "Fail to send.", "client");
    MSGREG(err2, 'E', "Fail to receive any response.", "client");

    /* Send the string to the server */
    if (send(sock, request, reqLen, 0) != reqLen) {
        MSGSND(err1);
        return 1;
    }

    /* Receive the same string back from the server */
    if ((resLen = recv(sock, response, maxResponseBytes, 0)) <= 0) {
        MSGSND(err2);
        return 1;
    }

    return 0;
}


/****************************************************/
/************** BUILD_TCP_COMM_SERVER ***************/
/****************************************************/
#ifdef BUILD_TCP_COMM_SERVER

#define MAX_MSG_SIZE 256

int main( int argc, char** argv)
{
    unsigned short servPort = 42561;    /* server port */
    char *hostName;                     /* Server Host Name */
    int bytesRcvd, totalBytesRcvd;      /* Bytes read in single recv() and total bytes read */
    if (argc > 2) { /* Test for correct number of arguments */
        fprintf(stderr, "Usage: %s [<Server Port>]\n", argv[0]);
        exit(1);
    }

    if (argc > 1) {
        servPort = atoi(argv[1]);
    }

    msgMgr(MsgShowAll, &std::cout);
    GenServer server(servPort, MAX_MSG_SIZE);

    while (1)
        sleep(100);

    exit(0);
}

#endif  /* BUILD_TCP_COMM_SERVER */


/****************************************************/
/************** BUILD_TCP_COMM_CLIENT ***************/
/****************************************************/
#ifdef BUILD_TCP_COMM_CLIENT

#define MAX_MSG_SIZE 256

int main( int argc, char** argv)
{
    unsigned short servPort;            /* server port */
    char *hostName;                     /* Server Host Name */
    int bytesRcvd, totalBytesRcvd;      /* Bytes read in single recv() and total bytes read */
    char *request   = new char[MAX_MSG_SIZE];
    char *response  = new char[MAX_MSG_SIZE];

    if ((argc < 3) || (argc > 4)) { /* Test for correct number of arguments */
        fprintf(stderr, "Usage: %s <Server IP> <msg> [<Server Port>]\n", argv[0]);
        exit(1);
    }

    hostName = argv[1];
    strcpy(request, argv[2]);

    if (argc == 4)
        servPort = atoi(argv[3]);     /* Use given port, if any */
    else
        servPort = 42561;

    msgMgr(MsgShowAll, &std::cout);

    /* initialize the client */
    MSGREG(msg1, 'I', "Initializing Client: Server:$ Port:$", "client");
    MSGSND(msg1, hostName, servPort);

    GenClient client( hostName, servPort, MAX_MSG_SIZE );

    /* send first message - client connect */
    MSGREG(msg2, 'I', "Sending (size:$) :$", "client");
    MSGSND(msg2, strlen(request)+1, request);
    int reqLen = strlen(request)+1;
    if (!client.sendMsg(reqLen, request, bytesRcvd, response)) {
        MSGREG(msg3, 'I', "Received (len:$) :$", "client");
        MSGSND(msg3, bytesRcvd, response);
    }
    exit(0);
}

#endif  /* BUILD_TCP_COMM_CLIENT */
