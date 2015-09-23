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

#ifndef IBMS_NODE_H
#define IBMS_NODE_H

/****h* IBMS/Node
* NAME
*	IB Management Simulator Node object and accompany Mad Processor
*
* DESCRIPTION
*	The simulator routes mad messages to the target node. This node
*  stores the device state (PM Counters, CR Space, etc). Pure virtual
*  MadProcessor class is provided to define the interface of mad processors.
*
* AUTHOR
*	Eitan Zahavi, Mellanox
*
*********/

#include <string>
#include <list>
#include <map>
#include <vector>
#include <algorithm>
#include <ibdm/Fabric.h>

typedef std::list< class IBMSMadProcessor * > list_mad_processor;
typedef std::vector< list_mad_processor > vec_mad_proc_list;
typedef std::list< uint16_t > list_uint16;
typedef std::list< uint8_t > list_uint8;


#include <iba/ib_types.h>
#include "simmsg.h"

inline class IBMSNode * ibmsGetIBNodeSimNode(IBNode *pIBNode) {
    return ((IBMSNode *)pIBNode->appData1.ptr);
};

inline void ibmsSetIBNodeSimNode(IBNode *pIBNode, class IBMSNode *pSimNode) {
    pIBNode->appData1.ptr = pSimNode;
};


/****************************************************/
/************ class IBMSPortErrProfile **************/
/****************************************************/
/*
 * This class holds the variables defining statistical
 * behavior of the port counters
 */
class IBMSPortErrProfile {
public:
    /* the rate of packets dropped from the total passed */
    float packetDropRate;

    /* the variance of the rate */
    float packetDropRateVar;

    /* to be able to filter we need to have some history */
    boolean_t drop;

    /* this variable counts the number of packet to pass until the next drop/pass
        random decision is made */
    unsigned int numPacketToNextChange;

    /* total number of packets passed */
    uint64_t numPackets;

    /* constructor */
    IBMSPortErrProfile() {
        packetDropRate = packetDropRateVar = 0;
        numPacketToNextChange = 1;      /* force first decision */
        numPackets = 0ULL;
    };

    /* checks if needs to drop current packet - also increases
        the packet count */
    boolean_t isDropped();
};


/****************************************************/
/************* class IBMSMadProcessor ***************/
/****************************************************/
/*
 * Mad processor class is a pure virtual class that
 * supports handling of mads of specific classes
 */
class IBMSMadProcessor {
protected:
    /* a pointer back to the sim node object */
    class IBMSNode *pSimNode;

    list_uint16 mgtClasses;

    /* add a single mad processor to the sim node */
    void addProcToNode(uint16_t mgtClass);

public:
    /* Actually handle the mad. Might result with a call to the
        outstandingMads->push() with a result                     */
    virtual int processMad(uint8_t inPort, ibms_mad_msg_t &madMsg) {
        return(0);
    };

    /* constructors - should register class in the  in the node. */
    IBMSMadProcessor(class IBMSNode *pSNode, uint16_t mgtClass, boolean_t preLocked = FALSE);
    IBMSMadProcessor(class IBMSNode *pSNode, list_uint16 &mgtClasses, boolean_t preLocked = FALSE);

    /* destructor - clean up from the node too */
    virtual ~IBMSMadProcessor();
};


/****************************************************/
/***************** class IBMSNode *******************/
/****************************************************/
/*
 * Every IB node have this simulator node attached
 */
class IBMSNode {
private:
    /* Holds the lists of mad processors per mgt class */
    vec_mad_proc_list madProccessors;

    /* back pointer to the simulator main object */
    class IBMgtSim *pSim;

    /* the fabric node we attach to. */
    class IBNode *pNode;

    /* a vector holding the performance management counters for the node */
    std::vector < ib_pm_counters_t > phyPortCounters;

    /* a vector holding the error statistics for the particular port */
    std::vector < class IBMSPortErrProfile > phyPortErrProfiles;

    /* node lock : used to avoid races from changes to the node and usage */
    pthread_mutex_t lock;

public:
    ib_switch_info_t switchInfo;
    ib_node_info_t nodeInfo;
    std::vector < ib_port_info_t > nodePortsInfo;
    std::vector < std::vector < ib_guid_info_t > > nodeGuidsInfo;
    std::vector < std::vector < ib_pkey_table_t > > nodePortPKeyTable;
    std::vector < std::vector < ib_mft_table_t > > switchMftPortsEntry;
    std::vector < ib_slvl_table_t > sl2VlOutPortEntry;
    std::vector < std::vector < ib_slvl_table_t > > sl2VlInPortEntry;
    std::vector < ib_vl_arb_table_t > vlArbPriority;
    std::vector < std::vector < ib_vl_arb_table_t > > vlArbPortEntry;
    /* thy node CR space - implemented as a map to be non continues */
    std::map < uint32_t , uint32_t, std::less < uint32_t > > crSpace;

