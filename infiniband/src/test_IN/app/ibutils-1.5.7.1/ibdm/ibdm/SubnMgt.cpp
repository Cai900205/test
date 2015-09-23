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


/*
    Subnet Utilities:

    The file holds a set of utilities to be run on the subnet to mimic OpenSM
    initialization and analyze the results:

    Assign Lids: SubnMgtAssignLids
    Init min hop tables: SubnMgtCalcMinHopTables
    Perform Enhanced LMC aware routing: SubnMgtOsmEnhancedRoute
    Perform standard routing: SubnMgtOsmRoute
    Verify all CA to CA routes: SubnMgtVerifyAllCaToCaRoutes
*/


#include "Fabric.h"
#include "TraceRoute.h"
#include "Regexp.h"
#include <set>
#include <algorithm>
#include <iomanip>


///////////////////////////////////////////////////////////////////////////////
// Assign lids given the start NodePort
int
SubnMgtAssignLids (IBPort *p_smNodePort, unsigned int lmc = 0)
{
    list<IBPort *> thisStepPorts;
    list<IBPort *> nextStepNodePorts;
    set<IBNode *, less<IBNode *> > visited;
    unsigned int i;
    IBFabric *p_fabric = p_smNodePort->p_node->p_fabric;
    IBPort *p_port;
    IBNode *p_node;
    IBPort *p_remPort;
    IBNode *p_remNode;
    unsigned int numLidsPerPort = (1 << lmc);

    thisStepPorts.push_back(p_smNodePort);

    unsigned int lid = 1, l;
    int step = 0;

    // BFS Style ...
    while (thisStepPorts.size() > 0) {
        nextStepNodePorts.clear();
        step++;

        // go over all this step ports
        while (! thisStepPorts.empty()) {
            p_port = thisStepPorts.front();
            thisStepPorts.pop_front();

            // get the node
            p_node = p_port->p_node;

            // just making sure since we can get on the BFS from several sides ...
            if (visited.find(p_node) != visited.end())
                continue;

            // mark as visited
            visited.insert(p_node);

            // based on the node type we do the recursion and assignment
            switch (p_node->type) {
            case IB_CA_NODE:
                // simple as we stop here
                p_port->base_lid = lid;
                for (l = lid ; l <= lid + numLidsPerPort; l ++)
                    p_fabric->setLidPort(l, p_port);
                    //We do not assign all the lids - just the base lid
                    //for (l = lid ; l <= lid + numLidsPerPort; l ++)
                    //    p_fabric->setLidPort(l, p_port);
                break;
            case IB_SW_NODE:
                // go over all ports of the node:
                for (i = 0; i < p_node->numPorts; i++)
                    if (p_node->Ports[i]) {
                        p_node->Ports[i]->base_lid = lid;
                        for (l = lid ; l <= lid + numLidsPerPort; l ++)
                            p_fabric->setLidPort(l, p_node->Ports[i]);
                    }
                break;
            default:
                cout << "-E- Un recognized node type: " << p_node->type
                << " name:"  <<  endl;
            }

            // do not forget to increment the lids
            lid = lid + numLidsPerPort;

            // now recurse
            for (i = 0; i < p_node->numPorts; i++) {
                if (p_node->Ports[i] == NULL) continue;

                // if we have a remote port that is not visited
                p_remPort = p_node->Ports[i]->p_remotePort;
                if (p_remPort != NULL) {
                    p_remNode = p_remPort->p_node;
                    // if the node was not visited and not included in
                    // next steps already
                    if ( (visited.find(p_remNode) == visited.end()) &&
                            (find(nextStepNodePorts.begin(),
                                    nextStepNodePorts.end(),p_remPort)
                                    == nextStepNodePorts.end()) )
                        nextStepNodePorts.push_back(p_remPort);
                }
            }
        }

        thisStepPorts = nextStepNodePorts;
    }

    lid = lid - numLidsPerPort;
    p_fabric->maxLid = lid;
    p_fabric->minLid = 1;
    p_fabric->lmc = lmc;
    cout << "-I- Assigned " << lid << " LIDs (lmc=" << lmc
            << ") in " << step << " steps" << endl;
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// OpenSM style relaxation algorithm:
// First step is to mark your own or neighbors
// Loop updating from neighbors hops untill no update
int
SubnMgtCalcMinHopTables (IBFabric *p_fabric)
{
    IBNode *p_node;
    map_str_pnode::iterator nI;
    unsigned int lid;
    unsigned lidStep = 1 << p_fabric->lmc;

    // first step
    // - update self on switches:
    // - update neighbor for CAs:
    // go over all nodes in the fabric
    for( nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_node = (*nI).second;

        // we should not assign hops for non SW nodes:
        if (p_node->type == IB_SW_NODE) {
            lid = 0;
            // switch lids are identical to all ports
            // get the lid of the first available port
            for (unsigned int i = 0; (lid == 0) && (i< p_node->numPorts); i++) {
                if (p_node->Ports[i])
                    lid = p_node->Ports[i]->base_lid;
            }

            // assign all ports value
            p_node->setHops(NULL,lid,0);
        } else {
            // a non switch node might have connections on both ports
            // and also we just want to update the switch on the other side
            int conPorts = 0;
            for (unsigned int i = 0; i< p_node->numPorts; i++) {
                IBPort *p_port = p_node->Ports[i];
                if (p_port &&
                        p_port->p_remotePort &&
                        p_port->p_remotePort->p_node->type == IB_SW_NODE) {
                    lid = p_port->base_lid;
                    conPorts++;
                    // update the switch:
                    p_port->p_remotePort->p_node->setHops(p_port->p_remotePort,lid,1);
                }
            }
            if (!conPorts)
                cout << "-W- CA with no connected ports:" << p_node->name << endl;
        }
    }

    // second - loop until nothing to update:
    int anyUpdate;
    int loop = 0;
    do {
        loop++;
        anyUpdate = 0;

        // go over all switch nodes:
        for( nI = p_fabric->NodeByName.begin();
                nI != p_fabric->NodeByName.end();
                nI++) {
            p_node = (*nI).second;

            // we should not assign hops for non SW nodes:
            if (p_node->type != IB_SW_NODE)
                continue;

            // go over all lids (base) on this switch:
            for (unsigned int bLid = 1;
                    bLid <= p_fabric->maxLid;
                    bLid += lidStep) {
                // go over all connected ports
                for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                    IBPort *p_port = p_node->getPort(pn);

                    // do we have a port on the other side ? is it a SW ?
                    if (p_port &&
                            p_port->p_remotePort &&
                            (p_port->p_remotePort->p_node->type == IB_SW_NODE)) {
                        // the min we have for this lid is:
                        int minHops = p_node->getHops(p_port, bLid);

                        // we need to update the local port hops only they will
                        // be made smaller by this step. I.e. the remote port
                        // hops value + 1 is < hops
                        int remNodeHops =
                                p_port->p_remotePort->p_node->getHops(NULL, bLid);

                        if (remNodeHops + 1 < minHops) {
                            // need to update:
                            p_node->setHops(p_port, bLid, remNodeHops + 1);
                            anyUpdate++;
                        }
                    }
                }
            }
        }
        // cout << "-I- Propagated:" << anyUpdate << " updates" << endl;
    } while (anyUpdate);
    cout << "-I- Init Min Hops Tables in:" << loop << " steps" << endl;

    // simply check that one can reach to all lids from all switches:
    // also we build a historgram of the number of ports one can use
    // to get to any lid.
    vec_int numPathsHist(50,0);
    int anyUnAssigend = 0;
    for( nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_node = (*nI).second;
        if (p_node->type == IB_CA_NODE)
            continue;

        // go over all the lids.
        for (unsigned int i = 1; i <= p_fabric->maxLid; i += lidStep ) {
            // skip lids that are not mapped to a port:
            if (! p_fabric->PortByLid[i])
                continue;

            int minHops = p_node->getHops(NULL, i);
            if (minHops == IB_HOP_UNASSIGNED) {
                cout << "-W- Found - un-assigned hops for node:"
                        << p_node->name << " to lid:" << i << ")" << endl;
                anyUnAssigend++;
            } else {
                // count all ports that have min hops to that lid (only HCAs count)
                IBPort *p_targetPort = p_fabric->getPortByLid(i);
                if (p_targetPort && (p_targetPort->p_node->type != IB_SW_NODE)) {
                    int numMinHopPorts = 0;
                    for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                        IBPort *p_port = p_node->getPort(pn);
                        if (p_port && (p_node->getHops(p_port, i) == minHops))
                            numMinHopPorts++;
                    }
                    numPathsHist[numMinHopPorts]++;
                }
            }
        }
    }

    cout << "------------------ NUM ALTERNATE PORTS TO CA HISTOGRAM --------------------" << endl;
    cout << "Describes how many out ports on every switch have the same Min Hop to each " << endl;
    cout << "target CA. Or in other words how many alternate routes are possible at the " << endl;
    cout << "switch level. This is useful to show the symmetry of the cluster.\n" << endl;
    cout << "OUT-PORTS NUM-SW-LID-PAIRS" << endl;
    for (int b = 0; b < 50 ; b++)
        if (numPathsHist[b])
            cout << setw(4) << b << "   " << numPathsHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;

    if (anyUnAssigend) {
        cout << "-W- Found - un-reachable lids." << endl;
        return 1;
    }

    // report the worst case hops count found.
    int maxHops = 0;
    IBNode *p_worstHopNode  = NULL;
    IBPort *p_worstHopPort  = NULL;
    unsigned int worstHopLid;
    IBNode *p_caNode;
    IBPort *p_caPort;
    vec_int maxHopsHist(50,0);

    for( nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_caNode = (*nI).second;
        if (p_caNode->type != IB_CA_NODE)
            continue;

        // find the switch connected to the HCA port
        for (unsigned int n = 1; n <= p_caNode->numPorts; n++) {
            p_caPort = p_caNode->getPort(n);
            // ignore the port if does not exist or not connected
            if (!p_caPort || !p_caPort->p_remotePort)
                continue;

            p_node = p_caPort->p_remotePort->p_node;

            // go over all lids found and get the min hop
            for (unsigned int i = 1; i <= p_fabric->maxLid; i += lidStep ) {
                // please ignore non CA lids and ourselves
                IBPort *p_port = p_fabric->PortByLid[i];
                if (p_port && (p_port->p_node->type == IB_CA_NODE) &&
                        (p_caPort != p_port)) {
                    int minHops = p_node->getHops(NULL, i);
                    if (IB_HOP_UNASSIGNED != minHops) {
                        maxHopsHist[minHops]++;
                        if (minHops > maxHops) {
                            p_worstHopPort = p_node->getFirstMinHopPort(i);
                            p_worstHopNode = p_node;
                            worstHopLid = i;
                            maxHops = minHops;
                        }
                    } else {
                        cout << "-W- Found - un-assigned hop for node:"
                                << p_node->name << " to port:" << p_port->p_node->name
                                << "/" << p_port->num
                                << " (lid:" << i << ")" << endl;
                    }
                }
            }
        }
    }

    // print the histogram:
    cout << "---------------------- CA to CA : MIN HOP HISTOGRAM -----------------------" << endl;
    cout << "The number of CA pairs that are in each number of hops distance." << endl;
    cout << "The data is based on topology only - even before any routing is run.\n" << endl;
    cout << "HOPS NUM-CA-CA-PAIRS" << endl;
    for (int b = 0; b <= maxHops ; b++)
        if (maxHopsHist[b])
            cout << setw(3) << b+1 << "   " << maxHopsHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;

    if (p_worstHopNode) {
        cout << "-I- Found worst min hops:" << maxHops + 1 << " at node:"
                << p_worstHopNode->name << " to node:"
                << p_fabric->PortByLid[worstHopLid]->p_node->name << endl;

        // the worst hop node lid
        TraceRouteByMinHops(p_fabric, p_worstHopPort->base_lid, worstHopLid);
    }
    return(0);
}


