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

#include "server.h"
#include "msgmgr.h"
#include "helper.h"

#include <fstream>      /* for C++ file IO */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <unistd.h>     /* for gethostname() */
#include <string.h>     /* for memset() */
#include "sim.h"


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

/* handle client request - should be called under lock */
/*
 * NOTE: to avoid deadlocks we require the node to be
 * locked before. We are using special constructor for
 * the MADProcessor marking the node is preLocked
 */
int
IBMSClientConn::handleBindMsg(ibms_bind_msg_t &msg)
{
    MSG_ENTER_FUNC;
    MSGREG(err1, 'E', "Fail to create a new mad processor.", "server");

    /* create a new client mad processor */
    IBMSClientMsgProcessor *madProcessor = new IBMSClientMsgProcessor(this, msg);
    if (madProcessor == NULL) {
        MSGSND(err1);
        MSG_EXIT_FUNC;
        return 1;
    }

    /* keep track of active mad processors */
    madProcessors.push_back(madProcessor);

    MSG_EXIT_FUNC;
    return 0;
}


/* destructor - needs to cleanup any binding done through this conn */
IBMSClientConn::~IBMSClientConn()
{
    MSG_ENTER_FUNC;
    /* NOTE: each mad processor will lock the node */
    for(list_pmad_proc::iterator lI = madProcessors.begin();
            lI != madProcessors.end(); ++lI) {
        delete *lI;
    }
    MSG_EXIT_FUNC;
}


/****************************************************/
/* class IBMSClientMsgProcessor : IBMSMadProcessor **/
/****************************************************/
/*
 * Specialization of IBMSMadProcessor - this processor is being created
 * for each "BIND" command requested by the client. It should handle
 * forwarding of the MAD that it got to the client by invoking the
 * client send method.
*/

/* if filter is matched - forward the mad msg to the client send() */
int
IBMSClientMsgProcessor::processMad(uint8_t inPort,
        ibms_mad_msg_t &madMsg)
{
    MSG_ENTER_FUNC;

    ibms_response_t response;
    ibms_client_msg_t fwdMsg;
    int respSize;

    MSGREG(inf1, 'V', "Forwarding MAD tid:$ to client node:$ port:$","server");
    MSGREG(inf2, 'V', "Ignoring MAD tid:$ node:$ port:$ != cli port:$","server");

    fwdMsg.msg_type = IBMS_CLI_MSG_MAD;
    fwdMsg.msg.mad = madMsg;

    IBNode *pNode = pClient->getSimNode()->getIBNode();

    /* ignore mads arriving on the other port */
    if ((inPort != 0) && (pClient->getIbPortNum() != inPort)) {
        MSGSND(inf2, cl_ntoh64(fwdMsg.msg.mad.header.trans_id),
                pNode->name, inPort, pClient->getIbPortNum());
        MSG_EXIT_FUNC;
        return 0;
    }

    MSGSND(inf1, cl_ntoh64(fwdMsg.msg.mad.header.trans_id),
            pNode->name, pClient->getIbPortNum());

    /* get the clientConn object */
    /* invoke its sendMsg with the provided message */
    pClient->sendMsg(sizeof(ibms_client_msg_t), (char*)&fwdMsg,
            respSize, (char *)&response);

    MSG_EXIT_FUNC;
    return 0;
}


/****************************************************/
/****** class IBMSServer : public GenServer *********/
/****************************************************/
/*
 * The simulator server allowing for multiple clients to connect
 */
IBMSServer::IBMSServer(IBMgtSim *pS, unsigned short portNum) :
        GenServer(portNum, sizeof(ibms_client_msg_t))
{
    MSG_ENTER_FUNC;
    char hostName[32];
    gethostname(hostName, sizeof(hostName)-1);

    pSim = pS;

    /* the generic gen server might fail */
    if (isAlive()) {
        /* write down the server port into the appropriate file */
        std::ofstream serverFile;
        string serverFileName(pSim->getSimulatorDir());
        serverFileName += "/ibmgtsim.server";
        serverFile.open(serverFileName.c_str());
        if (!serverFile.fail()) {
            serverFile << hostName << " " << portNum << endl;
            serverFile.close();
        } else {
            MSGREG(err1, 'E', "Fail to create file:$", "server");
            MSGSND(err1, serverFileName);
        }
    }
    MSG_EXIT_FUNC;
}


/* cleaning up client when notified it is closing */
int IBMSServer::closingClient(int clientSock)
{
    MSG_ENTER_FUNC;
    map_sock_client::iterator sI = sockToClientMap.find(clientSock);
    if (sI != sockToClientMap.end()) {
        IBMSClientConn *pClientConn = (*sI).second;
        sockToClientMap.erase(sI);
        delete pClientConn;
    }
    MSG_EXIT_FUNC;
    return(0);
}