    /* constructor */
    IBMSNode(class IBMgtSim *pS, class IBNode *pN);

    /* avoid friendship: */
    IBNode *getIBNode() {return pNode;};

    IBMgtSim *getSim() { return pSim;};

    /* get the link status of the given port */
    int getLinkStatus(uint8_t outPort) {
        if (nodePortsInfo.size() <= outPort)
            return IB_LINK_DOWN;
        return ib_port_info_get_port_state(&(nodePortsInfo[outPort]));
    };

    /* set the link status of the given port */
    int setLinkStatus(uint8_t portNum, uint8_t newState);

    /* handle incoming mad by sending it to the processMad of every
        IBMSMadProcessor on the node. */
    int processMad(uint8_t inPort, ibms_mad_msg_t &madMsg);

    /* set a particular port err profile */
    int setPhyPortErrProfile(uint8_t portNum, IBMSPortErrProfile &errProfile);

    /* get a particular port err profile */
    int getPhyPortErrProfile(uint8_t portNum, IBMSPortErrProfile &errProfile);

    /* set a specific port counter */
    int setPhyPortPMCounter(uint8_t portNum, uint32_t counterSelect,
            ib_pm_counters_t &countersVal);

    /* get a specific port counter */
    ib_pm_counters_t * getPhyPortPMCounter(uint8_t portNum, uint32_t counterSelect);

    /* set CR Space Value */
    int setCrSpace(uint32_t startAddr,uint32_t length,uint32_t data[] );

    /* get CR Space Value */
    int getCrSpace(uint32_t startAddr,uint32_t length,uint32_t data[] );

    /* get MFT block */
    int getMFTBlock(uint16_t blockIdx, uint8_t portIdx, ib_mft_table_t *outMftBlock);

    /* set MFT block */
    int setMFTBlock(uint16_t blockIdx, uint8_t portIdx, ib_mft_table_t *inMftBlock);

    /* get a specific port info */
    ib_port_info_t * getPortInfo(uint8_t portNum) {
        if (portNum >= nodePortsInfo.size()) return NULL;
        return &nodePortsInfo[portNum];
    };

    /* get the node info */
    ib_node_info_t * getNodeInfo() {
        return &nodeInfo;
    };

    /* get the switch info */
    ib_switch_info_t *getSwitchInfo() {
        return &switchInfo;
    };

    /* get GuidInfo table block */
    ib_guid_info_t *getGuidInfoBlock(uint8_t portNum, uint16_t blockNum) {
        if (portNum >= nodeGuidsInfo.size()) {
            printf("-E- Node:%s given port number out of range:%u > %u\n",
                    pNode->name.c_str(), portNum, (unsigned int)(nodeGuidsInfo.size() - 1));
            return NULL;
        }
        if (blockNum >= nodeGuidsInfo[portNum].size()) {
            printf("-E- Node:%s port:%u given GuidInfo block number out of range:%u > %u\n",
                    pNode->name.c_str(), portNum,
                    blockNum, (unsigned int)(nodeGuidsInfo[portNum].size() - 1));
            return NULL;
        }
        return &((nodeGuidsInfo[portNum])[blockNum]);
    }

    /* set GuidInfo table block */
    int setGuidInfoBlock(uint8_t portNum, uint16_t blockNum,
            ib_guid_info_t *tbl) {
        if (portNum >= nodeGuidsInfo.size()) {
            printf("-E- Given port number out of range:%u > %u\n",
                    portNum, (unsigned int)(nodeGuidsInfo.size() - 1));
            return 1;
        }
        if (blockNum >= nodeGuidsInfo[portNum].size()) {
            ib_guid_info_t emptyTable;
            memset(&emptyTable, 0, sizeof(ib_guid_info_t));
            for(uint16_t i = nodeGuidsInfo[portNum].size();
                    i <= blockNum; i++)
                nodeGuidsInfo[portNum].push_back(emptyTable);
        }
        (nodeGuidsInfo[portNum])[blockNum] = *tbl;
        return(0);
    };

    /* get pkey table block */
    ib_pkey_table_t *getPKeyTblBlock(uint8_t portNum, uint16_t blockNum) {
        if (portNum >= nodePortPKeyTable.size()) {
            printf("-E- Node:%s given port number out of range:%u > %u\n",
                    pNode->name.c_str(), portNum, (unsigned int)(nodePortPKeyTable.size() - 1));
            return NULL;
        }
        if (blockNum >= nodePortPKeyTable[portNum].size()) {
            printf("-E- Node:%s port:%u given pkey block number out of range:%u > %u\n",
                    pNode->name.c_str(), portNum,
                    blockNum, (unsigned int)(nodePortPKeyTable[portNum].size() - 1));
            return NULL;
        }
        return &((nodePortPKeyTable[portNum])[blockNum]);
    }