///////////////////////////////////////////////////////////////////////////////
// Fill in the FDB tables in an Extended OpesnSM style routing
// which is switch based, uses number of routes per port
// profiling and treat LMC assigned lids sequentialy
// Rely on running the SubnMgtCalcMinHopTables beforehand
// We added the notion of selecting the other system or node
// if same hop and profile
int
SubnMgtOsmEnhancedRoute(IBFabric *p_fabric)
{
    IBNode *p_node;
    cout << "-I- Using Enhanced OpenSM Routing" << endl;

    // we want to collect port subscriptions statistics:
    vec_int subscHist(10000,0);

    // also track the selections used:
    int numSelByOtherSys = 0;
    int numSelByOtherNode = 0;
    int numSelByMinSubsc = 0;

    // go over all nodes in the fabric
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_node = (*nI).second;

        // if not a switch cont
        if (p_node->type != IB_SW_NODE)
            continue;

        // define port profiles
        vec_int portsSubscriptions(p_node->numPorts,0);
        int lidStep = 1 << p_fabric->lmc;

        // go over all valid lid values (i.e. base lids )
        for (unsigned int bLid = 1; bLid <= p_fabric->maxLid;
                bLid += lidStep) {
            int targetIsHCA;
            IBPort *pTargetPort = p_fabric->PortByLid[bLid];
            if (pTargetPort && (pTargetPort->p_node->type == IB_SW_NODE))
                targetIsHCA = 0;
            else
                targetIsHCA = 1;

            // get the minimal hop count from this port:
            int minHop = p_node->getHops(NULL, bLid);

            // We track the Systems we already went through:
            set<IBSystem *> goThroughSystems;

            // we also track the nodes we went through:
            set <IBNode *> goThroughNodes;

            // loop on every LMC value:
            for (int lmcValue = 0; lmcValue < lidStep; lmcValue++) {
                // if same assign 0
                unsigned int lid = 0;
                for (unsigned int i = 0; (lid == 0) && (i< p_node->numPorts); i++) {
                    if (p_node->Ports[i])
                        lid = p_node->Ports[i]->base_lid;
                }
                // same lid so no routing needed!
                if (lid == bLid) {
                    p_node->setLFTPortForLid( bLid + lmcValue, 0);
                    continue;
                }

                // we are going to track the min profile under three
                // possible cases:
                int minSubsShared = 100000;
                int minSubsDiffNodes = 100000;
                int minSubsDiffSystems = 100000;
                unsigned int minPortNumShared = 0;
                unsigned int minPortNumDiffNodes = 0;
                unsigned int minPortNumDiffSystems = 0;
                int minSubs;

                if (minHop < 255) {
                    // look for the port with min profile
                    for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                        IBPort *p_port = p_node->getPort(pn);
                        if (! p_port)
                            continue;
                        if (! p_port->p_remotePort)
                            continue;

                        // the hops should match the min
                        if (p_node->getHops(p_port,bLid) == minHop) {
                            minSubs = portsSubscriptions[pn-1];
                            IBNode   *p_remNode = p_port->p_remotePort->p_node;
                            IBSystem *p_system = p_remNode->p_system;

                            if (goThroughSystems.find(p_system) == goThroughSystems.end()) {
                                if (minSubsDiffSystems > minSubs) {
                                    minSubsDiffSystems = minSubs;
                                    minPortNumDiffSystems = pn;
                                }
                            }
                            if (goThroughNodes.find(p_remNode) == goThroughNodes.end()) {
                                if (minSubsDiffNodes > minSubs) {
                                    minSubsDiffNodes = minSubs;
                                    minPortNumDiffNodes = pn;
                                }
                            }

                            if (minSubsShared > minSubs) {
                                minSubsShared = minSubs;
                                minPortNumShared = pn;
                            }
                        } // hop = min hops
                    } // all ports

                    // we always select the system over node over shared:
                    if (minPortNumDiffSystems) {
                        minPortNumShared = minPortNumDiffSystems;
                        numSelByOtherSys++;
                    } else if (minPortNumDiffNodes) {
                        numSelByOtherNode++;
                        minPortNumShared = minPortNumDiffNodes;
                    } else {
                        numSelByMinSubsc++;
                    }

                    // so now we need to have the port number or error
                    if (!minPortNumShared) {
                        cout << "-E- Cound not find min hop port!" << endl;
                        return(1);
                    }

                    // Track used systems and nodes
                    IBPort *p_bestPort = p_node->getPort(minPortNumShared);
                    IBNode *p_remNode = p_bestPort->p_remotePort->p_node;
                    IBSystem *p_system = p_node->p_system;

                    goThroughSystems.insert(p_system);
                    goThroughNodes.insert(p_remNode);

                } else {
                    // there is no path to that lid...
                    minPortNumShared = 255;
                }

                // track subscriptions:
                if (targetIsHCA)
                    portsSubscriptions[minPortNumShared-1]++;

                // assign the fdb table.
                p_node->setLFTPortForLid(bLid + lmcValue, minPortNumShared);
            } // all port lids
        } // all lids

        // we want to get some histogram of subsriptions.
        for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
            IBPort *p_port = p_node->getPort(pn);
            if (p_port && p_port->p_remotePort) {
                if (portsSubscriptions[pn-1] == 0) {
                    cout << "-W- Unused port:" << p_port->getName() << endl;
                }
                subscHist[portsSubscriptions[pn-1]]++;
            }
        }
    } // all nodes
#if 0
    // print the histogram:
    cout << "----------------------- LINK SUBSCRIPTIONS HISTOGRAM ----------------------" << endl;
    cout << "Distribution of number of LIDs mapped to each switch out port. Note that " << endl;
    cout << "this assumes every LID is routed through every switch which is not correct" << endl;
    cout << "if one ignores the switch to CA paths.\n" << endl;
    cout << "NUM-LIDS COUNT" << endl;
    for (unsigned int b = 0; b < 1024 ; b++)
        if (subscHist[b])
            cout << setw(7) << b << "   " << subscHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;
#endif
    cout << "-I- Enhanced selection by Sys:" << numSelByOtherSys
            << " Node:" << numSelByOtherNode
            << " Subscription:" << numSelByMinSubsc << endl;
   return(0);
}


///////////////////////////////////////////////////////////////////////////////
// Fill in the FDB tables in an OpesnSM style routing
// which is switch based, uses number of routes per port
// profiling and treat LMC assigned lids sequentialy
// Rely on running the SubnMgtCalcMinHopTables beforehand
int
SubnMgtOsmRoute(IBFabric *p_fabric)
{
    IBNode *p_node;
    cout << "-I- Using standard OpenSM Routing" << endl;

    // we want to collect port subscriptions statistics:
    vec_int subscHist(10000,0);

    // go over all nodes in the fabric
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_node = (*nI).second;

        // if not a switch cont
        if (p_node->type != IB_SW_NODE)
            continue;

        // define port profiles
        vec_int portsSubscriptions(p_node->numPorts,0);
        int lidStep = 1 << p_fabric->lmc;

        // go over all valid lid values (i.e. base lids )
        for (unsigned int bLid = 1; bLid <= p_fabric->maxLid;
                bLid += lidStep) {
            int targetIsHCA;
            IBPort *pTargetPort = p_fabric->getPortByLid(bLid);
            if (pTargetPort && (pTargetPort->p_node->type == IB_SW_NODE))
                targetIsHCA = 0;
            else
                targetIsHCA = 1;

            // get the minimal hop count from this port:
            int minHop = p_node->getHops(NULL,bLid);

            // We track the Systems we already went through:
            set<IBSystem *> goThroughSystems;

            // we also track the nodes we went through:
            set <IBNode *> goThroughNodes;

            // loop on every LMC value:
            for (int lmcValue = 0; lmcValue < lidStep; lmcValue++) {
                // if same assign 0
                unsigned int lid = 0;
                for (unsigned int i = 0; (lid == 0) && (i< p_node->numPorts); i++) {
                    if (p_node->Ports[i])
                        lid = p_node->Ports[i]->base_lid;
                }
                if (lid == bLid) {
                    p_node->setLFTPortForLid( bLid + lmcValue, 0);
                    continue;
                }

                // initialize the min subsription to a huge number:
                int minSubsc = 100000;
                unsigned int minSubsPortNum = 0;

                // look for the port with min profile
#if 1
                if (minHop != 255) {
                    for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                        IBPort *p_port = p_node->getPort(pn);

                        if (! p_port)
                            continue;

                        // the hops should match the min
                        if (p_node->getHops(p_port, bLid) == minHop) {
                            // Standard OpenSM Routing:
                            // is it the lowest subscribed port:
                            if (portsSubscriptions[pn-1] < minSubsc) {
                                minSubsPortNum = pn;
                                minSubsc = portsSubscriptions[pn-1];
                            }
                        }
                    }
                } else {
                    minSubsPortNum = 255;
                }
#else
                // do a random selection
                {
                    vector<unsigned int> minHopOutPorts;
                    for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                        IBPort *p_port = p_node->getPort(pn);

                        if (! p_port)
                            continue;

                        // the hops should match the min
                        if (p_node->getHops(p_port, bLid) == minHop) {
                            minHopOutPorts.push_back(pn);
                        }
                    }
                    double portRand = 1.0*minHopOutPorts.size()*rand()/RAND_MAX;
                    unsigned int portIdx = int(portRand);
                    minSubsPortNum = minHopOutPorts[portIdx];
                }
#endif
                // so now we need to ahve the port number or error
                if (!minSubsPortNum) {
                    cout << "-E- Cound not find min hop port!" << endl;
                    return(1);
                }

                // track subscriptions only if target is not a switch:
                if (targetIsHCA)
                    portsSubscriptions[minSubsPortNum-1]++;

                // assign the fdb table.
                p_node->setLFTPortForLid(bLid + lmcValue, minSubsPortNum);
            } // all port lids
        } // all lids

        // we want to get some histogram of subsriptions.
        for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
            IBPort *p_port = p_node->getPort(pn);
            if (p_port && p_port->p_remotePort) {
                if (portsSubscriptions[pn-1] == 0) {
                    cout << "-W- Unused port:" << p_port->getName() << endl;
                }
                subscHist[portsSubscriptions[pn-1]]++;
            }
        }
    } // all nodes

#if 0   // as we provide LFT based path count we do not need this
    // print the histogram:
    cout << "----------------------- LINK SUBSCRIPTIONS HISTOGRAM ----------------------" << endl;
    cout << "Distribution of number of LIDs mapped to each switch out port. Note that " << endl;
    cout << "this assumes every LID is routed through every switch which is not correct" << endl;
    cout << "if one ignores the switch to CA paths.\n" << endl;
    cout << "NUM-LIDS COUNT" << endl;
    for (unsigned int b = 0; b < 1024 ; b++)
        if (subscHist[b])
            cout << setw(7) << b << "   " << subscHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;