/* handle a connection message */
int IBMSServer::handleConnectionMsg(int clientSock,
        ibms_conn_msg_t &connMsg)
{
    MSG_ENTER_FUNC;
    MSGREG(err1, 'E', "Given port guid:$ does not exists in fabric.", "server");
    MSGREG(err2, 'E', "Socket $ has previously connected.", "server");
    MSGREG(err3, 'E', "Fail to create client connection.", "server");
    MSGREG(inf1, 'V', "Received connection requests to node:$ port:$.", "server");

    IBFabric *pFabric = pSim->getFabric();

    /* validate the given guid exists */
    map_guid_pport::const_iterator pI =
            pFabric->PortByGuid.find(connMsg.port_guid);
    if (pI == pFabric->PortByGuid.end()) {
        MSGSND(err1, connMsg.port_guid);
        MSG_EXIT_FUNC;
        return 1;
    }

    IBPort *pPort = (*pI).second;
    IBNode *pNode = pPort->p_node;
    IBMSNode *pMgtNode = ibmsGetIBNodeSimNode(pNode);

    MSGSND(inf1, pNode->name, pPort->num);

    /* we need to lock the map of the server to insert the new client */
    pthread_mutex_lock(&lock);

    /* if the client is not previously registered */
    map_sock_client::iterator sI = sockToClientMap.find(clientSock);
    if (sI != sockToClientMap.end()) {
        MSGSND(err2, clientSock);
        pthread_mutex_unlock(&lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    /* create a new client object */
    IBMSClientConn *clientConn = new IBMSClientConn(pMgtNode,
            pPort->num,
            &connMsg.host[0],
            connMsg.in_msg_port);

    if (clientConn == NULL) {
        MSGSND(err3);
        pthread_mutex_unlock(&lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    /* insert to the map */
    sockToClientMap[clientSock] = clientConn;

    /* unlock the map */
    pthread_mutex_unlock(&lock);

    MSG_EXIT_FUNC;
    return 0;
}


/* handle a disconnect message */
int
IBMSServer::handleDisconnectMsg(int clientSock,
        ibms_disconn_msg_t &discMsg)
{
    MSG_ENTER_FUNC;
    MSGREG(err1, 'E', "Given port guid:$ does not exists in fabric.", "server");
    MSGREG(err2, 'E', "Socket $ was not previously connected.", "server");
    MSGREG(inf1, 'V', "Received disconnect requests from node:$ port:$.", "server");

    IBFabric *pFabric = pSim->getFabric();

    /* validate the given guid exists */
    map_guid_pport::const_iterator pI =
            pFabric->PortByGuid.find(discMsg.port_guid);
    if (pI == pFabric->PortByGuid.end()) {
        MSGSND(err1, discMsg.port_guid);
        MSG_EXIT_FUNC;
        return 1;
    }

    IBPort *pPort = (*pI).second;
    IBNode *pNode = pPort->p_node;

    MSGSND(inf1, pNode->name, pPort->num);

    /* we need to lock the map of the server to delete the client */
    pthread_mutex_lock(&lock);

    /* if the client is not previously registered */
    map_sock_client::iterator sI = sockToClientMap.find(clientSock);
    if (sI == sockToClientMap.end()) {
        MSGSND(err2, clientSock);
        pthread_mutex_unlock(&lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    /* remove the connection from the list and delete it */
    IBMSClientConn *clientConn = (*sI).second;
    sockToClientMap.erase(sI);

    /* unlock the map */
    pthread_mutex_unlock(&lock);

  /* we need to delete the client itself not under the lock
   * since otherwise we will run into deadlock with the node lock */
    delete clientConn;

    MSG_EXIT_FUNC;
    return 0;
}


/* handle a bind message */
int
IBMSServer::handleBindMsg(int clientSock,
        ibms_bind_msg_t &bindMsg)
{
    MSG_ENTER_FUNC;
    int status;

    MSGREG(err1, 'E', "Socket $ was not previously connected.", "server");

    pthread_mutex_lock(&lock);

    /* find the client conn for the given sock */
    map_sock_client::iterator sI = sockToClientMap.find(clientSock);

    /* if none then fail */
    if (sI == sockToClientMap.end()) {
        MSGSND(err1, clientSock);
        pthread_mutex_unlock(&lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    IBMSClientConn *pClient = (*sI).second;
    IBMSNode *pSimNode = pClient->pSimNode;

    pthread_mutex_unlock(&lock);

    /* now that we know the client we can pre-lock the node */
    pthread_mutex_lock(&pSimNode->lock);
    pthread_mutex_lock(&lock);

    /* to be safe we find again */
    sI = sockToClientMap.find(clientSock);
    if (sI == sockToClientMap.end()) {
        MSGSND(err1, clientSock);
        pthread_mutex_unlock(&lock);
        pthread_mutex_unlock(&pSimNode->lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    status = (*sI).second->handleBindMsg(bindMsg);

    pthread_mutex_unlock(&lock);
    pthread_mutex_unlock(&pSimNode->lock);

    MSG_EXIT_FUNC;
    return(status);
}


/* handle a bind message */
int
IBMSServer::handleCapMsg(int clientSock,
        ibms_cap_msg_t &capMsg)
{
    MSG_ENTER_FUNC;
    MSGREG(err1, 'E', "Socket $ was not previously connected.", "server");

    pthread_mutex_lock(&lock);

    /* find the client conn for the given sock */
    map_sock_client::iterator sI = sockToClientMap.find(clientSock);

    /* if none then fail */
    if (sI == sockToClientMap.end()) {
        MSGSND(err1, clientSock);
        pthread_mutex_unlock(&lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    /* if we know the client we know the port number and node */
    IBMSClientConn *pCli = (*sI).second;

    ib_port_info_t *pPortInfo =
            &(pCli->getSimNode()->nodePortsInfo[pCli->getIbPortNum()]);

    pthread_mutex_unlock(&lock);

    /* OK we got the port info - now set/clr the capability mask */
    pPortInfo->capability_mask =
            (~capMsg.mask & pPortInfo->capability_mask) |
            (capMsg.mask & capMsg.capabilities);

    MSG_EXIT_FUNC;
    return(0);
}


/* handle a mad message */
int
IBMSServer::handleMadMsg(int clientSock,
        ibms_mad_msg_t &madMsg)
{
    MSG_ENTER_FUNC;
    int status;
    MSGREG(err1, 'E', "Socket $ was not previously connected.", "server");

    pthread_mutex_lock(&lock);

    /* find the client conn for the given sock */
    map_sock_client::iterator sI = sockToClientMap.find(clientSock);

    /* if none then fail */
    if (sI == sockToClientMap.end()) {
        MSGSND(err1, clientSock);
        pthread_mutex_unlock(&lock);
        MSG_EXIT_FUNC;
        return 1;
    }

    IBMSClientConn *pClientConn = (*sI).second;

    /* we need to replace the source lid with the lid of the port */
    madMsg.addr.slid =
            cl_hton16(pClientConn->getSimNode()->nodePortsInfo[pClientConn->getIbPortNum()].base_lid);

    status = pSim->getDispatcher()->dispatchMad(
            pClientConn->getSimNode(),
            pClientConn->getIbPortNum(),
            madMsg);

    pthread_mutex_unlock(&lock);
    MSG_EXIT_FUNC;
    return status;
}


/* handle client request - either create a new client conn or
    pass it there */
/* return 1 on error 0 otherwise */
int IBMSServer::proccessClientMsg(int clientSock,
        int reqLen, char request[],
        int &resLen, char *(pResponse[]) )
{
    MSG_ENTER_FUNC;
    MSGREG(err1, 'E', "Message is not of ibms_client_msg_t size ($ != $)", "server");
    MSGREG(inf1, 'V', "Received:\n$","server");

    if (reqLen != sizeof(ibms_client_msg_t)) {
        MSGSND(err1, reqLen, sizeof(ibms_client_msg_t));
        MSG_EXIT_FUNC;
        return 1;
    }

    ibms_client_msg_t *p_req = (ibms_client_msg_t*)request;
    MSGSND(inf1, ibms_get_msg_str(p_req));

    /* allocate new response */
    /*
     * NOTE: the allocated buffer for the response is deallocated by
     * the call from the GenServer::clientThreadMain
     */
    resLen = sizeof(ibms_response_t);
    *pResponse = new char[sizeof(ibms_response_t)];
    ibms_response_t *pResp = (ibms_response_t *)(*pResponse);
    memset(pResp, 0, sizeof(ibms_response_t));

    switch (p_req->msg_type) {
    case IBMS_CLI_MSG_CONN:
        pResp->status = handleConnectionMsg(clientSock, p_req->msg.conn);
        break;
    case IBMS_CLI_MSG_DISCONN:
        pResp->status = handleDisconnectMsg(clientSock, p_req->msg.disc);
        break;
    case IBMS_CLI_MSG_BIND:
        pResp->status = handleBindMsg(clientSock, p_req->msg.bind);
        break;
    case IBMS_CLI_MSG_CAP:
        pResp->status = handleCapMsg(clientSock, p_req->msg.cap);
        break;
    case IBMS_CLI_MSG_MAD:
        pResp->status = handleMadMsg(clientSock, p_req->msg.mad);
        break;
    case IBMS_CLI_MSG_QUIT:
        MSGREG(inf2, 'V', "Asked to quit. Quitting...","server");
        MSGSND(inf2);
        usleep(100000);
        exit(0);
        break;
    default:
        break;
    }

    /* we always succeed for now */

    MSG_EXIT_FUNC;
    return 0;
}

