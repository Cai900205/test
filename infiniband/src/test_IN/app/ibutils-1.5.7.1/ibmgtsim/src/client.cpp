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

#include <stdlib.h>             /* for atoi() and exit() */
#include <string.h>             /* for memset() */
#include <unistd.h>             /* for close() */
#include <fstream>              /* for file streams */
#include "msgmgr.h"             /* message manager */
#include "helper.h"             /* mad dumps */
#include "simmsg.h"             /* IB mgt simulator messages */
#include "tcpcomm.h"            /* generic server and client */
#include "ibms_client_api.h"    /* interface we provide to C clients */


/****************************************************/
/********** class IBMSClient : GenClient ************/
/****************************************************/
class IBMSClient : GenClient {
public:
    IBMSClient(char *srvHostName, unsigned short portNum):
        GenClient(srvHostName, portNum, sizeof(ibms_response_t)) {};

    int sendSimMsg(ibms_client_msg_t &request, ibms_response_t &response) {
        MSGREG(err1, 'E', "Fail to obtain valid response size ($ != $)", "server");
        MSGREG(info1, 'V', "Obtained remote result:$", "server");
        int reqLen = sizeof(ibms_client_msg_t);
        int resLen;

        sendMsg(reqLen, (char*)&request, resLen, (char*)&response);

        if (resLen != sizeof(ibms_response_t)) {
            MSGSND(err1, resLen,  sizeof(ibms_response_t));
            return 1;
        }
        MSGSND(info1, ibms_get_resp_str(&response));
        return 0;
    };
};


/****************************************************/
/****** class IBMSClient : public GenServer *********/
/****************************************************/
/* we subclass the generic server to receive our own messages */
class IBMSClientInMsgs : public GenServer {
private:
    ibms_pfn_receive_cb_t pfnIncommingMadCallback;
    void *incommingMadContext;

public:
    IBMSClientInMsgs(unsigned short portNum, ibms_pfn_receive_cb_t cb, void *ctx):
        GenServer(portNum, sizeof(ibms_client_msg_t)) {
        incommingMadContext = ctx;
        pfnIncommingMadCallback = cb;
    };

    /* handle incoming messages */
    int proccessClientMsg(int clientSock,
            int reqLen, char request[],
            int &resLen, char *(pResponse[]));
};


int
IBMSClientInMsgs::proccessClientMsg(int clientSock,
        int reqLen,
        char request[],
        int &resLen,
        char *(pResponse[]))
{
    MSGREG(err1, 'E', "Message is not of ibms_client_msg_t size ($ != $)", "client");
    MSGREG(inf1, 'V', "Received:\n$","client");

    if (reqLen != sizeof(ibms_client_msg_t)) {
        MSGSND(err1, reqLen, sizeof(ibms_client_msg_t));
        return 1;
    }

    ibms_client_msg_t *pReq = (ibms_client_msg_t*)request;

    MSGSND(inf1, ibms_get_msg_str(pReq));

    if (pReq->msg_type == IBMS_CLI_MSG_MAD) {
        if (pfnIncommingMadCallback != NULL) {
            pfnIncommingMadCallback( incommingMadContext, &(pReq->msg.mad));
        }
    }

    /*
     * NOTE: the allocated buffer for the response is deallocated by
     * the call from the GenServer::clientThreadMain
     */
    resLen = sizeof(ibms_response_t);
    *pResponse = new char[sizeof(ibms_response_t)];
    ibms_response_t *pResp = (ibms_response_t *)*pResponse;

    /* we always succeed for now */
    pResp->status = 0;

    return 0;
}


/****************************************************/
/************ ibms_client_conn_rec_t ****************/
/****************************************************/
/* struct holding all information about one connection */
typedef struct _ibms_client_conn_rec {
    IBMSClient *pClient;
    IBMSClientInMsgs *pServer;
} ibms_client_conn_rec_t ;


