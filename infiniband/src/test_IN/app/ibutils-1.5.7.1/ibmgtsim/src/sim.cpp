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


#include "git_version.h"
#include "sim.h"
#include "msgmgr.h"
#include "sma.h"
#include "vsa.h"
#include "pma.h"
#include <getopt.h>
#include <inttypes.h>


/****************************************************/
/****************** class IBMgtSim ******************/
/****************************************************/
const char *IBMgtSim::getSimulatorDir()
{
    if (!getenv("IBMGTSIM_DIR")) {
        printf("-W- Environment variable: IBMGTSIM_DIR does not exist.\n");
        printf("    Please create one used by the simulator.\n");
        printf("    Using /tmp/ibmgtsim as default.\n");
        return "/tmp/ibmgtsim";
    }
    return getenv("IBMGTSIM_DIR");
}


/* allocate guids to the nodes */
int
IBMgtSim::allocateFabricNodeGuids()
{
    FILE* dumpFile;

    MSGREG(errMsg1, 'F', "Fail to open guid dump file:$", "server");
    string dumpFileName(getSimulatorDir());
    dumpFileName += "/ibmgtsim.guids.txt";
    dumpFile = fopen(dumpFileName.c_str(), "w");
    if (! dumpFile) {
        MSGSND(errMsg1, dumpFileName);
        exit(1);
    }

    MSGREG(msg1, 'I', "Assigning Guids ...", "server");
    MSGSND(msg1);

    uint64_t curGuid = 0x0002c90000000000ULL;

    /* simply go over all nodes and allocate guids */
    for (map_str_pnode::iterator nI = pFabric->NodeByName.begin();
            nI != pFabric->NodeByName.end(); ++nI) {
        IBNode *pNode = (*nI).second;
        pNode->guid_set(++curGuid);
        fprintf(dumpFile, "NODE   %s 0x%016" PRIx64 "\n",
                pNode->name.c_str(), pNode->guid_get());

        /* go over all ports of the node and assign guid */
        for (unsigned int pn = 1; pn <= pNode->numPorts; pn++) {
            IBPort *pPort = pNode->getPort(pn);
            if (!pPort)
                continue;

            if (pNode->type == IB_SW_NODE)
                pPort->guid_set(curGuid);
            else {
                pPort->guid_set(++curGuid);
                fprintf(dumpFile, "PORT   %s 0x%016" PRIx64 "\n",
                        pPort->getName().c_str(), pPort->guid_get());
            }
        }

        /* assign system guids by first one */
        IBSystem *pSystem = pNode->p_system;
        if (pSystem && (pSystem->guid_get() == 0)) {
            pSystem->guid_set(++curGuid);
            fprintf(dumpFile, "SYSTEM %s 0x%016" PRIx64 "\n",
                    pSystem->name.c_str(), pSystem->guid_get());
        }
    }           //all nodes

    fclose(dumpFile);
    return 0;
}


/* initialize simulator nodes */
int
IBMgtSim::populateFabricNodes()
{
    MSGREG(msg1, 'I', "Populating Fabric Nodes ...", "server");
    MSGSND(msg1);

    /* simply go over all nodes and instantiate the sim node */
    for (map_str_pnode::iterator nI = pFabric->NodeByName.begin();
            nI != pFabric->NodeByName.end(); ++nI) {
        IBNode *pNode = (*nI).second;
        IBMSNode *pSimNode = new IBMSNode(this, pNode);
        ibmsSetIBNodeSimNode(pNode, pSimNode);

        list_uint16 smClassList;
        smClassList.push_back(0x1);
        smClassList.push_back(0x81);
        IBMSSma *pSma = new IBMSSma(pSimNode, smClassList);
        if (! pSma) {
            MSGREG(err1, 'F', "Fail to allocate SMA for Node:$", "server");
            MSGSND(err1, pNode->name);
            exit(1);
        }

        list_uint16 vsClassList;
        vsClassList.push_back(0x9);
        vsClassList.push_back(0x10);
        IBMSVendorSpecific *pVsa = new IBMSVendorSpecific(pSimNode, vsClassList);
        if (! pVsa) {
            MSGREG(err2, 'F', "Fail to allocate VSA for Node:$", "server");
            MSGSND(err2, pNode->name);
            exit(1);
        }

        IBMSPma *pPma = new IBMSPma(pSimNode, 0x04);
        if (! pPma) {
            MSGREG(err3, 'F', "Fail to allocate PMA for Node:$", "server");
            MSGSND(err3, pNode->name);
            exit(1);
        }
    }

    return 0;
}


