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

#include "Fabric.h"
#include "TraceRoute.h"
#include <set>
#include <algorithm>
#include <iomanip>

/*
 * Build a graph linked to the SW devices for input to output
 * links. We use the appData1 available on the nodes to ref the tables.
 * Based on this graph provide analysis on loops available on the
 * fabric
 *
 */
//////////////////////////////////////////////////////////////////////////////
//
// |------|  p1=p_port            |------| p2=p_portNext
// |      |                       |      |
// |    p1|>--------------------->|    p2|----------------
// |      |  \   channels         |      |   \   channels
// |------|   \--|=====|          |------|    \--|=====|
//               | VL0 |-|      next chan        | VL0 |
//               |-----| |     |========|        |-----|
//               | VL1 |  \    | P0-VL0 |   ---->| VL1 |
//               |-----|   \   |--------|  /     |-----|
//               | ... |    \->| P0-VL1 |-/      | ... |
//               |=====|  sl2vl|--------|        |=====|
//                             | P0-... |
//                             |========|
//                             | P1-VL0 |
//                             |--------|
//                             | P1-VL1 |
//                             |--------|
//                             | P1-... |
//                             |========|
//                             | ...... |
//                             |========|
//                             | Pn-VL0 |
//                             |========|
//
//////////////////////////////////////////////////////////////////////////////
using namespace std;


//////////////////////////////////////////////////////////////////////////////
// We keep global flags to control how the check is being done:

// If non zero will consider all switch to switch paths too
static int CrdLoopIncludeUcastSwitchPaths = 0;

// If non zero consider multicast paths too
static int CrdLoopIncludeMcastPaths = 0;

// Map each MLID to a list of SL's that may be used for this MGRP traffic
// If no entry is set then we will assume all traffic on SL=0
#define map_mlid_sl_list  map<int, list< int >, less<int> >
static map_mlid_sl_list mlidSLs;