#endif

    return(0);
}


///////////////////////////////////////////////////////////////////////////////
// Given a list of root nodes mark them with a zero rank
// Then BFS and rank min
// note we use the provided map of IBNode* to int for storing the rank
int
SubnRankFabricNodesByRootNodes(IBFabric *p_fabric,
        list_pnode rootNodes,
        map_pnode_int &nodesRank)
{
    list_pnode curNodes, nextNodes;
    curNodes = rootNodes;
    int rank = 0;

    // rank by zero the starting nodes:
    for (list_pnode::iterator nI = rootNodes.begin();
            nI != rootNodes.end();
            nI++) {
        IBNode *p_node = (*nI);
        nodesRank[p_node] = 0;
        p_node->rank = 0;
    }

    // ok so now we BFS
    while (curNodes.size()) {
        nextNodes.clear();
        rank++;

        // go over cur step nodes
        for (list_pnode::iterator lI = curNodes.begin();
                lI != curNodes.end(); lI++) {
            IBNode *p_node = *lI;
            // go over all ports to find unvisited remote nodes
            for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                IBPort *p_port = p_node->getPort(pn);
                if (! p_port)
                    continue;
                // might have remote port
                if (p_port->p_remotePort) {
                    // was it visited?
                    IBNode *p_remNode = p_port->p_remotePort->p_node;
                    if (nodesRank.find(p_remNode) == nodesRank.end()) {
                        // add it
                        nextNodes.push_back(p_remNode);
                        // mark it:
                        nodesRank[p_remNode] = rank;
                        p_remNode->rank = rank;
                    }
                }
            }
        }
        curNodes = nextNodes;
    }

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        cout << "-I- Max nodes rank=" << rank  << endl;
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Given a regular expression for nodes mark them with a zero rank
// Then BFS and rank min
// note we use the provided map of IBNode* to int for storing the rank
int
SubnRankFabricNodesByRegexp(IBFabric *p_fabric,
        const char * nodeNameRex,
        map_pnode_int &nodesRank)
{
    regExp nodeRex(nodeNameRex);
    rexMatch *p_rexRes;
    list_pnode rootNodes;

    // go over all nodes of the fabric;
    for (map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end(); nI++) {
        // match rex ?
        p_rexRes = nodeRex.apply((*nI).first.c_str());
        if (p_rexRes) {
            cout  << "-I- Starting UpDown Routing from node:"
                    << (*nI).second->name << endl;
            rootNodes.push_back((*nI).second);
            delete p_rexRes;
        }
    }
    return SubnRankFabricNodesByRootNodes(p_fabric, rootNodes, nodesRank);
}


///////////////////////////////////////////////////////////////////////////////
// Clasify the routes for the same port to port
// Given two list of nodes provide the number of shared
// Systems and Shared nodes:
int
SubnFindPathCommonality(list_pnode *p_path1, list_pnode *p_path2,
        int *commonSystems, int *commonNodes)
{
    map_pnode_int nodesIntersection;
    map_psystem_int systemIntersection;
    IBSystem *p_system;
    IBNode *p_node;

    // we got it now:
    *commonNodes = 0;
    *commonSystems = 0;

    // Go over all nodes in first path add them to the map:
    for (list_pnode::const_iterator lI = p_path1->begin();
            lI != p_path1->end(); lI++) {
        p_node = *lI;
        nodesIntersection[p_node] = 1;
        p_system = p_node->p_system;
        systemIntersection[p_system] = 1;
    }

    // We only need to count the number of pre-existing nodes.
    // we do it once for each nodes.
    for (list_pnode::const_iterator lI = p_path2->begin();
            lI != p_path2->end(); lI++) {
        // we only count once for each node.
        p_node = *lI;
        map_pnode_int::iterator mI = nodesIntersection.find(p_node);
        if ( (mI != nodesIntersection.end()) &&
                ((*mI).second == 1) ) {
            (*commonNodes)++;
            // we increase it for next time
            (*mI).second++;
        }
        p_system = p_node->p_system;
        map_psystem_int::iterator sI = systemIntersection.find(p_system);
        if ( (sI != systemIntersection.end()) &&
                ((*sI).second == 1) ) {
            (*commonSystems)++;
            // we increase it for next time
            (*sI).second++;
        }
    }

    //cout << "P1:" << p_path1->size()
    //  << " P2:"<< p_path2->size()
    //  << " Intersaction:" << *commonNodes << "/" << *commonSystems<<endl;
    return(0);
}


///////////////////////////////////////////////////////////////////////////////
// Verify point to point connectivity
int
SubnMgtVerifyAllCaToCaRoutes(IBFabric *p_fabric)
{
    unsigned int lidStep = 1 << p_fabric->lmc;
    int anyError = 0, paths = 0;
    vec_int maxHopsHist(50,0);
    // we want to track common Nodes and Systems
    // based on the original path length
    int CommonNodes[50][16];
    int CommonSystems[50][16];
    unsigned int maxDepth = 0;
    int maxLinkSubscriptions = 0;
    int maxDlidPerOutPort = 0;

    // to track the actual paths going through each switch port
    // we need to have a map from switch node to a vector of count
    // per port.
    map_pnode_vec_int switchPathsPerOutPort;

    // track the number of dlids actually routed through each switch port.
    // to avoid memory scalability we do the path scanning with dest port
    // in the external loop. So we only need to look on the aggregated
    // vectore per port at the end of all sources and sum up to teh final results
    map_pnode_vec_int switchDLidsPerOutPort;

    cout << "-I- Verifying all CA to CA paths ... " << endl;
    // initialize the histograms:
    memset(CommonNodes, 0 , 50*16 * sizeof(int));
    memset(CommonSystems, 0 , 50*16 * sizeof(int));

    unsigned int hops, maxHops = 0;
    list_pnode path1, path2, *p_path;

    // go over all ports in the fabric
    for (unsigned int i = p_fabric->minLid; i <= p_fabric->maxLid; i += lidStep ) {
        IBPort *p_dstPort = p_fabric->PortByLid[i];

        if (!p_dstPort || (p_dstPort->p_node->type == IB_SW_NODE)) continue;

        // tracks if a path to current dlid was found per switch out port
        map_pnode_vec_int switchAnyPathsPerOutPort;

        unsigned int dLid = p_dstPort->base_lid;
        // go over all the rest of the ports:
        for (unsigned int j = p_fabric->minLid; j <= p_fabric->maxLid; j += lidStep ) {
            IBPort *p_srcPort = p_fabric->PortByLid[j];

            // Avoid tracing to itself
            if (i == j)
                continue;
            if (! p_srcPort)
                continue;
            if (p_srcPort->p_node->type == IB_SW_NODE)
                continue;

            unsigned int sLid = p_srcPort->base_lid;

            // go over all LMC combinations:
            for (unsigned int l = 0; l < lidStep; l++) {
                paths++;

                // we track the path nodes in lists but need to know which one
                // to use:
                if (l == 0) {
                    p_path = &path1;
                    path1.clear();
                } else {
                    p_path = &path2;
                    path2.clear();
                }

                // now go and verify the path:
                if (TraceRouteByLFT(p_fabric, sLid + l, dLid + l, &hops, p_path)) {
                    cout << "-E- Fail to find a path from:"
                            << p_srcPort->p_node->name << "/" << p_srcPort->num
                            << " to:" << p_dstPort->p_node->name << "/" << p_dstPort->num
                            << endl;
                    anyError++;
                } else {
                    // track the hops histogram
                    maxHopsHist[hops]++;
                    if (hops > maxHops) maxHops = hops;

                    // populate the number of the out ports along the path
                    // but ignore the last element
                    for (list_pnode::const_iterator lI = p_path->begin();
                            lI != p_path->end(); lI++) {
                        // init the ports count vector if needed
                        IBNode *pNode = (*lI);
                        if (switchPathsPerOutPort.find(pNode) == switchPathsPerOutPort.end()) {
                            vec_int tmp(pNode->numPorts + 1,0);
                            switchPathsPerOutPort[pNode] = tmp;
                        }

                        // init the marking of any path through the port if needed:
                        if (switchAnyPathsPerOutPort.find(pNode) == switchAnyPathsPerOutPort.end()) {
                            vec_int tmp(pNode->numPorts + 1,0);
                            switchAnyPathsPerOutPort[pNode] = tmp;
                        }

                        list_pnode::const_iterator nlI = lI;
                        nlI++;
                        if (nlI != p_path->end()) {
                            unsigned int outPort = pNode->getLFTPortForLid(dLid + l);
                            switchPathsPerOutPort[pNode][outPort]++;
                            if (maxLinkSubscriptions < switchPathsPerOutPort[pNode][outPort])
                                maxLinkSubscriptions = switchPathsPerOutPort[pNode][outPort];

                            switchAnyPathsPerOutPort[pNode][outPort]++;
                        }
                    }

                    // Analyze the path against the previous path:
                    if (l != 0) {
                        static int commonSystems, commonNodes;

                        // we only care about paths longer then 2 hops since the two switches
                        //  must be identical
                        if (path1.size()) {
                            SubnFindPathCommonality(&path1, &path2, &commonSystems, &commonNodes);

                            // track the max depth
                            if (path1.size() > maxDepth) maxDepth = path1.size();

                            if (maxDepth > 15) {
                                cout << "-E- Found a path length > 15. Need to recompile with larger Histogram size!" << endl;
                                exit(1);
                            }

                            if (!path1.size()) {
                                cout << "-W- Zero size path1 ???" << endl;
                                continue;
                            }

                            // store the statistics:
                            CommonSystems[commonSystems][0]++;
                            CommonSystems[commonSystems][path1.size()]++;
                            CommonNodes[commonNodes][0]++;
                            CommonNodes[commonNodes][path1.size()]++;

                            if (commonSystems > 5) {
                                cout << "---- MORE THEN 5 COMMON SYSTEMS PATH ----- " << endl;
                                cout << "From:"
                                        << p_srcPort->p_node->name << "/" << p_srcPort->num
                                        << " to:" << p_dstPort->p_node->name << "/" << p_dstPort->num
                                        << endl;
                                cout << "Path 1" << endl;
                                for (list_pnode::iterator lI = path1.begin();
                                        lI!= path1.end();lI++)
                                    cout << "." << (*lI)->name.c_str() << endl;

                                cout << "Path 2" << endl;
                                for (list_pnode::iterator lI = path2.begin();
                                        lI != path2.end();
                                        lI++ )
                                    cout << "." << (*lI)->name.c_str() << endl;
                            }
                        }
                        // cleanup :
                        path2.clear();
                    }

                } // fail to trace route
            }
        } // all src lids

        // cleanup the list of nodes
        path1.clear();

        // add to dlid per port vector:
        for (map_pnode_vec_int::iterator nI = switchAnyPathsPerOutPort.begin();
                nI !=  switchAnyPathsPerOutPort.end();
                nI++) {
            IBNode *pNode = (*nI).first;
            for (unsigned int pn = 1; pn <= pNode->numPorts; pn++) {
                if (switchAnyPathsPerOutPort[pNode][pn]) {
                    // init if required:
                    if (switchDLidsPerOutPort.find(pNode) == switchDLidsPerOutPort.end()) {
                        vec_int tmp(pNode->numPorts + 1,0);
                        switchDLidsPerOutPort[pNode] = tmp;
                    }

                    switchDLidsPerOutPort[pNode][pn]++;
                    if (switchDLidsPerOutPort[pNode][pn] > maxDlidPerOutPort)
                        maxDlidPerOutPort = switchDLidsPerOutPort[pNode][pn];
                }
            }
        }

    } // all dlids

    cout << "---------------------- CA to CA : LFT ROUTE HOP HISTOGRAM -----------------" << endl;
    cout << "The number of CA pairs that are in each number of hops distance." << endl;
    cout << "This data is based on the result of the routing algorithm.\n" << endl;
    cout << "HOPS NUM-CA-CA-PAIRS" << endl;
    for (int b = 0; b <= (int)maxHops ; b++)
        if (maxHopsHist[b])
            cout << setw(3) << b+1 << "   " << maxHopsHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;

    if (p_fabric->lmc > 0) {
        cout << "------------------ LMC BASED ROTING :COMMON NODES HISTOGRAM -----------------" << endl;
        cout << "Describes the distribution of the number of common nodes between the " << endl;
        cout << "different LMC paths of all the CA to CA paths.\n" << endl;
        cout << "COMMON-NODES NUM-CA-CA-PAIRS" << endl;
        cout << "PATH DPT|";
        for (unsigned int d = 1; d <= maxDepth; d++)
            cout <<  setw(6) << d << "|";
        cout << endl;

        for (unsigned int b = 0; b <= maxHops ; b++)
            if (CommonNodes[b][0] != 0) {
                cout << "COMM="<< setw(3) << b << "|";
                for (unsigned int d = 1; d <= maxDepth; d++)
                    cout << setw(6) << CommonNodes[b][d] << "|";
                cout << endl;
            }
        cout << "---------------------------------------------------------------------------\n" << endl;

        cout << "---------------- LMC BASED ROTING :COMMON SYSTEMS HISTOGRAM ---------------" << endl;
        cout << "The distribution of the number of common systems between the " << endl;
        cout << "different LMC paths of all the CA to CA paths.\n" << endl;
        cout << "COMMON-SYSTEM NUM-CA-CA-PAIRS" << endl;
        cout << "PATH DPT|";
        for (unsigned int d = 1; d <= maxDepth; d++)
            cout <<  setw(6) << d << "|";
        cout << endl;
        for (unsigned int b = 0; b <= maxHops ; b++)
            if (CommonSystems[b][0] != 0) {
                cout << "COMM=" << setw(3) << b << "|";
                for (unsigned int d = 1; d <= maxDepth; d++)
                    cout << setw(6) << CommonSystems[b][d] << "|";
                cout << endl;
            }
        cout << "---------------------------------------------------------------------------\n" << endl;
    }

#if DO_CA_TO_CA_NUM_PATHS_HIST
    // report the link over subscription histogram and dump out the
    // num paths per switch out port
    ofstream linkUsage("/tmp/ibdmchk.sw_out_port_num_paths");
    linkUsage << "# NUM-PATHS PORT-NAME " << endl;
    vec_int linkSubscriptionHist(maxLinkSubscriptions + 1,0);
    for (map_pnode_vec_int::iterator nI = switchPathsPerOutPort.begin();
            nI !=  switchPathsPerOutPort.end();
            nI++) {
        IBNode *p_node = (*nI).first;
        for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
            IBPort *p_port = p_node->getPort(pn);
            if (p_port && p_port->p_remotePort &&
                    (p_port->p_remotePort->p_node->type == IB_SW_NODE)) {
                linkUsage << setw(6) << ((*nI).second)[pn]  << " " << p_port->getName() << endl;
                linkSubscriptionHist[((*nI).second)[pn]]++;
            }
        }
    }
    linkUsage.close();

    cout << "---------- LFT CA to CA : SWITCH OUT PORT - NUM PATHS HISTOGRAM -----------" << endl;
    cout << "Number of actual paths going through each switch out port considering" << endl;
    cout << "all the CA to CA paths. Ports driving CAs are ignored (as they must" << endl;
    cout << "have = Nca - 1). If the fabric is routed correctly the histogram" << endl;
    cout << "should be narrow for all ports on same level of the tree." << endl;
    cout << "A detailed report is provided in /tmp/ibdmchk.sw_out_port_num_paths.\n" << endl;
    cout << "NUM-PATHS NUM-SWITCH-PORTS" << endl;
    for (int b = 0; b <= maxLinkSubscriptions; b++)
        if (linkSubscriptionHist[b])
            cout << setw(8) << b << "   " << linkSubscriptionHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;