/****************************************************/
/****************************************************/
/****************************************************/
/* obtain the simulator host name and port number */
void
__ibms_get_sim_host_n_port(char *hostName,
        unsigned short int &simPortNum)
{
    MSGREG(err1, 'F', "Fail to open:$","client");
    MSGREG(inf1, 'I', "Sim server:$ port:$","client");
    std::ifstream serverFile;
    std::string serverHost;

    std::string simDir;
    if (getenv("IBMGTSIM_DIR"))
        simDir = getenv("IBMGTSIM_DIR");
    else
        simDir = "/tmp/ibmgtsim";

    std::string serverFileName = simDir + "/ibmgtsim.server";

    serverFile.open(serverFileName.c_str());
    if (serverFile.fail()) {
        MSGSND(err1, serverFileName);
        exit(1);
    }
    serverFile >> serverHost >> simPortNum;
    MSGSND(inf1, serverHost, simPortNum);
    serverFile.close();
    strcpy(hostName, serverHost.c_str());
}


/*
 * connect to the server to the port guid.
 * Registering incoming messages callbacks
 */
ibms_conn_handle_t
ibms_connect(uint64_t portGuid,
        ibms_pfn_receive_cb_t receiveCb,
        void* context)
{
    unsigned short int serverPortNum;
    ibms_client_conn_rec_t clientConn;
    char hostName[32];
    unsigned short int simPortNum;
    unsigned int seed = (int)time(NULL);

    /* get the simulator hostname and port */
    __ibms_get_sim_host_n_port(hostName, simPortNum);

    /* create a client object and store in the map */
    clientConn.pClient = new IBMSClient(hostName, simPortNum);

    /* create a server and register it in the servers map */
    ibms_client_msg_t request;      /* the message we send */
    ibms_response_t response;       /* the message we receive back */

    /* iterate several times to find an available socket */
    int trys = 0;
    do {
        serverPortNum =
                (unsigned short int)((1.0*rand_r(&seed)/RAND_MAX)*(65535-1024)+1024);
        clientConn.pServer =
                new IBMSClientInMsgs(serverPortNum, receiveCb, context);

        if (clientConn.pServer->isAlive())
            break;
        else {
            delete clientConn.pServer;
            clientConn.pServer = NULL;
        }
    } while (trys++ < 50);

    if (!clientConn.pServer) {
        printf("-E- Failed to connect to simulator!\n");
        return 0;
    }
    printf("-I- Connected to simulator!\n");


    char thisHostName[32];
    gethostname(thisHostName, sizeof(thisHostName) - 1);

    /* connect to the given guid */
    request.msg_type = IBMS_CLI_MSG_CONN;
    request.msg.conn.port_num = 1;
    request.msg.conn.port_guid = portGuid;
    strcpy(request.msg.conn.host, thisHostName);
    request.msg.conn.in_msg_port = serverPortNum;

    if (clientConn.pClient->sendSimMsg(request, response)) {
        MSGREG(err1, 'F', "Fail to send connect message.","client");
        MSGSND(err1);
        return 0;
    }

    if (response.status) {
        MSGREG(err2, 'F', "Fail to connect to guid:$","client");
        MSGSND(err2, portGuid);
        return 0;
    }

    ibms_client_conn_rec_t *pCon =
            (ibms_client_conn_rec_t*)malloc( sizeof(ibms_client_conn_rec_t) );
    pCon->pServer = clientConn.pServer;
    pCon->pClient = clientConn.pClient;
    return pCon;
}


/*
 * bind to a specific mad messages
 */
int
ibms_bind(ibms_conn_handle_t conHdl,
        ibms_bind_msg_t *pBindMsg)
{
    ibms_client_conn_rec_t *pCon = (ibms_client_conn_rec_t*)conHdl;
    ibms_client_msg_t request;      /* the message we send */
    ibms_response_t response;       /* the message we receive back */

    /* send second message - bind to  */
    request.msg_type = IBMS_CLI_MSG_BIND;
    request.msg.bind = *pBindMsg;

    if (pCon->pClient->sendSimMsg(request, response))
        return 1;
    return response.status;
}


