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

#ifndef IBMS_SERVER_H
#define IBMS_SERVER_H

#include "node.h"
#include "tcpcomm.h"
#include "msgmgr.h"

/****h* IBMS/Server
* NAME
*	IB Management Simulator Server
*
* DESCRIPTION
*	The simulator provides a TCP/IP server by which the clients can send
*  and receive mads.
*
* AUTHOR
*	Eitan Zahavi, Mellanox
*
*********/


/****************************************************/
/******** class IBMSClientConn : GenClient **********/
/****************************************************/
/*
 *  the client connection class handles a specific client
 */

/*
 * One instance of this class per exists for each connected client.
 * The instance is created when the client sends the connect message,
 * providing the port guid it attaches to.
 */
typedef std::list< class IBMSClientMsgProcessor *> list_pmad_proc;

class IBMSClientConn : GenClient {
private:
    /* the actual node this client attached to. */
    class IBMSNode *pSimNode;

    /* for cleanup we want to keep the registered processors */
    list_pmad_proc madProcessors;

    uint8_t ibPortNum;
public:
    /* constructor */
    /* TODO: It is not enough to keep the node - we need the IB port num */
    IBMSClientConn(IBMSNode *pN, uint8_t ibPN,
            char *hostName, unsigned short portNum) :
                GenClient(hostName, portNum, sizeof(ibms_response_t)) {
        pSimNode = pN;
        ibPortNum = ibPN;
    };

    /* destructor */
    ~IBMSClientConn();

    /* get the IB Port Number we attach to */
    uint8_t getIbPortNum() {
        return ibPortNum;
    };

    /* get the sim node connected to this client */
    class IBMSNode *getSimNode() {
        return pSimNode;
    };

    /* handle client bind request */
    int handleBindMsg(ibms_bind_msg_t &msg);

    friend class IBMSClientMsgProcessor;
    friend class IBMSServer;
};


/****************************************************/
/* class IBMSClientMsgProcessor : IBMSMadProcessor **/
/****************************************************/
/*
 * Specialization of IBMSMadProcessor - this processor is being created
 * for each "BIND" command requested by the client. It should handle
 * forwarding of the MAD that it got to the client by invoking the
 * client send method.
*/
class IBMSClientMsgProcessor : IBMSMadProcessor {
private:
    /* The filtering information by which we attach to. */
    ibms_bind_msg_t filter;

    /* the client object attached. */
    class IBMSClientConn *pClient;

public:
    /* if filter is matched - forward the mad msg to the client send() */
    int processMad(uint8_t inPort, ibms_mad_msg_t &madMsg);

    /* construct the new processor */
    IBMSClientMsgProcessor(class IBMSClientConn *pCli,
            ibms_bind_msg_t &msg) : IBMSMadProcessor(pCli->pSimNode,
                    msg.mgt_class,
                    TRUE) {
        MSGREG(inf1,'V', "Binding client to node:$ class:$","server");
        filter = msg;
        pClient = pCli;
        MSGSND(inf1, pCli->pSimNode->getIBNode()->name, msg.mgt_class);
    };
};


/****************************************************/
/****** class IBMSServer : public GenServer *********/
/****************************************************/
#define map_sock_client std::map< int, class IBMSClientConn *, std::less< int > >

/*
 * The simulator server allowing for multiple clients to connect
 */
class IBMSServer : public GenServer {
    /* a list of active clients */
    map_sock_client sockToClientMap;

    /* pointer back to the simulator */
    class IBMgtSim *pSim;

    /* handle a connection message */
    int handleConnectionMsg(int clientSock, ibms_conn_msg_t &connMsg);

    /* handle a connection message */
    int handleDisconnectMsg(int clientSock, ibms_disconn_msg_t &discMsg);

    /* handle a bind message */
    int handleBindMsg(int clientSock, ibms_bind_msg_t &bindMsg);

    /* handle a mad message */
    int handleMadMsg(int clientSock, ibms_mad_msg_t &madMsg);

    /* handle client port capabilities mask request */
    int handleCapMsg(int clientSock, ibms_cap_msg_t &msg);

    /* invoked when a client is closing - under a lock */
    int closingClient(int clientSock);

 public:
    /* constructor */
    IBMSServer(IBMgtSim *pS, unsigned short portNum);

    /* handle client request - either create a new client conn or
        pass the request to the existing one */
    int proccessClientMsg(int clientSock,
            int reqLen, char request[],
            int &resLen, char *(pResponse[]));
};


#endif /* IBMS_SERVER_H */