#endif

    // now do the DLID per out port:
    ofstream portDlidsUsage("/tmp/ibdmchk.sw_out_port_num_dlids");
    portDlidsUsage << "# NUM-DLIDS PORT-NAME " << endl;
    vec_int dlidsSubscriptionHist(maxDlidPerOutPort + 1,0);
    for (map_pnode_vec_int::iterator nI = switchDLidsPerOutPort.begin();
            nI != switchDLidsPerOutPort.end();
            nI++) {
        IBNode *pNode = (*nI).first;
        for (unsigned int pn = 1; pn <= pNode->numPorts; pn++) {
            IBPort *p_port = pNode->getPort(pn);
            if (p_port && p_port->p_remotePort &&
                    (p_port->p_remotePort->p_node->type == IB_SW_NODE)) {
                portDlidsUsage << setw(6) << ((*nI).second)[pn]  << " " << p_port->getName() << endl;
                dlidsSubscriptionHist[((*nI).second)[pn]]++;
            }
        }
    }
    portDlidsUsage.close();

    cout << "---------- LFT CA to CA : SWITCH OUT PORT - NUM DLIDS HISTOGRAM -----------" << endl;
    cout << "Number of actual Destination LIDs going through each switch out port considering" << endl;
    cout << "all the CA to CA paths. Ports driving CAs are ignored (as they must" << endl;
    cout << "have = Nca - 1). If the fabric is routed correctly the histogram" << endl;
    cout << "should be narrow for all ports on same level of the tree." << endl;
    cout << "A detailed report is provided in /tmp/ibdmchk.sw_out_port_num_dlids.\n" << endl;
    cout << "NUM-DLIDS NUM-SWITCH-PORTS" << endl;
    for (int b = 0; b <= maxDlidPerOutPort; b++)
        if (dlidsSubscriptionHist[b])
            cout << setw(8) << b << "   " << dlidsSubscriptionHist[b] << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;

    if (anyError)
        cout << "-E- Found " << anyError << " missing paths"
               << " out of:" << paths << " paths" << endl;
    else
        cout << "-I- Scanned:" << paths << " CA to CA paths " << endl;

    cout << "---------------------------------------------------------------------------\n" << endl;
    return anyError;
}


///////////////////////////////////////////////////////////////////////////////
int
SubnMgtVerifyAllRoutes(IBFabric *p_fabric)
{
    unsigned int lidStep = 1 << p_fabric->lmc;
    int anyError = 0, paths = 0;
    unsigned int maxDepth = 0;

    cout << "-I- Verifying all paths ... " << endl;
    unsigned int hops, maxHops = 0;
    list_pnode path;

    // go over all ports in the fabric
    for (unsigned int i = p_fabric->minLid; i <= p_fabric->maxLid; i += lidStep ) {
        IBPort *p_srcPort = p_fabric->PortByLid[i];

        if (!p_srcPort)
            continue;

        unsigned int sLid = p_srcPort->base_lid;
        // go over all the rest of the ports:
        for (unsigned int j = p_fabric->minLid; j <= p_fabric->maxLid; j += lidStep ) {
            IBPort *p_dstPort = p_fabric->PortByLid[j];

            // Avoid tracing to itself
            if (i == j)
                continue;
            if (! p_dstPort)
                continue;

            unsigned int dLid = p_dstPort->base_lid;

            // go over all LMC combinations:
            for (unsigned int l = 0; l < lidStep; l++) {
                paths++;

                // now go and verify the path:
                if (TraceRouteByLFT(p_fabric, sLid + l, dLid + l, &hops, &path)) {
                    cout << "-E- Fail to find a path from:"
                            << p_srcPort->p_node->name << "/" << p_srcPort->num
                            << " to:" << p_dstPort->p_node->name << "/" << p_dstPort->num
                            << endl;
                    anyError++;
                } else {
                    if (hops > maxHops) maxHops = hops;
                }
                path.clear();
            }
        }
    }

    if (anyError)
        cout << "-E- Found " << anyError << " missing paths"
               << " out of:" << paths << " paths" << endl;
    else
        cout << "-I- Scanned:" << paths << " paths " << endl;

    cout << "---------------------------------------------------------------------------\n" << endl;
    return anyError;
}


///////////////////////////////////////////////////////////////////////////////
// Analyze the fabric to see if it we can recognize the root nodes
// Return the list of root nodes found.
// We limit our auto root recognition too symmetrical fat trees only.
// in this case every switch should either connect to level N-1 or N+1.
// We use this and check for it during the BFS.
typedef list <IBNode *> list_pnode;

list_pnode
SubnMgtFindTreeRootNodes(IBFabric *p_fabric)
{
    list_pnode nextNodes;
    list_pnode curNodes;
    list_pnode rootNodes;
    list_pnode emptyRes;
    IBNode *p_node;
    int rank = 0;
    map_pnode_int nodeRankMap;

    cout << "-I- Automatically recognizing the tree root nodes ..."
            <<  endl;

    // find all non switch nodes and add them to the current step nodes
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_node = (*nI).second;
        if (p_node->type == IB_SW_NODE)
            continue;

        curNodes.push_back(p_node);
    }

    // BFS:
    while (! curNodes.empty()) {
        rank++;
        nextNodes.clear();

        // the last group should be our roots nodes.
        rootNodes = curNodes;

        if (0) {
            cout << "-V- Level:" << rank << " nodes:" << endl;
            int nInLine = 1;
            for (list_pnode::iterator nI = curNodes.begin();
                    nI != curNodes.end();
                    nI++) {
                if (nInLine == 16) {
                    nInLine = 0;
                    cout << endl;
                }
                nInLine++;
                cout << (*nI)->name << " ";
            }
        }

        // go over all this step ports
        while (! curNodes.empty()) {
            p_node = curNodes.front();
            curNodes.pop_front();

            // go over all ports of the node
            for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                IBPort *p_port = p_node->getPort(pn);

                // if there is a connection:
                if (p_port && p_port->p_remotePort) {
                    IBNode *p_remNode = p_port->p_remotePort->p_node;

                    // we ignore non SW nodes:
                    if (p_remNode->type != IB_SW_NODE)
                        continue;

                    // if already marked
                    map_pnode_int::iterator nI = nodeRankMap.find(p_remNode);
                    if (nI != nodeRankMap.end()) {
                        int remNodeRank = (*nI).second;

                        // as we use the rank for signing visited status
                        // we might have the remote node be marked
                        // either with rank - 1 or rank + 1
                        if ((remNodeRank != rank + 1) && (remNodeRank != rank - 1)) {
                            cout << "-E- Given topology is not a pure levelized tree:" << endl;
                            cout << "    Node:" << p_remNode->name << " rank:" << remNodeRank
                                    << " accessed from node:" << p_node->name
                                    << " rank:" << rank << endl;
                            return emptyRes;
                        }
                    } else {
                        // first we mark the node as rank + 1
                        nodeRankMap[p_remNode] = rank + 1;

                        // now we push it to the next level list:
                        nextNodes.push_back(p_remNode);
                    }
                }
            }
        }
        curNodes = nextNodes;
    }
    return rootNodes;
}