/*
 * set port capabilities
 */
int
ibms_set_cap(ibms_conn_handle_t conHdl,
        ibms_cap_msg_t *pCapMsg)
{
    ibms_client_conn_rec_t *pCon = (ibms_client_conn_rec_t*)conHdl;
    ibms_client_msg_t request;      /* the message we send */
    ibms_response_t response;       /* the message we receive back */

    /* send second message - bind to  */
    request.msg_type = IBMS_CLI_MSG_CAP;
    request.msg.cap = *pCapMsg;

    if (pCon->pClient->sendSimMsg(request, response))
        return 1;
    return response.status;
}


/*
 * send a message to the simulator
 */
int
ibms_send(ibms_conn_handle_t conHdl,
        ibms_mad_msg_t *pMadMsg)
{
    ibms_client_conn_rec_t *pCon = (ibms_client_conn_rec_t*)conHdl;
    ibms_client_msg_t request;      /* the message we send */
    ibms_response_t response;       /* the message we receive back */

    memset(&request.msg.mad, 0, sizeof(request.msg.mad));
    request.msg_type = IBMS_CLI_MSG_MAD;
    request.msg.mad = *pMadMsg;

    if (pCon->pClient->sendSimMsg(request, response))
        return 1;
    return response.status;
}


/*
 * disconnect from the simulator
 */
int
ibms_disconnect(ibms_conn_handle_t conHdl)
{
    ibms_client_conn_rec_t *pCon = (ibms_client_conn_rec_t*)conHdl;
    delete pCon;
    return(0);
}


/****************************************************/
/**************** BUILD_QUIT_CLIENT *****************/
/****************************************************/
#ifdef BUILD_QUIT_CLIENT
int main(int argc, char *argv[])
{
    unsigned short servPort;            /* server port */
    char *hostName;                     /* Server Host Name */
    ibms_client_msg_t request;          /* the message we send */
    ibms_response_t response;           /* the message we receive back */

    if ((argc < 2) || (argc > 3)) {     /* Test for correct number of arguments */
        fprintf(stderr, "Usage: %s <Server Host> [<Server Port>]\n", argv[0]);
        exit(1);
    }

    hostName = argv[1];

    if (argc == 3)
        servPort = atoi(argv[2]);   /* Use given port, if any */
    else
        servPort = 42561;

    msgMgr(0x1, &std::cout);
    msgMgr().setVerbLevel(0x1);

    /* we start the client */
    IBMSClient client(hostName, servPort);

    /* send first message - client connect */
    request.msg_type = IBMS_CLI_MSG_QUIT;

    printf("Sending Quit Message ...\n");
    client.sendSimMsg(request, response);

    sleep(2);

    exit(0);
}
#endif


