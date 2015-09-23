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
#include <iomanip>

// Trace a direct route from the given SM node port
int TraceDRPathRoute(IBPort *p_smNodePort, list_int drPathPortNums)
{
    IBPort *p_port = p_smNodePort;
    IBNode *p_node;
    IBPort *p_remPort;
    IBNode *p_remNode;
    IBPort *p_nextPort;
    unsigned int outPortNum;
    unsigned int hop = 0;
    list_int::iterator pI = drPathPortNums.begin();

    // check we haev at least two hops:
    if (drPathPortNums.size() < 2) {
        cout << "-E- We expect at least 2 hops to get out of the SM Node! (got:"
                << drPathPortNums.size() << ")" << endl;
        return(1);
    }

    // we require it to start with zero - marking the internal hop

    if (*pI != 0) {
        cout << "-E- Direct Route Ports list must start with a zero !" << endl;
        return(1);
    }

    // The second port in the we require it to start with zero - marking the internal hop
    pI++;
    if (*pI != p_port->num) {
        cout << "-E- Direct Route Ports second hop must match the SM port number !"
                << " (got:" << *pI << ")" << endl;
        return(1);
    }

    // we are at the port of the SM node so we are at hop 1.
    hop = 1;
    pI++; // points to next hop in the list

    // we traverse to null port or max number of hops reached.
    while (p_port && (hop < drPathPortNums.size())) {
        // get the node
        p_node = p_port->p_node;

        if (p_port->p_sysPort) {
            // is external port:
            cout << "[" << setw(3) <<hop << "] FROM Host:" << p_node->p_system->name.c_str()
                    << " Plug:" << p_port->p_sysPort->name.c_str() << endl;
            cout << "           Node:"  << p_node->name.c_str() << " Port:" << p_port->num << endl;
        } else {
            // internal so provide only internal data:
            cout << "[" << setw(3) << hop << "] FROM Node:"  << p_node->name.c_str() << " Port:"
                    << p_port->num << endl;
        }

        // we calc next step port:
        p_nextPort = NULL;

        // start going to the other side:
        p_remPort = p_port->p_remotePort;
        if (p_remPort != NULL) {
            p_remNode = p_remPort->p_node;

            // The to section:
            if (p_remPort->p_sysPort) {
                // is external port:
                cout << "      TO   Host:" << p_remNode->p_system->name.c_str()
                        << " Plug:" << p_remPort->p_sysPort->name.c_str() << endl;
                cout << "           Node:"  << p_remNode->name.c_str() << " Port:" << p_remPort->num << endl;
            } else {
                // internal so provide only internal data:
                cout << "      TO   Node:"  << p_remNode->name.c_str() << " Port:" << p_remPort->num << endl;
            }

            if (pI != drPathPortNums.end()) {
                // calc the next port
                outPortNum = *pI - 1;
                // we could get stuck on bad port num
                if (outPortNum > p_remNode->numPorts || outPortNum < 0) {
                    cout << "-E- Bad port number:" << outPortNum << " hop:" << hop << endl;
                    return 1;
                }

                // port not connected
                if (p_remNode->Ports[outPortNum] == NULL) {
                    cout << "[" << setw(3) << hop << "] Broken Route: not connected port:" << outPortNum << endl;
                    return 1;
                }

                p_nextPort = p_remNode->Ports[outPortNum];
            }

            pI++;
            hop++;
            }
            p_port = p_nextPort;
    }
    return 0;
}

typedef set< IBNode *, less< IBNode * > > set_p_node;

// Trace the path between the lids based on min hop count only
int
TraceRouteByMinHops(IBFabric *p_fabric,
        unsigned int slid , unsigned int dlid)
{
    IBPort *p_port = p_fabric->getPortByLid(slid), *p_remPort, *p_nextPort;
    IBNode *p_node, *p_remNode;
    unsigned int hop = 0;
    set_p_node visitedNodes;

    // make sure:
    if (! p_port) {
        cout << "-E- Provided source:" << slid
                << " lid is not mapped to a port!" << endl;
        return(1);
    }

    // find the physical port we out from:
    p_port = p_port->p_node->getFirstMinHopPort(dlid);
    if (!p_port) {
        cout << "-E- Fail to obtain minhop port for switch:" << slid << endl;
        return(1);
    }

    cout << "--------------------------- TRACE PATH BY MIN HOPS -------------------------" << endl;
    cout << "-I- Tracing by Min Hops from lid:" << slid
            << " to lid:" << dlid << endl;
    // we traverse to target dlid
    while (p_port) {
            // get the node
            p_node = p_port->p_node;

            if (p_port->p_sysPort) {
                // is external port:
                cout << "[" << setw(3) << hop << "] FROM Host:" << p_node->p_system->name.c_str()
                        << " Plug:" << p_port->p_sysPort->name.c_str() << endl;
                cout << "           Node:"  << p_node->name.c_str() << " Port:" << p_port->num << endl;
            } else {
                // internal so provide only internal data:
                cout << "[" << setw(3) << hop << "] FROM Node:"  << p_node->name.c_str() << " Port:"
                        << p_port->num << endl;
            }

            // we calc next step port:
            p_nextPort = NULL;

            // start going to the other side:
            p_remPort = p_port->p_remotePort;
            if (p_remPort != NULL) {
                p_remNode = p_remPort->p_node;

                // did we already visit this node?
                set_p_node::iterator sI = visitedNodes.find(p_remNode);
                if (sI != visitedNodes.end()) {
                    cout << "-E- Run into loop in min hop path at node:" << p_remNode->name << endl;
                    return 1;
                }
                visitedNodes.insert(p_remNode);

                // The to section:
                if (p_remPort->p_sysPort) {
                    // is external port:
                    cout << "      TO   Host:" << p_remNode->p_system->name.c_str()
                            << " Plug:" << p_remPort->p_sysPort->name.c_str() << endl;
                    cout << "           Node:"  << p_remNode->name.c_str() << " Port:" << p_remPort->num << endl;
                } else {
                    // internal so provide only internal data:
                    cout << "      TO   Node:"  << p_remNode->name.c_str() << " Port:" << p_remPort->num << endl;
                }

                // we need next port only if we are on a switch:
                if (p_remNode->type == IB_SW_NODE) {
                    p_nextPort = p_remNode->getFirstMinHopPort(dlid);
                }
                hop++;
            }
            p_port = p_nextPort;
    }
    cout << "---------------------------------------------------------------------------\n" << endl;
    return 0;
}