///////////////////////////////////////////////////////////////////////////////
// This routine is based on the min hop tables and
// the fact the statistics of the min hops changes for the
// root nodes:
typedef vector< int > vec_int;

list_pnode
SubnMgtFindRootNodesByMinHop(IBFabric *p_fabric)
{
    list_pnode rootNodes;
    unsigned int lidStep = 1 << p_fabric->lmc;
    int minHop;
    int numCas = 0;

    // go over all switch nodes and print the statistics:
    cout << "-I- Automatically recognizing the tree root nodes ..." <<  endl;

    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        IBNode *p_node = (*nI).second;
        if (p_node->type != IB_SW_NODE) numCas++;
    }

    // find all non switch nodes and add them to the current step nodes
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        IBNode *p_node = (*nI).second;
        if (p_node->type != IB_SW_NODE)
            continue;

        // the min hop table should exist on the node:
        unsigned int maxHops = 0;

        // go over all nodes that are CA and calc the histogram of min hops to cas
        vec_int swToCaMinHopsHist(50,0);

        // go over all ports in the fabric
        for (unsigned int i = p_fabric->minLid;
                i <= p_fabric->maxLid; i += lidStep ) {
            IBPort *p_port = p_fabric->PortByLid[i];

            if (!p_port || (p_port->p_node->type == IB_SW_NODE))
                continue;

            unsigned int bLid = p_port->base_lid;

            // get the min hops to this port:
            minHop = p_node->getHops(NULL, bLid);

            swToCaMinHopsHist[minHop]++;

            // track the max:
            if (minHop > maxHops) maxHops = minHop;
        } // all lids

        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
            // print out the hist for now:
            cout << " CA MIN HOP HISTOGRAM:" <<  p_node->name;
            for (unsigned int b = 0; b <= maxHops ; b++)
                cout << " " << setw(4) << swToCaMinHopsHist[b] ;
            cout << endl;
        }

        // we recognize spines by requiring one bar to be above 90% of the
        // number of CAs
        int numHopBarsOverThd1 = 0;
        int numHopBarsOverThd2 = 0;
        float thd1 = numCas * 0.9;
        float thd2 = numCas * 0.05;
        for (unsigned int b = 0; b <= maxHops ; b++) {
            if (swToCaMinHopsHist[b] > thd1) numHopBarsOverThd1++;
            if (swToCaMinHopsHist[b] > thd2) numHopBarsOverThd2++;
        }
        if ((numHopBarsOverThd1 == 1) && (numHopBarsOverThd2 == 1))
            rootNodes.push_back(p_node);
    } // all switches
    return rootNodes;
}


///////////////////////////////////////////////////////////////////////////////
// Find any routes that exist in the FDB's from CA to CA and do not adhare to
// the up/down rules. Report any crossing of the path.
int
SubnReportNonUpDownCa2CaPaths(IBFabric *p_fabric, map_pnode_int &nodesRank)
{
    list_pnode path;
    unsigned int lidStep = 1 << p_fabric->lmc;
    int anyError = 0, paths = 0, numBadPaths = 0;
    unsigned int hops;
    string firstChangeMsg;

    cout << "-I- Tracing all CA to CA paths for Credit Loops potential ..." << endl;

    // go over all ports in the fabric
    for (unsigned int i = p_fabric->minLid;
            i <= p_fabric->maxLid; i += lidStep ) {
        IBPort *p_srcPort = p_fabric->PortByLid[i];

        // avoid too many errors...
        if (numBadPaths > 100)
            break;

        if (!p_srcPort || (p_srcPort->p_node->type == IB_SW_NODE))
            continue;

        unsigned int sLid = p_srcPort->base_lid;
        // go over all the rest of the ports:
        for (unsigned int j = p_fabric->minLid;
                j <= p_fabric->maxLid; j += lidStep ) {
            IBPort *p_dstPort = p_fabric->PortByLid[j];

            // avoid too many errors...
            if (numBadPaths > 100)
                break;
            // Avoid tracing to itself
            if (i == j)
                continue;
            if (! p_dstPort)
                continue;
            if (p_dstPort->p_node->type == IB_SW_NODE)
                continue;

            paths++;

            unsigned int dLid = p_dstPort->base_lid;

            // now go and verify the path:
            if (TraceRouteByLFT(p_fabric, sLid, dLid, &hops, &path)) {
                cout << "-E- Fail to find a path from:"
                        << p_srcPort->p_node->name << "/" << p_srcPort->num
                        << " to:" << p_dstPort->p_node->name << "/" << p_dstPort->num
                        << endl;
                anyError++;
            } else {
                int numChanges = 0;
                int prevGoingUp = 1, goingUp;
                int prevRank = 99, rank;
                IBNode *p_prevNode;

                // Go through the path and check for up down transitions.
                for (list_pnode::iterator lI = path.begin(); lI!= path.end(); lI++) {
                    IBNode *p_node = *lI;

                    // lookup the rank of the current node:
                    map_pnode_int::iterator rI = nodesRank.find(p_node);
                    if (rI == nodesRank.end()) {
                        cout << "-E- Somehow we do not have rank for:" << p_node->name
                                << endl;
                        exit(1);
                    }

                    rank = (*rI).second;

                    // we are going up if
                    if (rank < prevRank)
                        goingUp = 1;
                    else
                        goingUp = 0;

                    // now look for dir change.
                    if (prevGoingUp != goingUp) {
                        // we need to report if not first change.
                        if (numChanges) {
                            // header only once:
                            if (numChanges == 1) {
                                cout << "-E- Potential Credit Loop on Path from:"
                                        << p_srcPort->p_node->name << "/" << p_srcPort->num
                                        << " to:" << p_dstPort->p_node->name
                                        << "/" << p_dstPort->num
                                        << endl;
                                cout << firstChangeMsg << endl;
                                numBadPaths++;
                            }
                            // now the point
                            if (goingUp) {
                                cout << "  Going:Up ";
                            } else {
                                cout << "  Going:Down ";
                            }
                            cout << "from:" << p_prevNode->name
                                    << " to:" << p_node->name << endl;
                        } else {
                            // first change so:
                            firstChangeMsg = string("  Going:Down from:") +
                                    p_prevNode->name + string(" to:") + p_node->name;
                        }
                        numChanges++;
                    }
                    prevRank = rank;
                    prevGoingUp = goingUp;
                    p_prevNode = p_node;
                }

                path.clear();
            }
        }
    }

    if (numBadPaths) {
        if (numBadPaths > 100)
            cout << "-W- Stopped checking for loops after 100 errors" << endl;
        cout << "-E- Found:" << numBadPaths
                << " CA to CA paths that can cause credit loops." << endl;
    } else
        cout << "-I- No credit loops found in:" << paths
               << " CA to CA paths" << endl;
    cout << "---------------------------------------------------------------------------\n" << endl;
    return numBadPaths;
}


///////////////////////////////////////////////////////////////////////////////
int
SubnMgtUpDnIsLegelStep(IBNode *curNode, IBNode *nxtNode)
{
    int curDir,nxtDir;
    //cout << "-V- B4 __IsLegelStep check" <<endl;
    curDir = (int) curNode->appData2.val;
    nxtDir = (int) nxtNode->appData2.val;
    //  cout << "-V- In  __IsLegelStep check , curDir="<<curDir<<" nxtDir="<<nxtDir<<"\n" << endl;
    if (curDir == 1 && nxtDir == 0)
        return 1;
    else
        return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Do BFS and set Min Hops for the current port , based on previous defined ranking
int
SubnMgtUpDnBFSFromPort(unsigned int lid,
        IBFabric *p_fabric ,
        map_pnode_int &nodesRank )
{
    list_pnode      CurState , NextState ;
    IBPort          *tmpPort,*self;
    IBNode          *self_node;
    tmpPort = p_fabric->getPortByLid(lid);
    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
        cout << "-V- BFS for lid="<<lid<<", type of port= " << tmpPort->p_node->type << endl;
        tmpPort->p_node->repHopTable();
    }
    if (tmpPort->p_node->type == IB_SW_NODE) {
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- This is a switch ..." << endl;
        self = tmpPort;
        self_node = tmpPort->p_node;

        // Update in MinHop Table port 0 with 1 hop
        //tmpPort = self_node->getPort(0);
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- tmpPort is : " << tmpPort << endl;
        if (self->base_lid == lid)
            self_node->setHops(tmpPort,lid,0);
        else
            self_node->setHops(tmpPort,lid,1);
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            self_node->repHopTable();
    } else {
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
            cout << "-V- This is an HCA ..." << endl;
            cout << "-V- tmpPort :" << tmpPort << " remote Port : " << tmpPort->p_remotePort << endl;
        }
        self = tmpPort->p_remotePort;
        self_node = self->p_node;
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- After assignment of self " << self << " && self_node " << self_node << endl;
        // Check to see non switchable subnet
        if (self_node->type != IB_SW_NODE) {
            cout << "-W- This is a non switch subnet could not perform algorithm" << endl;
            return 1;
        }
        // Assign current lid (HCA) to the remote node
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            self_node->repHopTable();
        // If we BFS through base_lid the same lid we can reach it by 0 hops
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- base lid : " <<self->base_lid << " BFS lid : " << lid << endl;
        self_node->setHops(self,lid,1);
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            self_node->repHopTable();
    }

    // We use the current node appData2.val to store direction
    self->p_node->appData2.val = 0UL;
    // Push into list the Current Item
    CurState.push_back(self->p_node);
    // Iterate over all nodes in CurState list till empty
    //  cout << "-V- b4 while .." << endl;
    while (CurState.size()) {
        NextState.clear();
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Iterating in while : size of CurState is : "
                  << CurState.size() << " size of NextState is : " << NextState.size() <<  endl;
        // go over cur step nodes
        for (list_pnode::iterator lI = CurState.begin();
                lI != CurState.end(); lI++) {
            IBNode *p_node = *lI;

            if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                cout << "-V- Current switch handeled is : " << p_node->name << endl;

            IBPort *p_zero_port = p_node->getPort(1);

            // go over all ports to find unvisited remote nodes
            for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                IBPort *p_port = p_node->getPort(pn);

                // Only if current port is NULL or not connected skip it
                if (! p_port || ! p_port->p_remotePort)
                    continue;

                if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                    cout << "-V- Handling port num : " << pn << " of node :" << p_node->name
                        << " p_type="<<p_node->type<<"\n"<<endl;

                IBPort *p_rem_port = p_node->getPort(pn)->p_remotePort;
                IBNode *p_rem_node = p_rem_port->p_node;

                int minHop=0;
                unsigned int rpn = p_rem_port->num;

                // Only if it is a switch then update its table
                if (p_rem_node->type == IB_SW_NODE) {
                    // First put UP / DOWN label to remote node
                    if (nodesRank[p_node] < nodesRank[p_rem_node])
                        // This is DOWN
                        p_rem_node->appData2.val = 1UL;
                    else
                        // This is UP
                        p_rem_node->appData2.val = 0UL;
                    // cout << "-V- b4 Validity check of UP/DOWN"<<endl;
                    // Check validity of direction
                    if (SubnMgtUpDnIsLegelStep(p_node,p_rem_node)) continue;
                    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                        cout << "-V- Current step is legal ..." << endl;

                    // Update remote port MinHopTable according to Current MinHopTable
                    // Set minHop with the minimum hop count for this lid through current node
                    minHop = p_node->getHops(NULL,lid);

                    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
                        cout << "-V- lid=" <<lid << " p_node="<<p_node<<"\n"<<endl;
                        cout << "-V- MinHopTable of current node at lid=" <<lid
                                << " port=0 is : " << minHop<<endl;
                        //cout << "-V- After minHop :"<<minHop<<"\n" <<endl;
                        cout << "-V- B4 Check of current_node(minHop)=" << minHop
                                << " and p_rem_node(minHop)="
                                << p_rem_node->getHops(p_rem_port, lid) << "\n"<<endl;
                    }

                    // we will use the remote node min hops to know if we
                    // already have visited this node
                    int remNodeHops = p_rem_node->getHops(NULL,lid);

                    // Check min hop count if better insert into NextState
                    // list && update the remote node Min Hop Table
                    if (minHop + 1 <= p_rem_node->getHops(p_rem_port, lid)) {
                        // Update MinHopTable of remote node and add it to NextState
                        p_rem_node->setHops(p_rem_port,lid,minHop + 1);

                        // only if the remote node did not have this hop we
                        // need to visit it next step
                        if (remNodeHops > minHop + 1) NextState.push_back(p_rem_node);

                        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
                            cout << "-V- Updating MinHopTable of node : " << p_rem_node->name
                                    << " for lid :" <<  lid  << " Value is : " << minHop << "\n" << endl;
                            // Lets see the last update for the current node
                            p_rem_node->repHopTable();
                        }
                    }
                }
            }
        }
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Start another iteration for NextState with : "
                  << NextState.size() << " elements" << endl;
        CurState = NextState;
    }

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        cout << "-V- End of Iterations ..." << endl;
    return(0);
}