    /* set pkey table block */
    int setPKeyTblBlock(uint8_t portNum, uint16_t blockNum,
            ib_pkey_table_t *tbl) {
        if (portNum >= nodePortPKeyTable.size()) {
            printf("-E- Given port number out of range:%u > %u\n",
                    portNum, (unsigned int)(nodePortPKeyTable.size() - 1));
            return 1;
        }

        if (blockNum >= nodePortPKeyTable[portNum].size()) {
            ib_pkey_table_t emptyTable;
            memset(&emptyTable, 0, sizeof(ib_pkey_table_t));
            for( uint16_t i = nodePortPKeyTable[portNum].size();
                    i <= blockNum; i++)
                nodePortPKeyTable[portNum].push_back(emptyTable);
        }
        (nodePortPKeyTable[portNum])[blockNum] = *tbl;
        return(0);
    };

    /* get a specific SL2VL Table */
    ib_slvl_table_t * getSL2VLTable(uint8_t inPortNum, uint8_t outPortNum) {
        if (inPortNum >= sl2VlInPortEntry.size()) {
            printf("-E- getSL2VLTable Node:%s in-port:%u > %u\n",
                    pNode->name.c_str(), inPortNum,
                    (unsigned int)(sl2VlInPortEntry.size() - 1));
            return NULL;
        }
        if (outPortNum >= sl2VlInPortEntry[inPortNum].size()) {
            printf("-E- getSL2VLTable Node:%s out-port:%u > %u\n",
                    pNode->name.c_str(), outPortNum,
                    (unsigned int)(sl2VlInPortEntry[inPortNum].size() - 1));
            return NULL;
        }
        return &sl2VlInPortEntry[inPortNum][outPortNum];
    };

    /* set a specific SL2VL Table */
    int setSL2VLTable(uint8_t inPortNum, uint8_t outPortNum,
            ib_slvl_table_t *tbl) {
        if (inPortNum >= sl2VlInPortEntry.size()) {
            printf("-E- setSL2VLTable Node:%s in-port:%u > %u\n",
                    pNode->name.c_str(), inPortNum,
                    (unsigned int)(sl2VlInPortEntry.size() - 1));
            return 1;
        }
        if (outPortNum >= sl2VlInPortEntry[inPortNum].size()) {
            printf("-E- setSL2VLTable Node:%s out-port:%u > %u\n",
                    pNode->name.c_str(), outPortNum,
                    (unsigned int)(sl2VlInPortEntry[inPortNum].size() - 1));
            return 1;
        }
        (sl2VlInPortEntry[inPortNum])[outPortNum] = *tbl;
        return 0;
    };

    /* get a specific VLArb Table */
    ib_vl_arb_table_t * getVLArbLTable(uint8_t portNum, uint8_t blockIndex) {
        if ((blockIndex < 1) || (blockIndex > 4)) {
            printf("-E- getVLArbLTable blockIndex:%u out of range 1..4\n", blockIndex);
            return NULL;
        }
        if (portNum >= vlArbPortEntry.size()) {
            printf("-E- getVLArbLTable Node:%s port-num:%u > %u\n",
                    pNode->name.c_str(), portNum,
                    (unsigned int)(vlArbPortEntry.size() - 1));
            return NULL;
        }
        return &vlArbPortEntry[portNum][blockIndex];
    };

    /* set a specific SL2VL Table */
    int setVLArbLTable(uint8_t portNum, uint8_t blockIndex,
            ib_vl_arb_table_t *tbl) {
        if ((blockIndex < 1) || (blockIndex > 4)) {
            printf("-E- setVLArbLTable blockIndex:%u out of range 1..4\n", blockIndex);
            return 1;
        }
        if (portNum >= vlArbPortEntry.size()) {
            printf("-E- getVLArbLTable Node:%s port-num:%u > %u\n",
                    pNode->name.c_str(), portNum,
                    (unsigned int)(vlArbPortEntry.size() - 1));
            return 1;
        }
        (vlArbPortEntry[portNum])[blockIndex] = *tbl;
        return 0;
    };

    /*
        Get remote node by given port number.
        Handle both HCA and SW.

        Return either 0 if step could be made or 1 if failed.

        Updated both output pointers:
        remNode - to the Sim Node of the other side
        remIBPort - to the remote side IB Fabric Port object on the other side.
     */
    int getRemoteNodeByOutPort(
            uint8_t outPortNum,
            IBMSNode **ppRemNode,
            IBPort **ppRemIBPort, int isVl15 = 0);

    /*
        Get remote node by lid. Handle both HCA and SW
        we can also simply provide pointer to the remote port as we assume
        no topology changes are done after init of the fabric
     */
    int getRemoteNodeByLid(uint16_t lid,
            IBMSNode **ppRemNode, IBPort **ppRemIBPort, int isVl15 = 0);

    /* for the sake of updating the map of mgtClass to list of handlers */
    friend class IBMSMadProcessor;
    friend class IBMSServer;
};


#endif /* IBMS_NODE_H */