/****************************************************/
/**************** BUILD_TEST_CLIENT *****************/
/****************************************************/
#ifdef BUILD_TEST_CLIENT
int main(int argc, char *argv[])
{
    unsigned short servPort;                    /* server port */
    char *hostName;                             /* Server Host Name */
    int bytesRcvd, totalBytesRcvd;              /* Bytes read in single recv() and total bytes read */
    ibms_client_msg_t request;                  /* the message we send */
    ibms_response_t response;                   /* the message we receive back */
    int msgLen = sizeof(ibms_client_msg_t);

    if ((argc < 2) || (argc > 3)) {   /* Test for correct number of arguments */
        fprintf(stderr, "Usage: %s <Server Host> [<Server Port>]\n", argv[0]);
        exit(1);
    }

    hostName = argv[1];

    if (argc == 3)
        servPort = atoi(argv[2]);   /* Use given port, if any */
    else
        servPort = 42561;

    msgMgr(MsgShowAll, &std::cout);

    /* need to have our own incoming messages port - so start our own server */
    IBMSClientInMsgs *pServer = new IBMSClientInMsgs( 46281, NULL, NULL );
    sleep(1);
    /* we start the client */
    IBMSClient client(hostName, servPort);

    /* send first message - client connect */
    request.msg_type = IBMS_CLI_MSG_CONN;
    request.msg.conn.port_num = 1;
    request.msg.conn.port_guid = 0x0002c90000000002ULL;
    gethostname(request.msg.conn.host, sizeof(request.msg.conn.host)-1);
    request.msg.conn.host[sizeof(request.msg.conn.host)] = '\0';
    request.msg.conn.in_msg_port = 46281;

    client.sendSimMsg(request, response);

    sleep(1);

    /* send second message - bind to  */
    request.msg_type = IBMS_CLI_MSG_BIND;
    request.msg.bind.port = 1;
    request.msg.bind.qpn = 1;
    request.msg.bind.mgt_class = 0x81;
    request.msg.bind.method = 0x1;
    request.msg.bind.mask = IBMS_BIND_MASK_PORT |
            IBMS_BIND_MASK_QP |
            IBMS_BIND_MASK_CLASS |
            IBMS_BIND_MASK_METH;

    client.sendSimMsg(request, response);

    sleep(1);

    for(int i = 1; i < 10; i++) {
        memset(&request.msg.mad, 0, sizeof(request.msg.mad));
        request.msg_type = IBMS_CLI_MSG_MAD;
        request.msg.mad.addr.dlid = 0x1;
        request.msg.mad.addr.slid = 0x2;
        request.msg.mad.addr.sqpn = 0;
        request.msg.mad.addr.dqpn = 0;

        request.msg.mad.header.mgmt_class = 0x81;
        request.msg.mad.header.method = 0x1;
        request.msg.mad.header.trans_id = i;

        /* we will hard-code some direct route path */
        ib_smp_t *p_mad = (ib_smp_t *)(&request.msg.mad.header);
        p_mad->hop_count = 3;
        p_mad->hop_ptr = 0;
        p_mad->initial_path[1] = 1;
        p_mad->initial_path[2] = i;
        p_mad->initial_path[3] = 2;
        p_mad->dr_dlid = IB_LID_PERMISSIVE;
        p_mad->dr_slid = IB_LID_PERMISSIVE;

        client.sendSimMsg(request, response);
    }

    /* let us make it return to us */
    memset(&request.msg.mad, 0, sizeof(request.msg.mad));
    request.msg_type = IBMS_CLI_MSG_MAD;
    request.msg.mad.addr.dlid = 0x1;
    request.msg.mad.addr.slid = 0x2;
    request.msg.mad.addr.sqpn = 0;
    request.msg.mad.addr.dqpn = 0;

    request.msg.mad.header.mgmt_class = 0x81;
    request.msg.mad.header.method = 0x1;
    request.msg.mad.header.trans_id = 123456;

    /* we will hard-code some direct route path */
    ib_smp_t *p_mad = (ib_smp_t *)(&request.msg.mad.header);
    p_mad->hop_count = 4;
    p_mad->hop_ptr = 0;
    p_mad->initial_path[1] = 1;
    p_mad->initial_path[2] = 3;
    p_mad->initial_path[3] = 1;
    p_mad->initial_path[4] = 1;

    p_mad->dr_dlid = IB_LID_PERMISSIVE;
    p_mad->dr_slid = IB_LID_PERMISSIVE;

    client.sendSimMsg(request, response);

    sleep(10);
    request.msg_type = IBMS_CLI_MSG_DISCONN;
    request.msg.disc.port_num = 1;
    request.msg.disc.port_guid = 0x0002c90000000002ULL;

    client.sendSimMsg(request, response);

    delete pServer;
    exit(0);
}
#endif