///////////////////////////////////////////////////////////////////////////////
// Calc Min Hop Table using UP-DOWN algorithm
int
SubnMgtCalcUpDnMinHopTbls(IBFabric *p_fabric,
        map_pnode_int &nodesRank)
{
    IBNode *p_node;
    unsigned lidStep = 1 << p_fabric->lmc;

    // go over all the lids and init their Min Hop Tables
    for (unsigned int i = 1; i <= p_fabric->maxLid; i += lidStep ) {
        IBNode *p_cur_node = p_fabric->getPortByLid(i)->p_node;

        if (p_cur_node->type == IB_SW_NODE) {
            // assign all ports of the current node/lid initial value
            p_cur_node->setHops(NULL,0,IB_HOP_UNASSIGNED);
        }
    }

    // Now do the UP-Down Min Hop propagation
    for (unsigned int i = 1; i <= p_fabric->maxLid; i += lidStep )
        if (SubnMgtUpDnBFSFromPort(i,p_fabric,nodesRank))
            return (1);

    // dump the min hops
    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
                nI != p_fabric->NodeByName.end();
                nI++) {
            IBNode *p_node = (*nI).second;
            if (p_node->type != IB_SW_NODE)
                continue;
            p_node->repHopTable();
        }
    return(0);
}


///////////////////////////////////////////////////////////////////////////////
// do up down given a node name regular expression to find the
// root nodes
int
SubnMgtCalcUpDnMinHopTblsByRootNodesRex(IBFabric *p_fabric,
        const char * rootNodesNameRex)
{
    map_pnode_int nodesRank;

    // rank the fabric
    SubnRankFabricNodesByRegexp(p_fabric, rootNodesNameRex, nodesRank);

    // do the actual route
    SubnMgtCalcUpDnMinHopTbls( p_fabric, nodesRank );
    return (0);
}


///////////////////////////////////////////////////////////////////////////////
struct bfsEntry {
   IBNode *pNode;
   uint8_t inPort;
};

// Check a multicast group :
// 1. All switches holding it and connect to HCAs are connected
// 2. No loops (i.e. a single BFS with no returns).
int
SubnMgtCheckMCGrp(IBFabric *p_fabric,
        uint16_t mlid)
{
    list< IBNode *> groupSwitches;
    list< IBNode *> groupHCAs;
    int anyErr = 0;
    char mlidStr[8];
    sprintf(mlidStr, "0x%04X", mlid);
    IBNode *p_firstHcaConnectedSwitch = NULL;

    // find all switches that are part of this mcgrp.
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        IBNode *p_node = (*nI).second;
        if (p_node->type != IB_SW_NODE)
            continue;

        // see if we have an MFT entry by the given lid:
        list_int portNums = p_node->getMFTPortsForMLid(mlid);

        if (portNums.empty())
            continue;

        groupSwitches.push_back(p_node);

        // find all HCAs connected to the group by following the links that
        // are marked in the MFT that connect to the group.
        for( list_int::iterator lI = portNums.begin();
                lI != portNums.end();
                lI++) {
            IBPort *p_port = p_node->getPort(*lI);

            // we do not count switches and disconnected ports
            if (p_port && p_port->p_remotePort &&
                    (p_port->p_remotePort->p_node->type != IB_SW_NODE)) {
                groupHCAs.push_back(p_port->p_remotePort->p_node);
                if (p_firstHcaConnectedSwitch == NULL)
                    p_firstHcaConnectedSwitch = p_node;
            }
        }
    }

    cout << "-I- Multicast Group:" << mlidStr << " has:" << groupSwitches.size()
                << " switches and:" << groupHCAs.size() << " HCAs" << endl;

    if (! groupSwitches.size())
        return(0);
    if (! groupHCAs.size())
        return(0);

    // Check the connectivity of the multicast group:
    // Start with an arbitrary switch and check all are connected with
    // no loops.

    map< IBNode *, uint8_t, less < IBNode *> > visitedNodeFromPort;
    list< bfsEntry > nodesQueue;
    bfsEntry thisStep, nextStep;

    // since we do not complain about disconnected switches if they are not
    // originally connected to HCAs we track the first HCA connected switch and
    // start with it
    thisStep.pNode = p_firstHcaConnectedSwitch;

    thisStep.inPort = 0; // start from port 0 we can go out any port
    nodesQueue.push_back(thisStep);

    // we do the BFS through the queue
    while (nodesQueue.size()) {
        thisStep = nodesQueue.front();
        nodesQueue.pop_front();

        // we keep track of all visited nodes
        visitedNodeFromPort[thisStep.pNode] = thisStep.inPort;

        // get the list of MC group ports for this mlid of this node
        list_int portNums = thisStep.pNode->getMFTPortsForMLid(mlid);

        // go over all MC output ports of the current node ignoring the input
        for (list_int::iterator pnI = portNums.begin();
                pnI != portNums.end(); pnI++) {
            unsigned int pn = (*pnI);

            // ignore the port we got here through.
            if (pn == thisStep.inPort)
                continue;

            IBPort *pPort = thisStep.pNode->getPort(pn);
            if (! pPort || ! pPort->p_remotePort)
                continue;

            // get the remote node
            IBNode *pRemNode = pPort->p_remotePort->p_node;

            // we ignore remote HCAs
            if (pRemNode->type != IB_SW_NODE)
                continue;

            // if we already visited this node - it is a loop!
            map< IBNode *, uint8_t, less < IBNode *> >::iterator vI =
                    visitedNodeFromPort.find(pRemNode);

            if (vI != visitedNodeFromPort.end()) {
                int prevPort =  (*vI).second ;
                cout << "-E- Found a loop on MLID:" << mlidStr
                        << " got to node:" << pRemNode->name
                        << " through port:" << pPort->p_remotePort->num
                        << " previoulsy visited through port:" << prevPort << endl;
                anyErr++;
                continue;
            }

            // if the remote node does not point back to this one (i.e. the port is bit is not set in the
            // MFT do not go through ...
            list_int remPortNums = pRemNode->getMFTPortsForMLid(mlid);
            if (find(remPortNums.begin(), remPortNums.end(),pPort->p_remotePort->num) == remPortNums.end()) {
                cout << "-W- Found a non symmetric MFT on MLID:" << mlidStr
                        << " got to node:" << pRemNode->name
                        << " through port:" << pPort->p_remotePort->num
                        << " which does not point back to node:"
                        << pPort->p_node->name
                        << " port:" << pPort->num << endl;
                continue;
            }

            // push the node into next steps:
            nextStep.pNode = pRemNode;
            nextStep.inPort = pPort->p_remotePort->num;
            nodesQueue.push_back(nextStep);
        }
    }

    for( list< IBNode *>::iterator lI = groupSwitches.begin();
            lI != groupSwitches.end();
            lI++) {
        IBNode *p_node = *lI;
        map< IBNode *, uint8_t, less < IBNode *> >::iterator vI =
                visitedNodeFromPort.find(p_node);

        if (vI == visitedNodeFromPort.end()) {
            // we care only if there are HCAs connected:
            list_pnode connHcas;
            list_int portNums = p_node->getMFTPortsForMLid(mlid);
            if (portNums.empty())
                continue;

            // find all HCAs connected to the group by following the links that
            // are marked in the MFT that connect to the group.
            for( list_int::iterator lI = portNums.begin();
                    lI != portNums.end();
                    lI++) {
                IBPort *p_port = p_node->getPort(*lI);

                // we do not count switches and disconnected ports
                if (p_port && p_port->p_remotePort &&
                        (p_port->p_remotePort->p_node->type != IB_SW_NODE))
                    connHcas.push_back(p_port->p_remotePort->p_node);
            }

            // so do we care?
            if (connHcas.size()) {
                cout << "-E- Disconnected switch:" << p_node->name << " in group:"
                        << mlidStr << endl;
                for (list_pnode::iterator hlI = connHcas.begin();
                        hlI != connHcas.end();
                        hlI++)
                    cout << "-E- Disconnected HCA:" << (*hlI)->name << endl;
                anyErr++;
            } else {
                cout << "-W- Switch:" << p_node->name << " has MFT entry of group:"
                        << mlidStr << " but is disconnceted from it" << endl;
            }
        }
    }

    for( map< IBNode *, uint8_t, less < IBNode *> >::iterator vI
            = visitedNodeFromPort.begin();
            vI != visitedNodeFromPort.end();
            vI++) {
        list< IBNode *>::iterator lI =
                find(groupSwitches.begin(), groupSwitches.end(), (*vI).first);

        if (lI == groupSwitches.end()) {
            cout << "-E- Extra switch:" << (*vI).first->name << " in group:"
                    << mlidStr << " MC packets flow into it but never leave."
                    << endl;
            anyErr++;
        }
    }

    // TODO: Do credit loop check in the Credit.cpp ...
    // traverse all paths from each HCA and report any that do not follow
    // UP/DN requirements. Also make sure no loops exist.

    return anyErr;
}


///////////////////////////////////////////////////////////////////////////////
// Check all multicast groups :
// 1. all switches holding it are connected
// 2. No loops (i.e. a single BFS with no returns).
int
SubnMgtCheckFabricMCGrps(IBFabric *p_fabric)
{
    int anyErrs = 0;
    cout << "-I- Scanning all multicast groups for loops and connectivity..."
            << endl;

    for (set_uint16::iterator sI = p_fabric->mcGroups.begin();
            sI != p_fabric->mcGroups.end();
            sI++)
        anyErrs += SubnMgtCheckMCGrp(p_fabric, *sI);

    if (anyErrs)
        cout << "-E- " << anyErrs << " multicast group checks failed" << endl;

    cout << "---------------------------------------------------------------------------\n" << endl;
    return anyErrs;
}