/* Initialize the fabric server and dispatcher */
int
IBMgtSim::init(string topoFileName,
        int serverPortNum,
        int numWorkers)
{
    /* parse the given fabric */
    pFabric = new IBFabric;

    if (pFabric->parseTopology(topoFileName)) {
        cout << "-E- Fail to parse topology file:" << topoFileName << endl;
        exit(1);
    }

    /* allocate guids to the nodes */
    allocateFabricNodeGuids();

    /* initialize simulator nodes */
    populateFabricNodes();

    /* try to generate the server on the given port or next ones... */
    int trys = 0;
    do {
        pServer = new IBMSServer(this, serverPortNum++);
        if (pServer->isAlive())
            break;
        else
            delete pServer;
    } while (trys++ < 10);

    if (!pServer->isAlive())
        return 1;

    pDispatcher = new IBMSDispatcher(numWorkers, 50, 10);
    return 0;
}


/****************************************************/
/****************************************************/
/****************************************************/
static char IBMgtSimUsage[] =
        "Usage: ibmgtsim [-vh] -t <topology file> [-p <server port>][-w <num workers>]";

void
show_usage() {
    cout << IBMgtSimUsage << endl;
}

void
show_help() {
    cout << "HELP - TODO" << endl;
}

#ifndef IBMGTSIM_CODE_VERSION
    #define IBMGTSIM_CODE_VERSION "undefined"
#endif
const char * ibmsSourceVersion = IBMGTSIM_CODE_VERSION ;



/****************************************************/
/*********************** MAIN ***********************/
/****************************************************/
#ifdef BUILD_STANDALONE_SIM
int main(int argc, char **argv)
{
    /*
     * Parsing of Command Line
     */
    string TopoFile = string("");
    int numWorkers = 5;
    int serverPortNum = 42561;
    int verbosity = MsgDefault;

    char next_option;
    const char * const short_option = "vht:w:p:";
    /*
     * In the array below, the 2nd parameter specified the number
     * of arguments as follows:
     * 0: no arguments
     * 1: argument
     * 2: optional
     */
    const option long_option[] = {
            {	"verbose",	     0,	NULL,	'v'},
            {	"help",		     0,	NULL,	'h'},
            {	"port-num",	     1,	NULL,	'p'},
            {	"workers",       1,	NULL,	'w'},
            {	"topology",	     1,	NULL,	't'},
            {	NULL,		0,	NULL,	 0 }          /* Required at the end of the array */
         };

    printf("-----------------------------------------------------\n\n");
    printf("    Mellanox Technologies IB Management Simulator\n");
    printf("   -----------------------------------------------\n\n");

    do {
        next_option = getopt_long(argc, argv, short_option, long_option, NULL);
        switch(next_option) {
        case 'v':
            /*
             * Verbose Mode
             */
            verbosity = MsgShowAll;
            printf(" Verbose Mode\n");
            break;

        case 'p':
            /*
             * Specific Server Port
             */
            serverPortNum = atoi(optarg);
            printf(" Using Port:%u\n", serverPortNum);
            break;

        case 'w':
            /*
             * Specifies number of worker threads
             */
            numWorkers = atoi(optarg);
            printf(" Initializing %u mad dispatcher threads.\n", numWorkers);
            break;

        case 't':
            /*
             * Specifies Subnet Cabling file
             */
            TopoFile = string(optarg);
            printf(" Using topology file:%s.\n", TopoFile.c_str());
            break;

        case 'h':
            show_help();
            return 0;
            break;

        case -1:
            break; /* done with option */
        default: /* something wrong */
            show_usage();
            exit(1);
        }
    } while(next_option != -1);

    if (!TopoFile.size()) {
        printf("-E- Missing some mandatory arguments.\n");
        show_usage();
        exit(1);
    }

    /* initialize the logger */
    msgMgr(verbosity, &cout);

    IBMgtSim sim;
    sim.init(TopoFile, serverPortNum, numWorkers);

    while (1)
        sleep(1000);
    return 0;
}
#endif