//////////////////////////////////////////////////////////////////////////////
// Apply DFS on a dependency graph
int CrdLoopDFS(VChannel* ch)
{
    // Already been there
    if (ch->getFlag() == Closed)
        return 0;
    // Credit loop
    if (ch->getFlag() == Open) {
        cout << "Found credit loop on: " << ch->pPort->getName()
                 << " VL: " << ch->vl << endl;
        return 1;
    }
    // Mark as open
    ch->setFlag(Open);
    // Make recursive steps
    for (int i=0; i<ch->getDependSize();i++) {
        VChannel* next = ch->getDependency(i);
        if (next) {
            if (CrdLoopDFS(next)) {
                cout << "  - BT credit loop through: " << ch->pPort->getName()
                     << " VL: " << ch->vl << endl;
                return 1;
            }
        }
    }
    // Mark as closed
    ch->setFlag(Closed);
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
// Go over CA's apply DFS on the dependency graphs starting from CA's port
int CrdLoopFindLoops(IBFabric* p_fabric)
{
    unsigned int lidStep = 1 << p_fabric->lmc;

    // go over all CA ports in the fabric
    for (int i = p_fabric->minLid; i <= p_fabric->maxLid; i += lidStep) {
        IBPort *p_Port = p_fabric->PortByLid[i];
        if (!p_Port || (p_Port->p_node->type == IB_SW_NODE)) continue;
        // Go over all CA's channels and find untouched one
        for (int j=0;j < p_fabric->getNumSLs(); j++) {
            dfs_t state = p_Port->channels[j]->getFlag();
            if (state == Open) {
                cout << "-E- open channel outside of DFS" << endl;
                return 1;
            }
            // Already processed, continue
            if (state == Closed)
                continue;
            // Found starting point
            if (CrdLoopDFS(p_Port->channels[j]))
                return 1;
        }
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
// Trace a route from slid to dlid by LFT
// Add dependency edges
int CrdLoopMarkRouteByLFT(IBFabric *p_fabric,
        unsigned int sLid , unsigned int dLid)
{
    IBPort *p_port = p_fabric->getPortByLid(sLid);
    IBNode *p_node;
    IBPort *p_portNext;
    unsigned int lidStep = 1 << p_fabric->lmc;
    int outPortNum = 0, inputPortNum = 0, hopCnt = 0;
    bool done;

    // make sure:
    if (!p_port) {
        cout << "-E- Provided source:" << sLid
                << " lid is not mapped to a port!" << endl;
        return(1);
    }

    // If started on a switch, need to use the correct output
    // port, not the first one found by getPortByLid
    if (p_port->p_node->type == IB_SW_NODE) {
        int outPortNum = p_port->p_node->getLFTPortForLid(dLid);
        if (outPortNum == IB_LFT_UNASSIGNED) {
            cout << "-E- Unassigned LFT for lid:" << dLid
                    << " Dead end at:" << p_port->p_node->name << endl;
            return 1;
        }
        p_port = p_port->p_node->getPort(outPortNum);
    }

    // Retrieve the relevant SL
    uint8_t SL, VL;
    SL = VL = p_port->p_node->getPSLForLid(dLid);

    if (!p_port->p_remotePort) {
        cout << "-E- Provided starting point is not connected !"
                << "lid:" << sLid << endl;
        return 1;
    }

    if (SL == IB_SLT_UNASSIGNED) {
        cout << "-E- SL to destination is unassigned !"
                << "slid: " << sLid << "dlid:" << dLid << endl;
        return 1;
    }

    // check if we are done:
    done = ((p_port->p_remotePort->base_lid <= dLid) &&
            (p_port->p_remotePort->base_lid+lidStep - 1 >= dLid));
    while (!done) {
        // Get the node on the remote side
        p_node = p_port->p_remotePort->p_node;
        // Get remote port's number
        inputPortNum = p_port->p_remotePort->num;
        // Get number of ports on the remote side
        int numPorts = p_node->numPorts;
        // Init vchannel's number of possible dependencies
        p_port->channels[VL]->setDependSize((numPorts+1)*p_fabric->getNumVLs());

        // Get port num of the next hop
        outPortNum = p_node->getLFTPortForLid(dLid);
        // Get VL of the next hop
        int nextVL = p_node->getSLVL(inputPortNum,outPortNum,SL);

        if (outPortNum == IB_LFT_UNASSIGNED) {
            cout << "-E- Unassigned LFT for lid:" << dLid << " Dead end at:" << p_node->name << endl;
            return 1;
        }

        if (nextVL == IB_SLT_UNASSIGNED) {
            cout << "-E- Unassigned SL2VL entry, iport: "<< inputPortNum<<", oport:"<<outPortNum<<", SL:"<<(int)SL<< endl;
            return 1;
        }

        // get the next port on the other side
        p_portNext = p_node->getPort(outPortNum);

        if (! (p_portNext &&
                p_portNext->p_remotePort &&
                p_portNext->p_remotePort->p_node)) {
            cout << "-E- Dead end at:" << p_node->name << endl;
            return 1;
        }
        // Now add an edge
        p_port->channels[VL]->setDependency(
                outPortNum*p_fabric->getNumVLs()+nextVL,p_portNext->channels[nextVL]);
        // Advance
        p_port = p_portNext;
        VL = nextVL;
        if (hopCnt++ > 256) {
            cout << "-E- Aborting after 256 hops - loop in LFT?" << endl;
            return 1;
        }
        //Check if done
        done = ((p_port->p_remotePort->base_lid <= dLid) &&
                (p_port->p_remotePort->base_lid+lidStep - 1 >= dLid));
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Go over all CA to CA paths and connect dependant vchannel by an edge
int
CrdLoopConnectUcastDepend(IBFabric* p_fabric)
{
    unsigned int lidStep = 1 << p_fabric->lmc;
    int anyError = 0;
    unsigned int i,j;

    // go over all ports in the fabric
    for ( i = p_fabric->minLid; i <= p_fabric->maxLid; i += lidStep) {
        IBPort *p_srcPort = p_fabric->PortByLid[i];

        if (!p_srcPort)
            continue;
        if (!CrdLoopIncludeUcastSwitchPaths &&
                (p_srcPort->p_node->type == IB_SW_NODE))
            continue;

        unsigned int sLid = p_srcPort->base_lid;

        // go over all the rest of the ports:
        for ( j = p_fabric->minLid; j <= p_fabric->maxLid; j += lidStep ) {
            IBPort *p_dstPort = p_fabric->PortByLid[j];

            // Avoid tracing to itself
            if (i == j)
                continue;
            if (! p_dstPort)
                continue;
            if (!CrdLoopIncludeUcastSwitchPaths &&
                    (p_dstPort->p_node->type == IB_SW_NODE))
                continue;

            unsigned int dLid = p_dstPort->base_lid;
            // go over all LMC combinations:
            for (unsigned int l1 = 0; l1 < lidStep; l1++) {
                for (unsigned int l2 = 0; l2 < lidStep; l2++) {
                    // Trace the path but record the input to output ports used.
                    if (CrdLoopMarkRouteByLFT(p_fabric, sLid + l1, dLid + l2)) {
                        cout << "-E- Fail to find a path from:"
                                << p_srcPort->p_node->name << "/" << p_srcPort->num
                                << " to:" << p_dstPort->p_node->name << "/" << p_dstPort->num
                                << endl;
                        anyError++;
                    }
                }// all LMC lids 2 */
            } // all LMC lids 1 */
        } // all targets
    } // all sources

    if (anyError) {
        cout << "-E- Fail to traverse:"
                << anyError << " CA to CA paths" << endl;
        return 1;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Go over all Multicast Groups on all switches and add the dependecies
// they create to the dependency graph
// Return number of errors it found
int
CrdLoopConnectMcastDepend(IBFabric* p_fabric)
{
    // We support providing an SL list to each MLID by provining a special file
    // HACK: we can ignore connectivity check of the MCG and treat every switch
    // separately. The connectivity analysis can be run independently if loops
    // found.

    // HACK: the algorithm assumes constant SL2VL which is not port dependant!
    //       otherwise it should have been propagating traffic from each CA and SW on
    //       the MGRP such that the out VL of the previous port is known...
    //
    // Algorithm:
    // Foreach switch
    //   Create empty port-to-port P2P(sl,in,out) mask matrix for each SL
    //   Foreach MLID
    //     Foreach SL of MLID
    //        Copy the MFT(MLID) port mask to the matrix
    //   Foreach SL
    //      Lookup VL by SL - this is where the hack comes in handy
    //      Foreach in-port
    //         Foreach out-port
    //            Create the dependency edge (port-driving-in-port, VL, out-port, VL)

    int nErrs = 0;
    int addedEdges = 0;

    // foreach Switch
    for (map_str_pnode::const_iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end(); nI++) {
        IBNode *p_node = (*nI).second;

        // we only do MFT on switches
        if (p_node->type != IB_SW_NODE)
            continue;

        // allocate the array of dependencies:
        uint8_t sl_in_out_dep[16][p_node->numPorts+1][p_node->numPorts+1];
        memset(sl_in_out_dep, 0, sizeof(uint8_t)*16*(p_node->numPorts+1)*(p_node->numPorts+1));

        // foreach MLID
        for (unsigned int i = 0; i < p_node->MFT.size(); i++) {
            list<int> sls;

            // lookup SL's
            map_mlid_sl_list::const_iterator mlidI = mlidSLs.find(i+0xc000);
            if (mlidI != mlidSLs.end()) {
                sls = (*mlidI).second;
            } else {
                sls.push_back(0);
            }

            // now go over each SL at a time
            for (list<int>::const_iterator lI = sls.begin();
                    lI != sls.end();
                    lI++) {
                int sl = (*lI);
                // check all ports of the MFT
                uint64_t port_mask = p_node->MFT[i];
                for (unsigned int inPortNum = 1; inPortNum <= p_node->numPorts; inPortNum++) {
                    // we only care about port that are part of the MCG
                    if ((((uint64_t)1) << inPortNum) & port_mask) {
                        for (unsigned int outPortNum = 1; outPortNum <= p_node->numPorts; outPortNum++) {
                            if ((((uint64_t)1) << outPortNum) & port_mask) {
                                if (inPortNum != outPortNum) {
                                    sl_in_out_dep[sl][inPortNum][outPortNum] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }

        // now convert the dependency matrix into channel graph edges:
        // Foreach SL
        for (unsigned int sl = 0; sl < 16; sl++) {
            for (unsigned int inPortNum = 1; inPortNum <= p_node->numPorts; inPortNum++) {
                for (unsigned int outPortNum = 1; outPortNum <= p_node->numPorts; outPortNum++) {

                    if (sl_in_out_dep[sl][inPortNum][outPortNum] != 1)
                        continue;

                    // Lookup VL by SL
                    int vl = p_node->getSLVL(inPortNum,outPortNum,sl);

                    // Create the dependency edge (port-driving-in-port, VL, out-port, VL)
                    IBPort *p_outPort = p_node->getPort(outPortNum);
                    if (! p_outPort) {
                        cout << "-E- Switch:" << p_node->name << " port:" << outPortNum
                                << " is included in some MFT but is not connnected" << endl;
                        nErrs++;
                        continue;
                    }
                    IBPort *p_inPort = p_node->getPort(inPortNum);
                    if (! p_inPort) {
                        cout << "-E- Switch:" << p_node->name << " port:" << inPortNum
                                << " is included in some MFT but is not connnected" << endl;
                        nErrs++;
                        continue;
                    }
                    IBPort *p_drvPort = p_inPort->p_remotePort;
                    if (! p_drvPort) {
                        cout << "-E- Switch:" << p_node->name << " port:" << inPortNum
                                << " is included in some MFT but has no remote port." << endl;
                        nErrs++;
                        continue;
                    }

                    if (p_drvPort->p_node->type != IB_SW_NODE)
                        continue;

                    // Init vchannel's number of possible dependencies
                    p_drvPort->channels[vl]->setDependSize((p_node->numPorts+1)*p_fabric->getNumVLs());

                    // HACK: we assume the same VL was used entering to this node
                    p_drvPort->channels[vl]->setDependency(outPortNum*p_fabric->getNumVLs()+vl,
                                                                     p_outPort->channels[vl]);
                    addedEdges++;
                }
            }
        }
    }

    // Ref from LFT code:
    // outPortNum is the FBD port
    // get the next port on the other side
    // p_portNext = p_node->getPort(outPortNum);
    // Now add an edge
    // p_port->channels[VL]->setDependency(outPortNum*p_fabric->getNumVLs()+nextVL,
    //                 p_portNext->channels[nextVL]);
    cout << "-I- MFT added " << addedEdges << " edges to links dependency graph" << endl;
    return(nErrs);
}


//////////////////////////////////////////////////////////////////////////////
// Prepare the data model
int
CrdLoopPrepare(IBFabric *p_fabric)
{
    unsigned int lidStep = 1 << p_fabric->lmc;

    // go over all ports in the fabric
    for (int i = p_fabric->minLid; i <= p_fabric->maxLid; i += lidStep) {
        IBPort *p_Port = p_fabric->PortByLid[i];
        if (!p_Port)
            continue;
        IBNode *p_node = p_Port->p_node;
        int nL;
        if (p_node->type == IB_CA_NODE)
            nL = p_fabric->getNumSLs();
        else
            nL = p_fabric->getNumVLs();
        // Go over all node's ports
        for (int k=0;k<p_node->Ports.size();k++) {
            IBPort* p_Port = p_node->Ports[k];
            // Init virtual channel array
            p_Port->channels.resize(nL);
            for (int j=0;j<nL;j++)
                p_Port->channels[j] = new VChannel(p_Port, j);
        }
    }
    return 0;
}

// Cleanup the data model
int
CrdLoopCleanup(IBFabric *p_fabric)
{
    unsigned int lidStep = 1 << p_fabric->lmc;

    // go over all ports in the fabric
    for (int i = p_fabric->minLid; i <= p_fabric->maxLid; i += lidStep) {
        IBPort *p_Port = p_fabric->PortByLid[i];
        if (!p_Port)
            continue;
        IBNode *p_node = p_Port->p_node;
        int nL;
        if (p_node->type == IB_CA_NODE)
            nL = p_fabric->getNumSLs();
        else
            nL = p_fabric->getNumVLs();
        // Go over all node's ports
        for (int k=0;k<p_node->Ports.size();k++) {
            IBPort* p_Port = p_node->Ports[k];
            for (int j=0;j<nL;j++)
                if (p_Port->channels[j])
                    ;//delete p_Port->channels[j];
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
// Top Level Subroutine:
int
CrdLoopAnalyze(IBFabric *p_fabric)
{
    int res=0;
    cout << "-I- Analyzing Fabric for Credit Loops "
            << (int)p_fabric->getNumSLs() <<" SLs, "
            << (int)p_fabric->getNumVLs() << " VLs used." << endl;

    // Init data structures
    if (CrdLoopPrepare(p_fabric)) {
        cout << "-E- Fail to prepare data structures." << endl;
        return(1);
    }
    // Create the dependencies for unicast traffic
    if (CrdLoopConnectUcastDepend(p_fabric)) {
        cout << "-E- Fail to build dependency graphs." << endl;
        return(1);
    }
    // Do multicast if require
    if (CrdLoopIncludeMcastPaths) {
        if ( CrdLoopConnectMcastDepend(p_fabric) ) {
            cout << "-E- Fail to build multicast dependency graphs." << endl;
            return(1);
        }
    }

    // Find the loops if exist
    res = CrdLoopFindLoops(p_fabric);
    if (!res)
        cout << "-I- no credit loops found" << endl;
    else
        cout << "-E- credit loops in routing"<<endl;

    // cleanup:
    CrdLoopCleanup(p_fabric);
    return res;
}


//////////////////////////////////////////////////////////////////////////////
int
CredLoopMode(int include_switch_to_switch_paths, int include_multicast)
{
    CrdLoopIncludeUcastSwitchPaths = include_switch_to_switch_paths;
    CrdLoopIncludeMcastPaths = include_multicast;
    return 0;
}