///////////////////////////////////////////////////////////////////////////////
struct upDnBfsEntry {
   IBNode *pNode;
   IBNode *pTurnNode;
   uint8_t inPort;
   int     down;
};

// Given a starting point switch for multicast traversal and an mlid
// make sure all traversals are following up/down rules.
// return 1 oif there is a violation
int
SubnReportNonUpDownMulticastGroupFromCaSwitch(IBFabric *p_fabric,
        IBNode *pSwitchNode,
        map_pnode_int &nodesRank,
        uint16_t mlid)
{
    map< IBNode *, uint8_t, less < IBNode *> > visitedNodeFromPort;
    list< upDnBfsEntry > nodesQueue;
    upDnBfsEntry thisStep, nextStep;
    int thisNodeRank, remNodeRank;
    int anyErr = 0;
    char mlidStr[8];
    sprintf(mlidStr, "0x%04X", mlid);

    thisStep.down = 0;
    thisStep.pNode = pSwitchNode;
    thisStep.inPort = 0; // start as we got in from port 0 we can go out
                        //  any port
    thisStep.pTurnNode = NULL;
    nodesQueue.push_back(thisStep);

    // we do the BFS through the queue
    while (nodesQueue.size()) {
        thisStep = nodesQueue.front();
        nodesQueue.pop_front();

        // we keep track of all visited nodes
        visitedNodeFromPort[thisStep.pNode] = thisStep.inPort;

        // get the list of MC group ports for this mlid of this node
        list_int portNums = thisStep.pNode->getMFTPortsForMLid(mlid);

        // lookup the rank of the current node:
        map_pnode_int::iterator rI = nodesRank.find(thisStep.pNode);
        if (rI == nodesRank.end()) {
            cout << "-E- Somehow we do not have rank for:" << thisStep.pNode->name
                    << endl;
            exit(1);
        }
        thisNodeRank = (*rI).second;

        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Visiting node:" << thisStep.pNode->name
                  << " dir:" << (thisStep.down ? "DOWN" : "UP") << endl;

        // go over all MC output ports of the current node ignoring the input
        for (list_int::iterator pnI = portNums.begin();
                pnI != portNums.end(); pnI++) {
            unsigned int pn = (*pnI);

            // ignore the port we got here through.
            if (pn == thisStep.inPort)
                continue;

            IBPort *pPort = thisStep.pNode->getPort(pn);
            if (! pPort || ! pPort->p_remotePort)
                continue;

            // get the remote node
            IBNode *pRemNode = pPort->p_remotePort->p_node;

            // we ignore remote HCAs
            if (pRemNode->type != IB_SW_NODE)
                continue;

            // if we already visited this node - it is a loop!
            map< IBNode *, uint8_t, less < IBNode *> >::iterator vI =
                    visitedNodeFromPort.find(pRemNode);

            if (vI != visitedNodeFromPort.end()) {
                int prevPort =  (*vI).second ;
                cout << "-E- Found a loop on MLID:" << mlidStr
                        << " got to node:" << pRemNode->name
                        << " through port:" << pPort->p_remotePort->num
                        << " previoulsy visited through port:" << prevPort << endl;
                anyErr++;
                continue;
            }

            // lookup remote node rank:
            map_pnode_int::iterator rI = nodesRank.find(pRemNode);
            if (rI == nodesRank.end()) {
                cout << "-E- Somehow we do not have rank for:" << pRemNode->name
                        << endl;
                exit(1);
            }
            remNodeRank = (*rI).second;

            // if we are goin up make sure we did not go down before.
            if (remNodeRank < thisNodeRank) {
                // we go up - was it a down?
                if (thisStep.down) {
                    cout << "-E- Non Up/Down on MLID:" << mlidStr
                            << " turning UP from:" << thisStep.pNode->name
                            << "/P" << pn << "("<< thisNodeRank << ") to node:"
                            << pRemNode->name << "/P" << pPort->p_remotePort->num
                            << "(" << remNodeRank << ") previoulsy turned down:"
                            << thisStep.pTurnNode->name << endl;
                    anyErr++;
                    continue;
                } else {
                    // we turned down here so track it:
                    nextStep.down = 0;
                    nextStep.pTurnNode = NULL;
                }
            } else {
                nextStep.pTurnNode = thisStep.pNode;
                nextStep.down = 1;
            }

            // push the node into next steps:
            nextStep.pNode = pRemNode;
            nextStep.inPort = pPort->p_remotePort->num;
            nodesQueue.push_back(nextStep);
        }
    }
    return anyErr;
}


///////////////////////////////////////////////////////////////////////////////
// Find any routes that exist in the FDB's from CA to CA and do not adhare to
// the up/down rules. Report any crossing of the path.
int
SubnReportNonUpDownMulticastGroupCa2CaPaths(IBFabric *p_fabric,
        map_pnode_int &nodesRank,
        uint16_t mlid)
{
    int anyError = 0, numBadPaths = 0, paths = 0;
    char mlidStr[8];
    sprintf(mlidStr, "0x%04X", mlid);

    cout << "-I- Tracing Multicast Group:" << mlidStr
            << " CA to CA paths for Credit Loops potential ..."
            << endl;

    // find all HCAs in the MGRP
    list< IBNode *> groupSwitchesConnToHCAs;

    // find all switches that are part of this mcgrp and connects to HCAs
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        IBNode *p_node = (*nI).second;
        if (p_node->type != IB_SW_NODE)
            continue;

        // see if we have an MFT entry by the given lid:
        list_int portNums = p_node->getMFTPortsForMLid(mlid);

        if (portNums.empty())
            continue;

        // find all HCAs connected to the group by following the links that
        // are marked in the MFT that connect to the group.
        for( list_int::iterator lI = portNums.begin();
                lI != portNums.end();
                lI++) {
            IBPort *p_port = p_node->getPort(*lI);

            if (p_port && p_port->p_remotePort &&
                    p_port->p_remotePort->p_node->type != IB_SW_NODE) {
                groupSwitchesConnToHCAs.push_back(p_node);
                break;
            }
        }
    }

    cout << "-I- Multicast group:" << mlidStr << " has:"
            << groupSwitchesConnToHCAs.size()
            << " Switches connected to HCAs" << endl;

    // from each HCA traverse BFS through the switches.
    for( list_pnode::iterator lI = groupSwitchesConnToHCAs.begin();
            lI != groupSwitchesConnToHCAs.end();
            lI++) {
        // avoid too many errors reported:
        if (numBadPaths > 100)
            break;

        numBadPaths +=
                SubnReportNonUpDownMulticastGroupFromCaSwitch(
                        p_fabric, (*lI), nodesRank, mlid);
        paths++;
    }

    if (numBadPaths) {
        if (numBadPaths > 100)
            cout << "-W- Stopped checking multicast groups after 100 errors" << endl;
        cout << "-E- Found:" << numBadPaths
                << " Multicast:" << mlidStr
                << " CA to CA paths that can cause credit loops." << endl;
    } else
        cout << "-I- No credit loops found traversing:" << paths
               << " leaf switches for Multicast LID:" << mlidStr << endl;
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Check all multicast groups :
// 1. all switches holding it are connected
// 2. No loops (i.e. a single BFS with no returns).
int
SubnMgtCheckFabricMCGrpsForCreditLoopPotential(IBFabric *p_fabric,
        map_pnode_int &nodesRank)
{
    int anyErrs = 0;
    cout << "-I- Scanning all multicast groups for Credit Loops Potential ..." << endl;

    for (set_uint16::iterator sI = p_fabric->mcGroups.begin();
            sI != p_fabric->mcGroups.end();
            sI++)
        anyErrs +=
                SubnReportNonUpDownMulticastGroupCa2CaPaths(p_fabric, nodesRank, *sI);

    if (anyErrs)
        cout << "-E- " << anyErrs << " multicast groups failed" << endl;

    cout << "---------------------------------------------------------------------------\n" << endl;
    return anyErrs;
}


///////////////////////////////////////////////////////////////////////////////
// Fill in the FDB tables assuming the fabric is a fat tree.
// Also assumes the fabric is already ranked from roots downwards.
// We also assume MinHop tables are defined.
//
// LIMITATION: Currently only LMC=0 is supported
//
// The main idea behind this algorithm is that if we have enough
// root ports we can assign each port one destination LID.
// Then we propagate that selection by allowing routing to that LID
// only through that top root and downwards - unless closes at lower levels
//
// ALGORITHM:
// * Initialize a WORKMAP of all HCA LIDs - also count them
// * Calc total switch ports versus number of HCA lids.
// * If we do not have enough switch ports abort.
// * Loop on all rank=0 switches.
//   * For each port select one of the LIDs that it MinHop to and are
//     still in WORKMAP
//   * Traverse forward to that LID assigning LFT
//
// Traverse Forward to LID:
// * Given switch and target LID
// * Find the out-port to be used for going to that LID
//   (use # paths to select from many?)
// * Set FDB to that LID (actually done by the Backward traversal)
// * Recurse to the direction of the LID (need Min hop for that)
// * Perform Backward traversal through all ports connected to lower
//   level switches in-port = out-port
//
// Traverse Backward to LID:
// * Given current switch and LID, in-port
// * Set FDB to target LID to the "in-port"
// * Recurse through all ports connected to lower level switches
//   not including the in-port

static inline void
markPortUtilization(IBPort *p_port)
{
    p_port->counter1++;
}

// given source and destination nodes find the port with lowest
// utilization (subscriptions) and return its number
static int
getLowestUtilzedPortFromTo(IBNode *p_fromNode, IBNode *p_toNode)
{
    int minUtil;
    int minUtilPortNum = 0;
    IBPort *p_port;

    for (unsigned int pn = 1; pn <= p_fromNode->numPorts; pn++) {
        p_port = p_fromNode->getPort(pn);

        if (! p_port)
            continue;
        if (! p_port->p_remotePort)
            continue;
        if (p_port->p_remotePort->p_node != p_toNode)
            continue;

        // the hops should match the min
        if ((minUtilPortNum == 0) || (p_port->counter1 < minUtil)) {
            minUtilPortNum = pn;
            minUtil = p_port->counter1;
        }
    }
    return( minUtilPortNum );
}

// given a node and a target LID find the port that has min hops
// to that LID and lowest utilization
static int
getLowestUtilzedPortToLid(IBNode *p_node, unsigned int dLid)
{
    IBPort *p_port;
    int minUtil;
    int minUtilPortNum = 0;

    // get the minimal hop count from this node:
    int minHop = p_node->getHops(NULL,dLid);

    for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
        p_port = p_node->getPort(pn);

        if (! p_port)
            continue;
        if (! p_port->p_remotePort)
            continue;

        // the hops should match the min
        if (p_node->getHops(p_port, dLid) == minHop) {
            if ((minUtilPortNum == 0) || (p_port->counter1 < minUtil)) {
                minUtilPortNum = pn;
                minUtil = p_port->counter1;
            }
        }
    }
    return( minUtilPortNum );
}