// Trace a route from slid to dlid by LFT
int TraceRouteByLFT(IBFabric *p_fabric,
        unsigned int sLid , unsigned int dLid,
        unsigned int *hops,
        list_pnode *p_nodesList)
{
    IBPort *p_port = p_fabric->getPortByLid(sLid);
    IBNode *p_node;
    IBPort *p_remotePort;
    unsigned int lidStep = 1 << p_fabric->lmc;
    int hopCnt = 0;

    // make sure:
    if (! p_port) {
        cout << "-E- Provided source:" << sLid
                << " lid is not mapped to a port!" << endl;
        return(1);
    }

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE) {
        cout << "--------------------------- TRACE PATH BY FDB -----------------------------" << endl;
        cout << "-V- Tracing from lid:" << sLid << " to lid:"
                << dLid << endl;
    }

    if (hops) *hops = 0;

    // if the port is not a switch - go to the next switch:
    if (p_port->p_node->type != IB_SW_NODE) {
        // try the next one:
        if (!p_port->p_remotePort) {
            cout << "-E- Provided starting point is not connected !"
                    << "lid:" << sLid << endl;
            return 1;
        }
        p_node = p_port->p_remotePort->p_node;
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Arrived at Node:" << p_node->name
                  << " Port:" << p_port->p_remotePort->num << endl;
    } else {
        // it is a switch :
        p_node = p_port->p_node;
    }


    // verify we are finally of a switch:
    if (p_node->type != IB_SW_NODE) {
            cout << "-E- Provided starting point is not connected to a switch !"
                    << "lid:" << sLid << endl;
            return 1;
    }

    // traverse:
    int done = 0;
    while (!done) {
        // insert the node pointer to the list
        if (p_nodesList) {
            p_nodesList->push_back(p_node);
        }

        // calc next node:
        int pn = p_node->getLFTPortForLid(dLid);
        if (pn == IB_LFT_UNASSIGNED) {
            cout << "-E- Unassigned LFT for lid:" << dLid << " Dead end at:" << p_node->name << endl;
            return 1;
        }

        // if the port number is 0 we must have reached the target
        if (pn == 0) {
            uint16_t base_lid = 0;
            // get lid of any port of this node
            for (unsigned int portNum = 0;
                    (base_lid == 0) && (portNum <= p_node->numPorts); portNum++) {
                IBPort *p_port = p_node->getPort(portNum);
                if (p_port) base_lid = p_port->base_lid;
            }
            if (base_lid == 0) {
                cout << "-E- Fail to find node:" << p_node->name
                        << " base lid?" << endl;
                return 1;
            }

            if ((base_lid > dLid) || (base_lid + lidStep - 1 < dLid)) {
                cout << "-E- Dead end at port 0 of node:" << p_node->name << endl;
                return 1;
            }
            return 0;
        }

        // get the port on the other side
        p_port = p_node->getPort(pn);
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Going out on port:" << pn << endl;

        if (! (p_port &&
                p_port->p_remotePort &&
                p_port->p_remotePort->p_node)) {
            cout << "-E- Dead end at:" << p_node->name << endl;
            return 1;
        }

        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Arrived at Node:" << p_port->p_remotePort->p_node->name
                  << " Port:" << p_port->p_remotePort->num << endl;

        p_remotePort = p_port->p_remotePort;
        // check if we are done:
        done = ((p_remotePort->base_lid <= dLid) &&
                    (p_remotePort->base_lid+lidStep - 1 >= dLid));

        p_node = p_remotePort->p_node;
        if (hops) (*hops)++;
        if (hopCnt++ > 256) {
            cout << "-E- Aborting after 256 hops - loop in LFT?" << endl;
            return 1;
        }
    }

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        cout << "---------------------------------------------------------------------------\n" << endl;
    return 0;
}