int
SubnMgtFatTreeBwd(IBNode *p_node, uint16_t dLid, unsigned int outPortNum)
{
    IBPort* p_port;

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        cout << "-V- SubnMgtFatTreeBwd from:" << p_node->name << " dlid:" << dLid
               << " out-port:" << outPortNum << endl;

    // Set FDB to target LID to the "in-port"
    p_node->setLFTPortForLid(dLid, outPortNum);

    // mark this port was utilized
    markPortUtilization(p_node->getPort(outPortNum));

    // get the remote node to avoid decending down through it
    p_port = p_node->getPort(outPortNum);
    IBNode *p_origRemNode = p_port->p_remotePort->p_node;

    // Recurse through all ports connected to lower level switches not
    // including the in-port
    for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
        if (pn == outPortNum)
            continue;

        p_port = p_node->getPort(pn);
        if (!p_port || !p_port->p_remotePort)
            continue;

        IBNode *p_remNode = p_port->p_remotePort->p_node;
        // we might have several ports
        if (p_remNode == p_origRemNode)
            continue;
        if (p_remNode->type != IB_SW_NODE)
            continue;
        // avoid going up or sideways in the tree
        if (p_node->rank >= p_remNode->rank)
            continue;

        // avoid loops by inspecting the FDB value of the remote port
        if (p_remNode->getLFTPortForLid(dLid) != IB_LFT_UNASSIGNED)
            continue;

        // select the best port from the remote node to this one:
        int remPortNum = getLowestUtilzedPortFromTo(p_remNode, p_node);
        SubnMgtFatTreeBwd(p_remNode, dLid, remPortNum);
    }
    return(0);
}


///////////////////////////////////////////////////////////////////////////////
int
SubnMgtFatTreeFwd(IBNode *p_node, uint16_t dLid)
{
    int outPortNum = 0;
    IBPort *p_port;

    // Find the out-port to be used for going to that LID
    // (use # paths to select from many?)
    outPortNum = getLowestUtilzedPortToLid(p_node, dLid);

    if (!outPortNum) {
        cout << "-E- fail to find output port for switch:" << p_node->name
                << " to LID:" << dLid << endl;
        exit(1);
    }

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        cout << "-V- SubnMgtFatTreeFwd from:" << p_node->name << " dlid:" << dLid
        << " through port:" << outPortNum << endl;

    p_port = p_node->getPort(outPortNum);

    // Set FDB to that LID (actually done by the Backward traversal)

    // Recurse to the direction of the LID
    if (p_port->p_remotePort->p_node->type == IB_SW_NODE)
        SubnMgtFatTreeFwd(p_port->p_remotePort->p_node, dLid);

    // Perform Backward traversal through all ports connected to lower
    // level switches in-port = out-port
    SubnMgtFatTreeBwd(p_node, dLid, outPortNum);
   return(0);
}


///////////////////////////////////////////////////////////////////////////////
int
SubnMgtFatTreeRoute(IBFabric *p_fabric)
{
    IBNode *p_node;
    IBPort *p_port;

    cout << "-I- Using Fat Tree Routing" << endl;

    // HACK we currently do not support LMC > 0
    if (p_fabric->lmc > 0) {
        cout << "-E- Fat Tree Router does not support LMC > 0 yet" << endl;
        return(1);
    }

    list<IBNode*> rootNodes;
    set<int, less<int> > unRoutedLids;

    int numHcaPorts = 0;
    int numRootPorts = 0;

    // get all root nodes
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
            nI != p_fabric->NodeByName.end();
            nI++) {
        p_node = (*nI).second;

        // if not a switch just count
        if (p_node->type != IB_SW_NODE) {
            // count all ports that are connected
            for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                p_port = p_node->getPort(pn);
                if (p_port && p_port->p_remotePort) {
                    numHcaPorts++;
                    unRoutedLids.insert(p_port->base_lid);
                }
            }
        } else {
            // we only crae now about root switch ports
            if (p_node->rank == 0) {
                rootNodes.push_back(p_node);
                // count all ports that are connected
                for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
                    p_port = p_node->getPort(pn);
                    if (p_port && p_port->p_remotePort)
                        numRootPorts++;
                }
            }
        }
    }

    // do we have enough root ports?
    if (numRootPorts < numHcaPorts) {
        cout << "-E- Can Route Fat-Tree - not enough root ports:"
                << numRootPorts << " < HCA ports:" << numHcaPorts << endl;
        return(1);
    }

    // Loop on all rank=0 switches.
    for (list<IBNode *>::iterator lI = rootNodes.begin();
            lI != rootNodes.end();
            lI++) {
        set<int, less<int> > switchAllocatedLids;

        p_node = *lI;

        // Foreach port select one of the LIDs that it MinHop to and are
        // still in WORKMAP
        for (unsigned int pn = 1; pn <= p_node->numPorts; pn++) {
            p_port = p_node->getPort(pn);
            if (! p_port || !p_port->p_remotePort)
                continue;

            // since most fat trees are fully connected we can simply try from the
            // head of the unRoutedLids...
            int found = 0;
            for ( set<int, less<int> >::iterator mI = unRoutedLids.begin();
                    !found && (mI != unRoutedLids.end());
                    mI++) {
                // is this dLid in our port min hop?
                uint16_t dLid = *mI;
                // is it in this port min hop?
                if (p_node->getHops(NULL, dLid) == p_node->getHops(p_port, dLid)) {
                    found = 1;
                    // remove from set
                    unRoutedLids.erase(mI);

                    // as it is important we handle lids in order we just collect
                    // them here and later use in order
                    switchAllocatedLids.insert(dLid);

                    // escape the for loop
                    break;
                }
            }
        } // all ports of switch

        // now handle all allocated lids of this switch:
        for ( set<int, less<int> >::iterator alI = switchAllocatedLids.begin();
                alI != switchAllocatedLids.end();
                alI++) {
            unsigned int dLid = *alI;
            if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                cout << "-V- Routing to LID:" << dLid << " through root port:"
                << p_port->getName() << endl;
            // Traverse forward to that LID assigning LFT
            SubnMgtFatTreeFwd(p_node, dLid);
        }
    } // all rank 0 switches

    // double check no more HCA LIDs to route...
    if (unRoutedLids.size()) {
        cout << "-E- " << unRoutedLids.size()
                   << " lids still not routed:" << endl;
        for( set<int, less<int> >::iterator sI = unRoutedLids.begin();
                sI != unRoutedLids.end();
                sI++)
            cout << "   " << *sI << endl;
        return(1);
    }
    return(0);
}


///////////////////////////////////////////////////////////////////////////////
// Recursivly DFS backwards through all switch ports
// (except for the port to the lids) and fill in HCAS reached
static int
dfsBackToCAByLftToDLIDs(IBNode *node,
        list<unsigned int> &dstLids,
        unsigned int dstPortNum,
        set<IBNode *> &visitedNodes,
        map<IBPort *, list<unsigned int> > &HCAPortsLids)
{
    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
        cout << "-V- Visiting " << node->name << " searching for lids:";
        for (list<unsigned int>::const_iterator lI = dstLids.begin();
                lI != dstLids.end(); lI++) cout << *lI << ",";
        cout << endl;
    }

    if (node->type != IB_SW_NODE) {
        IBPort* port = node->getPort(dstPortNum);
        HCAPortsLids[port] = dstLids;
        return(0);
    }

    // first check which dst lid is still valid
    list<unsigned int> subDstLids;
    for (list<unsigned int>::const_iterator lI = dstLids.begin();
            lI != dstLids.end(); lI++) {
        unsigned int lid = *lI;
        if ((lid < node->LFT.size()) && (node->LFT[lid] == dstPortNum)) {
            subDstLids.push_front(lid);
        }
    }

    // no paths to the dlids through the port
    if (subDstLids.size() == 0) {
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Dead end" << endl;
        return(0);
    }

    // mark the node visited now
    visitedNodes.insert(node);

    // DFS through the other ports
    for (unsigned int pn = 1; pn <= node->numPorts; pn++) {
        if (pn != dstPortNum) {
            IBPort *port = node->getPort(pn);
            if (!port || ! port->p_remotePort) continue;
            // set remPort [IBPort_p_remotePort_get $port]
            // if {$remPort == ""} {continue}
            IBNode *p_remNode = port->p_remotePort->p_node;
            if (visitedNodes.find(p_remNode) == visitedNodes.end()) {
                unsigned int remPortNum = port->p_remotePort->num;
                dfsBackToCAByLftToDLIDs(p_remNode, subDstLids, remPortNum,
                        visitedNodes, HCAPortsLids);
            }
        }
    }
    return(0);
}

// return 1 if LID is reachable by LFT from given port
static int
isThereLFTPathToLID(IBPort *p_port, unsigned int lid)
{
    if (p_port->base_lid == lid)
        return(1);

    set<IBNode *> visitedNodes;
    visitedNodes.insert(p_port->p_node);
    while (p_port) {
        // get remote port
        IBPort *p_remPort = p_port->p_remotePort;
        if (!p_remPort)
            return(0);

        if (p_remPort->base_lid == lid)
            return(1);

        // if we already visited we are in a loop
        if (visitedNodes.find(p_remPort->p_node) != visitedNodes.end())
            return(0);

        visitedNodes.insert(p_remPort->p_node);

        // if it is a switch
        if (p_remPort->p_node->type == IB_SW_NODE) {
            unsigned int pn = p_remPort->p_node->getLFTPortForLid(lid);
            p_port = p_remPort->p_node->getPort(pn);
        } else {
            p_port = NULL;
        }
    }
    return(0);
}

// Obtain all the CA to CA port pairs going through the
// given port
int
SubnReportCA2CAPathsThroughSWPort(IBPort *p_port)
{
    // if not a switch port error
    if (p_port->p_node->type != IB_SW_NODE) {
        cout << "-E- Provided port:" << p_port->getName()
	           << " is not a switch port" << endl;
        return(1);
    }

    IBNode *node = p_port->p_node;

    // obtain the DLIDs on the given switch port
    list<unsigned int> lidsThroughPort;
    for (unsigned int i = 0; i < node->LFT.size(); i++) {
        if (node->LFT[i] == p_port->num)
            // TODO: validate there is really a path to that node
            if (isThereLFTPathToLID(p_port, i))
                lidsThroughPort.push_front(i);
            else
                if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                    cout << "-V- LID:" << i
                    << " pointed by LFT but is not reachable from:"
                    << p_port->getName() << endl;
    }

    if (lidsThroughPort.size() == 0) {
        cout << "-W- No paths through port:" << p_port->getName() << endl;
        return(1);
    }

    set<IBNode *> visitedNodes;
    map<IBPort *, list<unsigned int> > HCAPortsLids;

    dfsBackToCAByLftToDLIDs(node, lidsThroughPort, p_port->num,
            visitedNodes, HCAPortsLids);
    IBFabric *fabric = node->p_fabric;
    if (HCAPortsLids.size()) {
        map<IBPort *, list<unsigned int> >::const_iterator pI;
        for (pI = HCAPortsLids.begin(); pI != HCAPortsLids.end(); pI++) {
            IBPort *port = (*pI).first;
            list<unsigned int>::const_iterator lI;
            cout << "From:" << port->getName() << " SLID:" << port->base_lid << endl;
            for (lI = (*pI).second.begin(); lI != (*pI).second.end(); lI++) {
                IBPort *dPort = fabric->getPortByLid(*lI);
                cout << "   To:" << dPort->getName() << " DLID:" << dPort->base_lid
                        << endl;
            }
        }
    }
    return(0);
}

