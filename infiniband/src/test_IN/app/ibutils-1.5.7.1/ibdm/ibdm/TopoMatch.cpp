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

Topology Matching

The file holds a set of utilities to providea "topology matching"
functionality: comparing two fabrics based on some entry point.

The comparison is of a "discovered" fabric to a "spec" fabric:
disc = The fabric de-facto as OpenSM see it.
spec = The fabric as specified in either a topology file or cables file.

Through the matching algorithm:
1. The specification fabric get guids annotations.
2. Missmatches are being formatted into a text report.

After Matching a Merger can be run to create the merged fabric.
This fabric only includes the discovered nodes but uses the
names of the specification fabric and systems from the specification
fabric.

*/

#include "Fabric.h"
#include "Regexp.h"
#include <iomanip>
#include <sstream>


//////////////////////////////////////////////////////////////////////////////
// we use the node appData2.val as a "visited" marker for the
// topology matching algorithm by setting it's value to 1
static inline void
TopoMarkNodeAsMatchAlgoVisited(IBNode *p_node) {
    p_node->appData2.val = 1;
}

static inline int
TopoIsNodeMatchAlgoVisited(IBNode *p_node) {
    return (p_node->appData2.val & 1);
}

// we use the node appData2.val as a "reported" marker for the
// topology matching algorithm by setting it's second bit (2) to 1
static inline void
TopoMarkNodeAsReported(IBNode *p_node) {
    p_node->appData2.val = (p_node->appData2.val | 2);
}

static inline int
TopoIsNodeMarkedAsReported(IBNode *p_node) {
    return (p_node->appData2.val & 2);
}


//////////////////////////////////////////////////////////////////////////////
// Check for two ports to be matching:
// PortNumber must match,
// If RemotePort exists should:
// a. Same port number
// b. Same node guid if already defined
// c. Link width and speed (these are only added to diagnostics as warnings)
// d. Remote nodes have same number of ports...
// RETURN: 1 if match 0 otherwise.
static int
TopoMatchPorts(IBPort *p_sPort,
        IBPort *p_dPort,
        int doDiag,
        stringstream &diag)
{
    if (!p_sPort || !p_dPort) {
        return 0;
    }
    if (p_sPort->num != p_dPort->num) {
        if (doDiag)
            diag << "Port number missmatch found. The port:" << p_sPort->getName()
            << " != discovered:" << p_dPort->num << endl;
        return 0;
    }

    IBPort *p_dRemPort = p_dPort->p_remotePort;
    IBPort *p_sRemPort = p_sPort->p_remotePort;

    if (p_sRemPort && !p_dRemPort) {
        if (doDiag)
            diag << "Missing link from:" << p_sPort->getName()
            << " to:" << p_sRemPort->getName()
            << endl;
        return 0;
    } else if (!p_sRemPort && p_dRemPort) {
        if (doDiag)
            diag << "Extra link from:" << p_sPort->getName()
            << " to:" << p_dRemPort->getName()
            << endl;
        return 0;
    } else if (p_sRemPort) {
        // check for port numbers
        if (p_sRemPort->num != p_dRemPort->num) {
            // on HCA nodes we can try the other port ...
            if (p_dRemPort->p_node->type != IB_SW_NODE) {
                // we need the spec to define other port of the HCA as empty
                if (doDiag)
                    diag << "Probably switched CA ports on cable from:"
                    << p_sPort->getName()
                    << ". Expected port:" << p_sRemPort->num
                    << " but got port:" << p_dRemPort->num << endl;
            } else {
                if (doDiag)
                    diag << "Wrong port number on remote side of cable from:"
                    << p_sPort->getName()
                    << ". Expected port:" << p_sRemPort->num
                    << " but got port:" << p_dRemPort->num << endl;
                return 0;
            }
        }

        // check link width - since it does not affect the result
        // only in diagnostics mode:
        if (doDiag) {
            if (p_sPort->width != p_dPort->width)
                diag << "Wrong link width on:" << p_sPort->getName()
                << ". Expected:" << width2char(p_sPort->width)
                << " got:" << width2char(p_dPort->width) << endl;
            if (p_sPort->speed != p_dPort->speed)
                diag << "Wrong link speed on:" << p_sPort->getName()
                << ". Expected:" << speed2char(p_sPort->speed)
                << " got:" << speed2char(p_dPort->speed) << endl;
        }

        IBNode *p_dRemNode = p_dRemPort->p_node;
        IBNode *p_sRemNode = p_sRemPort->p_node;

        // check to see if the remote nodes are not already cross linked
        if (p_dRemNode->appData1.ptr) {
            if ((IBNode *)p_dRemNode->appData1.ptr != p_sRemNode) {
                IBPort *p_port =
                        ((IBNode *)p_dRemNode->appData1.ptr)->getPort(p_sRemPort->num);
                if (p_port) {
                    diag << "Link from port:" << p_sPort->getName()
                       << " should connect to port:" << p_sRemPort->getName()
                       << " but connects to (previously matched) port:"
                       << p_port->getName() << endl;
                } else {
                    diag << "Link from port:" << p_sPort->getName()
                       << " should connect to port:" << p_sRemPort->getName()
                       << " but connects to a port not supposed to be connected"
                       << " on (previously matched) node:"
                       << ((IBNode *)p_dRemNode->appData1.ptr)->name << endl;
                }
                return 0;
            }
        }

/*
        if (p_sRemNode->appData1.ptr) {
            if ((IBNode *)p_sRemNode->appData1.ptr != p_dRemNode) {
                diag << "Link from port:" << p_sPort->getName()
                << " should connect to node:" << p_sRemNode->name
                << " but connects to previously matched node:"
                << ((IBNode *)p_sRemNode->appData1.ptr)->name << endl;
                return 0;
            }
        }
*/

        // if spec node guid is defined verify same guid:
        if (p_sRemNode->guid_get() &&
                (p_sRemNode->guid_get() != p_dRemNode->guid_get())) {
            if (doDiag)
                diag << "Wrong node on cable from:" << p_sPort->getName()
                << ". Expected connection to node:"
                << guid2str(p_sRemNode->guid_get())
                << " but connects to:"
                << guid2str(p_dRemNode->guid_get()) << endl;
            return 0;
        }

        // same number of ports on nodes
        if (p_sRemNode->numPorts != p_dRemNode->numPorts) {
            if (doDiag)
                diag << "Other side of cable from:" << p_sPort->getName()
                << " difference in port count. Expected:"
                << p_sRemNode->numPorts
                << " but got:"
                << p_dRemNode->numPorts << endl;
            return 0;
        }
    }
    return 1;
}


// Consider node matching by inspecting all their ports.
// All ports but maxMiss should be matched to pass.
// RETURN: 1 if qualified as matching 0 otherwise.
static int
TopoQalifyNodesMatching(IBNode *p_sNode, IBNode *p_dNode)
{
    int maxMissed = 2;          // TODO : Maybe need to restrict by the number of ports?
    int numMissed = 0;
    IBPort *p_sPort;
    IBPort *p_dPort;
    stringstream tmpDiag;

    // we must make sure we did not assign a match to any of the nodes ...
    if ((p_sNode->appData1.ptr || p_dNode->appData1.ptr) &&
            (p_sNode->appData1.ptr != p_dNode->appData1.ptr))
        return 0;

    // we must have same number of ports ...
    if (p_dNode->numPorts != p_sNode->numPorts)
        return 0;

    // Try to match all ports of this node
    for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
        p_dPort = p_dNode->getPort(pn);
        p_sPort = p_sNode->getPort(pn);

        // we might miss some ports:
        if (!p_dPort && !p_sPort) {
            continue;
        } else if (!p_dPort || !p_sPort) {
            if (p_dPort && p_dPort->p_remotePort) {
                tmpDiag << "Port:" << pn << " exist only in discovered model." << endl;
                numMissed++;
            } else if (p_sPort && p_sPort->p_remotePort) {
                tmpDiag << "Port:" << pn << " exist only in specification model."
                        << endl;
                numMissed++;
            }
            continue;
        }

        // we do not care about cases where we miss a
        // discovered link:
        if (! p_dPort->p_remotePort)
            continue;

        // ports match so push remote side into BFS if ok
        if (!TopoMatchPorts(p_sPort, p_dPort, 1, tmpDiag)) {
            numMissed++;
            // if (numMissed > maxMissed) break;
        }
    }

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        if (numMissed)
            if (numMissed <= maxMissed)
                cout << "-V- Qualified Nodes:" << p_sNode->name
                << " to:" << p_dNode->name << " with:" << numMissed
                << " missmatches!" <<endl;
            else
                cout << "-V- Disqualified Nodes:" << p_sNode->name
                << " to:" << p_dNode->name << " due to:" << numMissed
                << " missmatches!\n" << tmpDiag.str() << endl;
    return numMissed <= maxMissed;
}


// Mark nodes as matching
static inline void
TopoMarkMatcedNodes(IBNode *p_node1, IBNode *p_node2, int &matchCounter)
{
    if (p_node1->appData1.ptr || p_node2->appData1.ptr) {
        if (p_node1->appData1.ptr ==  p_node2->appData1.ptr) {
            if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                cout << "-V- Skipping previously Matched nodes:" << p_node1->name
                << " and:" <<  p_node2->name << endl;
        } else {
            if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                cout << "-V- Requested to mark matching nodes:" << p_node1->name
                << " and:" <<  p_node2->name << " previously matched to others" << endl;
        }
    } else {
        // do the cross links on the spec and disc fabrics:
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Matched Node:" << p_node1->name
            << " and:" <<  p_node2->name << endl;
        p_node1->appData1.ptr = p_node2;
        p_node2->appData1.ptr = p_node1;
        matchCounter++;
    }
}


// Perform a BFS on both fabrics and cross point between matching nodes.
static int
TopoDoBFSAndMatch(IBNode *p_sNodeAnchor,  // Starting node on the specification fabrric
        IBNode *p_dNodeAnchor,  // Starting node on the discovered fabrric
        int &numMatchedNodes,
        stringstream &diag)
{
    int dNumNodes, sNumNodes;
    IBNode *p_sNode, *p_dNode;
    IBNode *p_sRemNode, *p_dRemNode;
    IBFabric *p_dFabric, *p_sFabric;
    int status;

    // BFS through the matching ports only.
    // we keep track of the the discovered nodes only as it needs to be matched
    // already to get into this list and thus we have a pointer to the spec node.
    // To mark visited nodes we use the appData2.val
    list < IBNode * > bfsFifo;

    bfsFifo.push_back(p_dNodeAnchor);
    TopoMarkNodeAsMatchAlgoVisited(p_dNodeAnchor);

    // On discovered fabrics where the CA nodes are marked by name we
    // can start traversing from these nodes also - of they match any
    // spec system by name.
    p_dFabric = p_dNodeAnchor->p_fabric;
    p_sFabric = p_sNodeAnchor->p_fabric;
    if (p_dFabric->subnCANames) {
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Matching nodes by name"  << endl;

        // we go through entire discovered fabric trying to find match by name
        for (map_str_pnode::iterator nI = p_dFabric->NodeByName.begin();
                nI != p_dFabric->NodeByName.end(); nI++) {
            IBNode *p_node1 = (*nI).second;

            // so we try to find a node by the same name on the spec fabric.
            map_str_pnode::iterator snI = p_sFabric->NodeByName.find((*nI).first);

            // no match
            if (snI == p_sFabric->NodeByName.end()) {
                if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                    cout << "-V- No match for:" << (*nI).first << endl;
                continue;
            }
            IBNode *p_node2 = (*snI).second;

            // make sure they are not previously match somehome
            if (p_node1->appData1.ptr || p_node2->appData1.ptr)
                continue;

            // do not rush into matching - double check all the nodes ports ...
            int anyMissmatch = 0;
            if (p_node1->numPorts != p_node2->numPorts)
                continue;
                for (unsigned int pn = 1; !anyMissmatch && (pn <= p_node1->numPorts);
                        pn++) {
                    IBPort *p_dPort = p_node1->getPort(pn);
                    IBPort *p_sPort = p_node2->getPort(pn);

                    if (! TopoMatchPorts(p_sPort, p_dPort, 1, diag)) {
                        anyMissmatch++;
                        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                            cout << "-V- Matched node:" <<  (*nI).first
                            << " by name - but some ports are different." << endl;
                    }

                    if (0 && p_dPort->p_remotePort) {
                        IBNode *p_dRemNode = p_dPort->p_remotePort->p_node;
                        IBNode *p_sRemNode = p_sPort->p_remotePort->p_node;
                        if (! TopoQalifyNodesMatching(p_sRemNode, p_dRemNode)) {
                            anyMissmatch++;
                            if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                                cout << "-V- Disqualified start nodes match:"
                                << p_node1->name << " by rem nodes" << endl;
                        }
                    }

                    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
                        cout << "-V- Using name match for nodes:" << p_node1->name << endl;
                    // set the cross pointers and visited flag:
                    TopoMarkNodeAsMatchAlgoVisited(p_node1);

                    TopoMarkMatcedNodes(p_node1, p_node2, numMatchedNodes);
                    // If we do not start from the HCAs we avoid cases where the
                    // wrong leaf is connected and the leaf is deep enough to hide
                    // it
                    // bfsFifo.push_back(p_node1);
                }
        }
    }

    // track any further unmatch:
    int anyUnmatch = 0;

    // BFS loop
    while (!bfsFifo.empty()) {
        p_dNode = bfsFifo.front();
        bfsFifo.pop_front();
        // just makeing sure
        if (! p_dNode->appData1.ptr) {
            cerr << "TopoMatchFabrics: Got to a BFS node with no previous match?"
                    << endl;
            abort();
        }

        p_sNode = (IBNode *)p_dNode->appData1.ptr;
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- Visiting Node:" << p_sNode->name << endl;

        // Try to match all ports of this node
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            IBPort *p_dPort = p_dNode->getPort(pn);
            IBPort *p_sPort = p_sNode->getPort(pn);

            if (!p_sPort && !p_dPort) {
                continue;
            } else if (!p_sPort) {
                diag << "Non existing Spec Port for: "
                        << p_dPort->getName() << endl;
                anyUnmatch++;
                continue;
            } else if (!p_dPort) {
                diag << "Non existing Discovered Port for: "
                        << p_sPort->getName() << endl;
                anyUnmatch++;
                continue;
            }

            // ports match so push remote side into BFS if ok
            if (TopoMatchPorts(p_sPort, p_dPort, 1, diag)) {
                // ports matched but is there a remote side?
                if (p_dPort->p_remotePort) {
                    p_dRemNode = p_dPort->p_remotePort->p_node;
                    p_sRemNode = p_sPort->p_remotePort->p_node;

                    // we need to qualify the match (looking one more step ahead)
                    if (TopoQalifyNodesMatching(p_sRemNode, p_dRemNode)) {
                        // propagate the matching of the nodes:
                        TopoMarkMatcedNodes(p_dRemNode, p_sRemNode, numMatchedNodes);

                        // if not already matched
                        if (!TopoIsNodeMatchAlgoVisited(p_dPort->p_remotePort->p_node)) {
                            bfsFifo.push_back(p_dPort->p_remotePort->p_node);
                            TopoMarkNodeAsMatchAlgoVisited(p_dPort->p_remotePort->p_node);
                        }
                    }
                }
            } else {
                anyUnmatch++;
            }
        }
    }

    status = anyUnmatch;
    sNumNodes = p_sNodeAnchor->p_fabric->NodeByName.size();
    dNumNodes = p_dNodeAnchor->p_fabric->NodeByName.size();
    if (numMatchedNodes != sNumNodes) {
        diag << "Some nodes are missing!" << endl;
        status = 1;
    }

    if (numMatchedNodes != dNumNodes) {
        diag << "Some extra or unmatched nodes were discoeverd." << endl;
        status = 1;
    }
    return 0;
}


// Perform a BFS on both fabrics and cross point between matching nodes.
// Start the algorithm from node on the specification fabric and node on the discovered fabric
static int
TopoBFSAndMatchFromNodes(IBNode *p_sNode, // Starting port on the specification fabrric
        IBNode *p_dNode,
        stringstream &diag)
{
    int numMatchedNodes = 0;

    // Mark local and remote nodes as matching
    // this is an assumption because we need to start from some node
    TopoMarkMatcedNodes(p_sNode, p_dNode, numMatchedNodes);

    return TopoDoBFSAndMatch(p_sNode,
            p_dNode,
            numMatchedNodes,
            diag);
}


// Perform a BFS on both fabrics and cross point between matching nodes.
// Start the algorithm from port on the specification fabric and port on the discovered fabric
static int
TopoBFSAndMatchFromPorts(IBPort *p_sPort, // Starting port on the specification fabrric
        IBPort *p_dPort,
        stringstream &diag)
{
    int numMatchedNodes = 0;
    IBNode *p_sRemNode;
    IBNode *p_dRemNode;

    // Check first that we have a match from the anchor starting ports
    if (! TopoMatchPorts(p_sPort, p_dPort, 1, diag)) {
        diag << "Starting ports do not match. Did you switch the ports?" << endl;
        return 1;
    }

    // Mark the start node as visited:
    TopoMarkNodeAsMatchAlgoVisited(p_dPort->p_node);

    // if we do not have a remote port what do we really match?
    if (!p_dPort->p_remotePort) {
        diag << "No link connected to starting port. Nothing more to match."
                << endl;
        return 1;
    }

    p_dRemNode = p_dPort->p_remotePort->p_node;
    p_sRemNode = p_sPort->p_remotePort->p_node;

    // Mark local and remote nodes as matching
    TopoMarkMatcedNodes(p_dRemNode, p_sRemNode, numMatchedNodes);
    TopoMarkMatcedNodes(p_dPort->p_node, p_sPort->p_node, numMatchedNodes);

    return TopoDoBFSAndMatch(p_sRemNode,
            p_dRemNode,
            numMatchedNodes,
            diag);
}


//////////////////////////////////////////////////////////////////////////////
// Second step in matching - refine missmatched nodes:
// Based on the previous matching - produce a set of all spec nodes
// that are connected to other matched spec nodes.
// Then go over all these nodes and produce a "best fit" match for them:
// The nodes will be connected on the spec model to a set of other nodes.
// Go over all the node ports:
//  If connected to a matched node
//   Check to see what node is connected to the same port of the disc node
//   If it is un-matched - add it to the map of optential nodes adn
//      incr the match value count.
// Now observe the map of potential matching nodes:
// If there is a node that is  mapped by > half the number of ports
// mark it as matching.
typedef map < IBNode *, int , less < IBNode * > >  map_pnode_int;

static int
TopoMatchNodeByAdjacentMatched(IBNode *p_sNode)
{
    int succeedMatch;

    // This map will hold the number of matching ports for each candidate
    // discovered node
    map_pnode_int dNodeNumMatches;

    // we track the total number of connected ports of the spec node
    int numConnPorts = 0;

    // Go over all the node ports:
    for (unsigned int pn = 1; pn <= p_sNode->numPorts; pn++) {
        IBPort *p_sPort = p_sNode->getPort(pn);

        //  If connected to a remote port
        if (! p_sPort || ! p_sPort->p_remotePort)
            continue;

        numConnPorts++;

        IBPort *p_sRemPort = p_sPort->p_remotePort;

        // But the remote spec node must be matched to be considered
        IBNode *p_dRemNode = (IBNode *)p_sRemPort->p_node->appData1.ptr;
        if (! p_dRemNode)
            continue;

        //   Check to see what node is connected to the same port of the disc node
        IBPort *p_dRemPort = p_dRemNode->getPort(p_sRemPort->num);

        // is it connected ?
        if (! p_dRemPort || ! p_dRemPort->p_remotePort)
            continue;

        IBNode *p_dCandidateNode = p_dRemPort->p_remotePort->p_node;

        // It must be un-matched
        if (p_dCandidateNode->appData1.ptr)
            continue;

        // OK so it is a valid candidate
        // - add it to the map of optential nodes adn
        //      incr the match value count.
        map_pnode_int::iterator cnI = dNodeNumMatches.find(p_dCandidateNode);
        if (cnI == dNodeNumMatches.end()) {
            dNodeNumMatches[p_dCandidateNode] = 1;
        } else {
            (*cnI).second++;
        }
    }

    // Now observe the map of potential matching nodes:
    // If there is a node that is  mapped by > half the number of ports
    // mark it as matching.
    for (map_pnode_int::iterator cnI = dNodeNumMatches.begin();
       cnI != dNodeNumMatches.end(); cnI++) {
        if ( (*cnI).second > numConnPorts / 2) {
            TopoMarkMatcedNodes(p_sNode, (*cnI).first, succeedMatch);
        }
    }
    return succeedMatch;
}


// Provide the list of un-matched spec nodes that
// are adjecant to a matched nodes
static list < IBNode *>
TopoGetAllSpecUnMatchedAdjacentToMatched(IBFabric *p_sFabric)
{
    list < IBNode *> adjNodes;
    for (map_str_pnode::iterator nI = p_sFabric->NodeByName.begin();
            nI != p_sFabric->NodeByName.end(); nI++) {
        IBNode *p_node = (*nI).second;

        // if the node is already matcehd skip it:
        if (p_node->appData1.ptr)
            continue;

        // go over all the node ports and check adjacent node.
        int done = 0;
        for (unsigned int pn = 1;
                (pn <= p_node->numPorts) && !done; pn++) {
            IBPort *p_port = p_node->getPort(pn);
            // if there is a remote port
            if (p_port && p_port->p_remotePort) {
                // check if the other node is matched.
                if (p_port->p_remotePort->p_node->appData1.ptr) {
                    // add the node to the list and break the for
                    done = 1;
                    adjNodes.push_back(p_node);
                }
            }
        }
    }
    return adjNodes;
}


// Second Matching step
// Return the number of matched nodes by this step
static int
TopoMatchSpecNodesByAdjacentNode(IBFabric *p_sFabric)
{
    list < IBNode *> unMatchedWithAdjacentMatched;
    int numMatched = 0;

    unMatchedWithAdjacentMatched =
            TopoGetAllSpecUnMatchedAdjacentToMatched(p_sFabric);

    for( list < IBNode *>::iterator nI = unMatchedWithAdjacentMatched.begin();
            nI != unMatchedWithAdjacentMatched.end(); nI++)
        numMatched += TopoMatchNodeByAdjacentMatched(*nI);
}


//////////////////////////////////////////////////////////////////////////////
// Report missmatched nodes by examining entire system and looking for
// all system and then a single board (matching regexp).
// So the report will be optimized:
// If the entire system is missing - report only at this level
// If an entire board of the system missing - report only at this level
// Otherwise report at the specific node only.
static int
TopoReportMismatchedNode(IBNode *p_node,
        int reportMissing, // 0 = reportExtra ...
        stringstream &diag)
{
    int anyMissmatch = 0;
    int MaxConnRep = 12;
    // we assume if a previous check was done we would have
    // never been called since the "reported" mark would have been set
    IBSystem *p_system = p_node->p_system;

    // we always mark the board of the node by examining all but the "UXXX"
    const char *p_lastSlash = rindex(p_node->name.c_str(), '/');
    char nodeBoardName[512];
    int  boardNameLength;
    if (!p_lastSlash) {
        strcpy(nodeBoardName, "NONE");
        boardNameLength = 0;
    } else {
        boardNameLength = p_lastSlash - p_node->name.c_str();
        strncpy(nodeBoardName, p_node->name.c_str(), boardNameLength);
        nodeBoardName[boardNameLength] = '\0';
    }

    int anyOfBoardMatch = 0;
    int anyOfSystemMatch = 0;
    list< IBNode *> boardNodes;

    // visit all nodes of the the specified system.
    for (map_str_pnode::iterator nI = p_system->NodeByName.begin();
            nI != p_system->NodeByName.end(); nI++) {
        IBNode *p_node = (*nI).second;

        // see if it of the same board:
        if (!strncmp(p_node->name.c_str(), nodeBoardName, boardNameLength)) {
            // on same board
            boardNodes.push_back(p_node);
            if (p_node->appData1.ptr) {
                // it was matched to discovered node!
                anyOfBoardMatch++;
                anyOfSystemMatch++;
                // since we only need one to match the board we do not need to
                // continue...
                break;
            }
        } else {
            if (p_node->appData1.ptr) {
                anyOfSystemMatch++;
            }
        }
    }

    // got the markings:
    if (! anyOfSystemMatch) {
        if (reportMissing) {
            diag << "Missing System:" << p_system->name << "("
                    << p_system->type << ")" << endl;

            // we limit the number of reported connections ...
            int numRep = 0;
            for (map_str_psysport::iterator spI = p_system->PortByName.begin();
                    (spI != p_system->PortByName.end()) && (numRep < MaxConnRep); spI++) {
                IBSysPort *p_sysPort = (*spI).second;
                if (p_sysPort->p_remoteSysPort) {
                    numRep++;
                    diag << "   Should be connected by cable from port: "
                            << (*spI).first
                            << "(" << p_sysPort->p_nodePort->p_node->name << "/P"
                            << p_sysPort->p_nodePort->num << ")"
                            << " to:" << p_sysPort->p_remoteSysPort->p_system->name
                            << "/" << p_sysPort->p_remoteSysPort->name
                            << "(" << p_sysPort->p_remoteSysPort->p_nodePort->p_node->name
                            << "/P" << p_sysPort->p_remoteSysPort->p_nodePort->num << ")"
                            << endl;
                }
            }
            if (numRep == MaxConnRep) diag << "   ..." << endl;
            diag << endl;
        } else {
            diag << "Extra System:" << p_system->name << endl;
            int numRep = 0;
            for (unsigned int pn = 1;
                    (pn <= p_node->numPorts) && (numRep < MaxConnRep); pn++) {
                IBPort *p_port = p_node->getPort(pn);
                if (p_port && p_port->p_remotePort) {
                    numRep++;
                    if (p_port->p_remotePort->p_node->appData1.ptr) {
                        IBNode *p_sNode =
                                (IBNode *)p_port->p_remotePort->p_node->appData1.ptr;
                        // NOTE: we can not assume all ports were matched!
                        diag << "   Connected by cable from port: P" << pn
                                << " to:"
                                << p_sNode->name << "/P" << p_port->p_remotePort->num
                                << endl;
                    } else {
                        diag << "   Connected by cable from port: P" << pn
                                << " to:" << p_port->p_remotePort->getName() << endl;
                    }
                }
            }
            if (numRep == MaxConnRep) diag << "   ..." << endl;
            diag << endl;
        }
        anyMissmatch++;

        // also we do not need any more checks on this system
        for (map_str_pnode::iterator nI = p_system->NodeByName.begin();
                nI != p_system->NodeByName.end(); nI++) {
            IBNode *p_node = (*nI).second;
            TopoMarkNodeAsReported(p_node);
        }
    } else if (! anyOfBoardMatch) {
        if (reportMissing) {
            diag << "Missing System Board:" << nodeBoardName << endl;
            diag << endl;
        } else {
            diag << "Extra System Board:" << nodeBoardName << endl;
            diag << endl;
        }
        anyMissmatch++;

        for (list< IBNode *>::iterator nI = boardNodes.begin();
                nI != boardNodes.end(); nI++) {
            TopoMarkNodeAsReported(*nI);
        }
    } else {
        if (reportMissing) {
            diag << "Missing Node:" << p_system->name
                    << "/" << p_node->name << endl;
            int numRep = 0;
            for (unsigned int pn = 1;
                    (pn <= p_node->numPorts) && (numRep < MaxConnRep); pn++) {
                IBPort *p_port = p_node->getPort(pn);
                if (p_port && p_port->p_remotePort) {
                    numRep++;
                    diag << "   Should be connected by cable from port:" << pn
                            << " to:" << p_port->getName() << endl;
                }
            }
            if (numRep == MaxConnRep) diag << "   ..." << endl;
            diag << endl;
        } else {
            diag << "Extra Node:" << p_system->name
                    << "/" << p_node->name << endl;
            int numRep = 0;
            for (unsigned int pn = 1;
                    (pn <= p_node->numPorts) && (numRep < MaxConnRep); pn++) {
                IBPort *p_port = p_node->getPort(pn);
                if (p_port && p_port->p_remotePort) {
                    numRep++;
                    if (p_port->p_remotePort->p_node->appData1.ptr) {
                        IBNode *p_sNode =
                                (IBNode *)p_port->p_remotePort->p_node->appData1.ptr;
                        IBPort *p_tmpPort = p_sNode->getPort(p_port->p_remotePort->num);
                        if (p_tmpPort)
                            diag << "   Connected by cable from port:" << pn
                            << " to:"
                            << p_tmpPort->getName()
                            << endl;
                        else
                            diag << "   Connected by cable from port:" << pn
                            << " to:"
                            << p_sNode->name << "/P" << p_port->p_remotePort->num
                            << endl;
                    } else {
                        diag << "   Connected by cable from port:" << pn
                                << " to:" << p_port->p_remotePort->getName() << endl;
                    }
                }
            }
            if (numRep == MaxConnRep)
                diag << "   ..." << endl;
            diag << endl;
        }
        anyMissmatch++;

        TopoMarkNodeAsReported(p_node);
    }
    return(anyMissmatch);
}


// Given two matching nodes - report un-matching links.
// To avoid duplicated reporting:
// * We only report missmatching links to matched nodes.
// * We only report link to nodes with bigger pointer value
// RETURN number of missmatches
static int
TopoReportMatchedNodesUnMatchingLinks(IBNode *p_sNode,
        IBNode *p_dNode,
        stringstream &diag)
{
    int anyMissmatch = 0;
    // missmatch is when port numbers differ
    // or the remote node is not the same.
    IBPort *p_sPort, *p_dRemPort;
    IBPort *p_dPort, *p_sRemPort;

    for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
        p_dPort = p_dNode->getPort(pn);
        p_sPort = p_sNode->getPort(pn);
        if (p_dPort)
            p_dRemPort = p_dPort->p_remotePort;
        else
            p_dRemPort = NULL;

        if (p_sPort)
            p_sRemPort = p_sPort->p_remotePort;
        else
            p_sRemPort = NULL;

        // so we have four cases
        if (p_dRemPort && p_sRemPort) {
            // can we ignore this report:
            if (!p_dRemPort->p_node->appData1.ptr)
                continue;

            IBNode *p_actRemSNode = (IBNode *)p_dRemPort->p_node->appData1.ptr;
            IBPort *p_actRemSPort = p_actRemSNode->getPort(p_dRemPort->num);

            // make sure the expected remote spec node is what we got.
            if (p_actRemSNode != p_sRemPort->p_node) {
                // clasify internal and external cables
                if (p_sNode->p_system != p_actRemSNode->p_system) {
                    IBPort *p_repPort = p_actRemSNode->getPort(p_dRemPort->num);
                    if (p_repPort)
                        diag << "Wrong cable connecting:" << p_sPort->getName()
                        << " to:" << p_repPort->getName()
                        << " instead of:" << p_sRemPort->getName() << endl;
                    else
                        diag << "Wrong cable connecting:" << p_sPort->getName()
                        << " to:" << p_actRemSNode->name << "/P" << p_dRemPort->num
                        << " instead of:" << p_sRemPort->getName() << endl;
                } else {
                    diag << "Wrong Internal Link connecting:" << p_sPort->getName()
                       << " to:" << p_actRemSNode->name << "/P" << p_dRemPort->num
                       << " instead of:" << p_sRemPort->getName() << endl;
                }
                anyMissmatch++;
            } else {
                // make sure same port remote number
                if (p_dRemPort->num != p_sRemPort->num) {
                    if (p_sNode->p_system != p_actRemSNode->p_system) {
                        IBPort *p_repPort = p_actRemSNode->getPort(p_dRemPort->num);
                        if (p_repPort)
                            diag << "Wrong cable connecting:" << p_sPort->getName()
                            << " to:" << p_repPort->getName()
                            << " instead of:" << p_sRemPort->getName() << endl;
                        else
                            diag << "Wrong cable connecting:" << p_sPort->getName()
                            << " to:" << p_actRemSNode->name << "/P" << p_dRemPort->num
                            << " instead of:" << p_sRemPort->getName() << endl;
                    } else {
                        diag << "Wrong Internal Link connecting:" << p_sPort->getName()
                         << " to:" << p_actRemSNode->name << "/P" << p_dRemPort->num
                         << " instead of:" << p_sRemPort->getName() << endl;
                    }
                    anyMissmatch++;
                }
            }
            // anyway make sure we got the width and speed.
            if (p_sPort->width != p_dPort->width) {
                diag << "Wrong link width on:" << p_sPort->getName()
                     << ". Expected:" << width2char(p_sPort->width)
                     << " got:" << width2char(p_dPort->width) << endl;
                anyMissmatch++;
            }
            if (p_sPort->speed != p_dPort->speed) {
                diag << "Wrong link speed on:" << p_sPort->getName()
                     << ". Expected:" << speed2char(p_sPort->speed)
                     << " got:" << speed2char(p_dPort->speed) << endl;
                anyMissmatch++;
            }
            // done with the case both spec and dicsovered links exists
        } else if (!p_dRemPort && p_sRemPort) {
            // We have a missing cable/link

            // can we ignore this report if not both spec nodes matched
            // or not up going ...
            if (!p_sRemPort->p_node->appData1.ptr)
                continue;

            // we can ignore cases where the other side of the link is
            // connected - since it will be reported later
            IBPort *p_actRemPort =
                    ((IBNode *)p_sRemPort->p_node->appData1.ptr)->getPort(p_sRemPort->num);
            if (p_actRemPort && p_actRemPort->p_remotePort)
                continue;

            // as the spec connections are reciprocal we do not want double the
            // report
            if (p_sPort > p_sRemPort) {
                // clasify the link as cable or internal
                if (p_sPort->p_sysPort || p_sRemPort->p_sysPort) {
                    diag << "Missing cable connecting:" << p_sPort->getName()
                       << " to:" <<  p_sRemPort->getName() << endl;
                } else {
                    diag << "Missing internal Link connecting:" << p_sPort->getName()
                       << " to:" << p_sRemPort->getName() << endl;
                }
            }
            anyMissmatch++;
        } else if (p_dRemPort && !p_sRemPort) {
            // Only a discovered link exists:

            // can we ignore this report if not both discovered nodes matched
            // or not up going ...
            if (!p_dRemPort->p_node->appData1.ptr)
                continue;

            // we can ignore cases where the other side of the link was
            // supposed to be connected - since it will be reported later as wrong
            IBPort *p_actRemPort =
                    ((IBNode *)p_dRemPort->p_node->appData1.ptr)->getPort(p_dRemPort->num);
            if (p_actRemPort && p_actRemPort->p_remotePort)
                continue;

            // we also ignore it by comparing pointer values on discoeverd
            // fabric to avoid double reporting.
            if (p_dRemPort < p_dPort)
                continue;

            string portName;
            if (p_dRemPort->p_node->appData1.ptr) {
                IBPort *p_port =
                        ((IBNode *)p_dRemPort->p_node->appData1.ptr)->getPort(p_dRemPort->num);
                // We can not guarantee all ports match on matching nodes
                if (p_port) {
                    portName = p_port->getName();
                } else {
                    char buff[256];
                    sprintf(buff, "%s/P%d",
                            ((IBNode *)p_dRemPort->p_node->appData1.ptr)->name.c_str(),
                            p_dRemPort->num);

                    portName = string(buff);
                }
            } else {
                portName = p_dRemPort->getName();
            }

            string specPortName;
            if (p_sPort) {
                specPortName = p_sPort->getName();
            } else {
                char buf[16];
                sprintf(buf,"%d",pn);
                specPortName = p_sNode->name + "/" + buf;
            }

            // clasify the link as cable or internal
            if ((p_dNode->p_system != p_dRemPort->p_node->p_system) ||
                    (p_sPort && p_sPort->p_sysPort) ) {
                diag << "Extra cable connecting:" << specPortName
                        << " to:" << portName  << endl;
            } else {
                diag << "Extra internal Link connecting:" << specPortName
                        << " to:" << portName << endl;
            }
            anyMissmatch++;
        }
    }
    if (anyMissmatch)
        diag << endl;
  return(anyMissmatch);
}


// Report Topology Missmatched and Build the Merged Fabric:
static int
TopoReportMissmatches(IBNode *p_sNodeAnchor,            // Starting node on the specification fabrric
        IBNode *p_dNodeAnchor,
        stringstream &diag)
{
    IBNode *p_sNode, *p_dNode;
    int anyMissmatchedSpecNodes = 0;
    int anyMissmatchedDiscNodes = 0;
    int anyMissmatchedLinks = 0;

    // since we do not want to report errors created by previous node failure
    // to discover or match we will BFS from the start ports.

    // NOTE: we only put matched nodes in the fifo.
    list < IBNode * > bfsFifo;

    // If the starting nodes match:
    if (p_dNodeAnchor->appData1.ptr)
        bfsFifo.push_back(p_dNodeAnchor);

    // BFS through the fabric reporting
    while (! bfsFifo.empty()) {
        p_dNode = bfsFifo.front();
        bfsFifo.pop_front();
        // we got in here only with matching nodes:
        p_sNode = (IBNode *)p_dNode->appData1.ptr;
        if (!p_sNode) {
            cerr << "How did we get in BFS with missmatching nodes!" << endl;
            exit (1);
        }

        // Try to match all ports of the current nodes
        // we assume port counts match since the nodes match
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            IBPort *p_dPort = p_dNode->getPort(pn);
            IBPort *p_sPort = p_sNode->getPort(pn);

            if (! p_dPort || !p_sPort) continue;

            IBPort *p_dRemPort = p_dPort->p_remotePort;
            IBPort *p_sRemPort = p_sPort->p_remotePort;

            // we only do care about spec nodes for now:
            if (!p_sRemPort )
                continue;

            // if we did visit it by this reporting algorithm:
            if (TopoIsNodeMarkedAsReported(p_sRemPort->p_node)) continue;

            // if the remote spec node is not marked matched:
            if (!p_sRemPort->p_node->appData1.ptr) {
                if (TopoReportMismatchedNode(p_sRemPort->p_node, 1, diag))
                    anyMissmatchedSpecNodes++;
            } else {
                // mark as visited.
                TopoMarkNodeAsReported(p_sRemPort->p_node);

                // ok so we matched the nodes - put them in for next step:
                if (p_dRemPort) {
                    if (p_dRemPort->p_node->appData1.ptr)
                        bfsFifo.push_back(p_dRemPort->p_node);
                }
            }
        }           // all ports
    }           // next nodes available...

    if (anyMissmatchedSpecNodes)
        diag << endl;

    IBFabric *p_dFabric = p_dNodeAnchor->p_fabric;

    // now when we are done reporting missing - let us report extra nodes:
    // but we only want to count those who touch matching nodes BFS wise...
    // If the starting nodes match:
    if (p_dNodeAnchor->appData1.ptr) {
        bfsFifo.push_back(p_dNodeAnchor);
    } else {
        diag << "Even starting nodes do not match! "
                << "Expected:" << p_sNodeAnchor->name
                << " but it is probably not connected correctly." <<  endl;
        anyMissmatchedDiscNodes++;
    }

    // we track visited nodes in here by clearing their visited bits

    // BFS through the fabric reporting
    while (! bfsFifo.empty()) {
        p_dNode = bfsFifo.front();
        bfsFifo.pop_front();

        int anyPortMatch = 0;
        // Try to match all ports of the current nodes
        // we assume port counts match since the nodes match
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            IBPort *p_tmpPort = p_dNode->getPort(pn);
            if (! p_tmpPort)
                continue;

            IBPort *p_dRemPort = p_tmpPort->p_remotePort;
            if (!p_dRemPort)
                continue;

            IBNode *p_node = p_dRemPort->p_node;

            // we only care about missmatched discovered nodes
            if (! p_node->appData1.ptr) {
                // if not already visited.
                if (! TopoIsNodeMarkedAsReported(p_node))
                    if (TopoReportMismatchedNode(p_node, 0, diag))
                        anyMissmatchedDiscNodes++;
            } else {
                // if not already visited.
                if (! TopoIsNodeMarkedAsReported(p_node))
                    // it is an OK node so we traverse through
                    bfsFifo.push_back(p_node);
                anyPortMatch++;
            }
        }           // all ports

        p_sNode = (IBNode *)p_dNode->appData1.ptr;
        if (!anyPortMatch) {
            diag << "None of node:" << p_sNode->name
                    << " remote nodes match the topology."
                    << " No further extra nodes reported" << endl;
        }

        TopoMarkNodeAsReported(p_dNode);
    }           // next nodes available...

    if (anyMissmatchedDiscNodes)
        diag << endl;

    // finaly we want to report any missmatched cables between matching nodes
    for (map_str_pnode::iterator nI = p_dFabric->NodeByName.begin();
            nI != p_dFabric->NodeByName.end(); nI++) {
        p_dNode = (*nI).second;
        p_sNode = (IBNode *)p_dNode->appData1.ptr;
        if ( p_sNode ) {
            // report any missmatched links on this node.
            if (TopoReportMatchedNodesUnMatchingLinks(p_sNode, p_dNode, diag))
                anyMissmatchedLinks++;
        }
    }

    if (anyMissmatchedLinks)
        diag << endl;
    return anyMissmatchedLinks +
    anyMissmatchedDiscNodes +
    anyMissmatchedSpecNodes;
}


//////////////////////////////////////////////////////////////////////////////
static void
TopoCleanUpBeforeMerge(IBFabric *p_sFabric, IBFabric *p_dFabric)
{
    IBNode *p_node;
    // since we only use the appData1 and appData2 on all node  ...
    for (map_str_pnode::iterator nI = p_sFabric->NodeByName.begin();
            nI != p_sFabric->NodeByName.end(); nI++) {
        p_node = (*nI).second;
        p_node->appData1.ptr = 0;
        p_node->appData2.val = 0;
    }

    for (map_str_pnode::iterator nI = p_dFabric->NodeByName.begin();
            nI != p_dFabric->NodeByName.end(); nI++) {
        p_node = (*nI).second;
        p_node->appData1.ptr = 0;
        p_node->appData2.val = 0;
    }
}


//////////////////////////////////////////////////////////////////////////////
// Return 0 if fabrics match 1 otherwise.
int
TopoMatchFabrics(IBFabric   *p_sFabric,         // The specification fabric
        IBFabric   *p_dFabric,                  // The discovered fabric
        const char *anchorNodeName,             // The name of the system to be the anchor point
        int         anchorPortNum,              // The port number of the anchor port
        uint64_t    anchorPortGuid,             // Guid of the anchor port
        char **messages)
{
    stringstream diag, tmpDiag;
    int status = 0;

    IBNode *p_sNode, *p_dNode;
    IBPort *p_sPort, *p_dPort;

    // get the anchor node on the spec fabric - by name:
    p_sNode = p_sFabric->getNode(anchorNodeName);
    if (! p_sNode) {
        diag << "Fail to find anchor node:"
                << anchorNodeName << " on the specification fabric." << endl;
        status = 1;
        goto Exit;
    }

    // get the anchor port on the discovered fabric - by guid.
    p_dPort = p_dFabric->getPortByGuid(anchorPortGuid);
    if (! p_dPort) {
        diag << "Fail to find anchor port guid:"
                << guid2str(anchorPortGuid)
                << " in the discovered fabric." << endl;
        status = 1;
        goto Exit;
    }

    // get the anchor node on the discovered fabric - by port obj:
    p_dNode = p_dPort->p_node;
    if (! p_dNode) {
        diag << "Fail to find anchor node:"
                << anchorNodeName << " on the discoverd fabric." << endl;
        status = 1;
        goto Exit;
    }

    // Not management port
    if (anchorPortNum != 0) {
        // get the anchor port on the spec fabric - by port num:
        // we can do it only if we run from non management port because port0 isn't a phyisical port
        p_sPort = p_sNode->getPort(anchorPortNum);
        if (! p_sPort) {
            diag << "Fail to find anchor port:"
                    << anchorNodeName <<  anchorPortNum
                    << " in the specification fabric." << endl;
            status = 1;
            goto Exit;
        }
    }

    // Cleanup the flags we use for tracking matching and progress
    TopoCleanUpBeforeMerge(p_sFabric, p_dFabric);

    // Not management port
    if (anchorPortNum != 0) {
        // Do a BFS matching nodes from each fabrics. When this leaves
        // the appData1.ptr will cross point. status is 0 if there is no
        // difference
        status = TopoBFSAndMatchFromPorts(p_sPort, p_dPort, tmpDiag);
    } else {
        // Do a BFS matching nodes from each fabrics. When this leaves
        // the appData1.ptr will cross point. status is 0 if there is no
        // difference
        status = TopoBFSAndMatchFromNodes(p_sNode, p_dNode, tmpDiag);
    }
    if (status) {
        cout << "-W- Topology Matching First Phase Found Missmatches:\n"
                << tmpDiag.str() << endl;
    }

    // Do the second step in matching - rely on preexisting matched nodes
    // and try to map the unmatched.
    TopoMatchSpecNodesByAdjacentNode(p_sFabric);

    if (TopoReportMissmatches(p_sNode, p_dNode, diag))
        status = 1;

 Exit:
      string msg(diag.str());
      int msgLen = strlen(msg.c_str());
      if (msgLen) {
          *messages = (char *)malloc(msgLen + 1);
          strncpy(*messages, msg.c_str(), msgLen);
          (*messages)[msgLen] = '\0';
      } else {
          *messages = NULL;
      }
      return(status);
}


//////////////////////////////////////////////////////////////////////////////
// Given a disc node pointer create the merged system and node by inspecting
// the merged spec fabric too.
// The node will have all it's ports setup (no links) and the system and
// sysPorts as well
static IBNode *
TopoCopyNodeToMergedFabric(IBFabric *p_mFabric,
        IBNode   *p_dNode)
{
    IBNode    *p_sNode = (IBNode *)p_dNode->appData1.ptr;
    IBSystem  *p_system = 0;
    IBSysPort *p_sysPort = 0;
    IBNode    *p_node = 0;

    string nodeName = p_dNode->name;
    string sysName =  p_dNode->p_system->name;
    string sysType =  p_dNode->p_system->type;

    // if we have a matching spec node we use that instead:
    if (p_sNode) {
        nodeName = p_sNode->name;
        sysName =  p_sNode->p_system->name;
        sysType =  p_sNode->p_system->type;
    }

    // make sure the system exists:
    // NOTE - we can not use the makeSystem of the fabric since it
    // will instantiate the entire system.
    p_system = p_mFabric->getSystem(sysName);
    if (!p_system) {
        p_system = new IBSystem(sysName, p_mFabric, sysType);
        // copy other system info:
        p_system->guid_set(p_dNode->p_system->guid_get());
    }

    // create the node:
    p_node = p_mFabric->getNode(nodeName);
    if (! p_node) {
        p_node =
                p_mFabric->makeNode(nodeName, p_system, p_dNode->type,
                              p_dNode->numPorts);

        // copy extra info:
        p_node->guid_set(p_dNode->guid_get());
        p_node->devId  = p_dNode->devId;
        p_node->revId  = p_dNode->revId;
        p_node->vendId = p_dNode->vendId;

        if (p_sNode)
            p_node->attributes = p_sNode->attributes;

        if (p_dNode && p_dNode->attributes.size())
            if (p_node->attributes.size()) {
                p_node->attributes += string(",") + p_dNode->attributes;
            } else {
                p_node->attributes = p_dNode->attributes;
            }

        // create the node ports:
        for (unsigned int pn = 1; pn <=  p_dNode->numPorts; pn++) {
            // we only care if the discovered port exists:
            IBPort *p_dPort = p_dNode->getPort(pn);
            if (! p_dPort)
                continue;

            // some fabrics might be generated with all ports
            // pre-allocated.
            IBPort *p_port = p_node->getPort(pn);
            if (!p_port) p_port = new IBPort(p_node, pn);

            // copy some vital data:
            p_port->guid_set(p_dPort->guid_get());
            p_port->base_lid = p_dPort->base_lid;
            p_mFabric->setLidPort(p_port->base_lid, p_port);
            if (p_mFabric->maxLid < p_port->base_lid)
                p_mFabric->maxLid = p_port->base_lid;

            // if there is a matching spec port we use it and get the
            // sys port name etc
            IBPort *p_sPort = 0;
            if (p_sNode) p_sPort = p_sNode->getPort(pn);
            if (p_sPort) {
                // if we had a spec sysPort make it:
                if (p_sPort->p_sysPort) {
                    p_sysPort = new IBSysPort(p_sPort->p_sysPort->name, p_system);
                    p_sysPort->p_nodePort = p_port;
                    p_port->p_sysPort = p_sysPort;
                } else {
                    p_port->p_sysPort = 0;
                }
            } else {
                // the discovered fabric might have a sysport?
                if (p_dPort->p_sysPort) {
                    p_sysPort = new IBSysPort(p_dPort->p_sysPort->name, p_system);
                    p_sysPort->p_nodePort = p_port;
                    p_port->p_sysPort = p_sysPort;
                } else {
                    p_port->p_sysPort = 0;
                }
            }
        }
    }
    return p_node;
}


// Copy the link from the p_dPort to it's remote port to the
// given merged port.
static void
TopoCopyLinkToMergedFabric(IBFabric *p_mFabric,
        IBPort   *p_mPort,
        IBPort   *p_dPort)
{
    // make sure the remote node exists or copy it to the merged fabric
    IBNode *p_remNode =
            TopoCopyNodeToMergedFabric(p_mFabric, p_dPort->p_remotePort->p_node);

    // find the remote port on the merged fabric
    IBPort *p_remPort = p_remNode->getPort(p_dPort->p_remotePort->num);
    if (!p_remPort ) {
        cerr << "-F- No Remote port:" << p_dPort->p_remotePort->num
                << " on node:" << p_remNode->name << endl;
        exit(1);
    }

    // we need to create sys ports connection if both ports are on sys ports
    if (p_remPort->p_sysPort && p_mPort->p_sysPort) {
        p_remPort->p_sysPort->connect(p_mPort->p_sysPort,
                p_dPort->width,
                p_dPort->speed);
    } else {
/*
        // The following error messages are actually not required at
        // all. If there is a missmatch on an internal node this will happen.
        if (p_remPort->p_sysPort) {
            cout << "-E- Linking:" << p_mPort->getName() << " to:"
                    << p_remPort->p_node->name
                    << "/" << p_dPort->p_remotePort->num << endl;
            cout << "-F- BAD LINK since Remote System port:"
                    << p_remPort->p_sysPort->name << " exists, but no sys port for:"
                    << p_mPort->getName() << endl;
            // exit(1);
        } else if (p_mPort->p_sysPort) {
            cout << "-F- BAD LINK since local has System port:"
                    << p_mPort->p_sysPort->name << " exists, but no sys port for:"
                    << p_remPort->getName() << endl;
            // exit(1);
        } else
*/
        p_mPort->connect(p_remPort, p_dPort->width, p_dPort->speed);
        p_remPort->connect(p_mPort, p_dPort->width, p_dPort->speed);
    }
}


//////////////////////////////////////////////////////////////////////////////
// Build a merged fabric from a matched discovered and spec fabrics:
// * Every node from the discovered fabric must appear
// * We use matched nodes and system names.
int
TopoMergeDiscAndSpecFabrics(IBFabric  *p_sFabric,
        IBFabric  *p_dFabric,
        IBFabric  *p_mFabric)
{
    // go through all nodes of the discovered fabric.
    // copy all their systems, and links...
    p_dFabric->setLidPort(0, NULL);
    p_dFabric->minLid = 1;
    p_dFabric->maxLid = 0;

    for (map_str_pnode::iterator nI = p_dFabric->NodeByName.begin();
            nI != p_dFabric->NodeByName.end(); nI++) {
        IBNode *p_dNode = (*nI).second;

        // make sure the node exists as well as the system and ports and
        // sysports:
        IBNode *p_node = TopoCopyNodeToMergedFabric(p_mFabric, p_dNode);

        // go over all ports and connect accordingly:
        for (unsigned int pn = 1; pn <=  p_dNode->numPorts; pn++) {
            IBPort *p_dPort = p_dNode->getPort(pn);
            IBPort *p_port = p_node->getPort(pn);

            // we got a remote connection:
            if (p_port && p_dPort && p_dPort->p_remotePort) {
                TopoCopyLinkToMergedFabric(p_mFabric, p_port, p_dPort);
            }
        }
    }
    p_mFabric->minLid = p_dFabric->minLid;
    return 0;
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Another approach for matching fabric starting on the edges and
// moving into the fabric. Using NAMES to match.
//----------------------------------------------------------------------
//----------------------------------------------------------------------
static int
isGUIDBasedName(IBNode *p_node) {
    string g = guid2str(p_node->guid_get());
    string n = p_node->name;
    const char *gs = g.c_str()+2;
    const char *ns = n.c_str()+1;
    //cout << "G:" << gs << endl << " N:" << ns << endl;
    return (!strncmp(gs,ns,6));
}

// Scan through all CA nodes and try to match them by the name
// return the list of matches found an error for those extra or missing
static int
TopoMatchCAsByName(IBFabric *p_sFabric, IBFabric *p_dFabric,
        list<IBNode*> &matchingDiscHosts, stringstream &s)
{
    int anyMiss = 0;
    int numMatchedCANodes = 0;
    s << "-I- Matching all hosts by name ..." << endl;
    // we go through entire discovered fabric trying to find match by name
    for (map_str_pnode::iterator nI = p_dFabric->NodeByName.begin();
            nI != p_dFabric->NodeByName.end();
            nI++) {
        IBNode *p_node1 = (*nI).second;
        if (p_node1->type != IB_CA_NODE) continue;

        // so we try to find a node by the same name on the spec fabric.
        map_str_pnode::iterator snI = p_sFabric->NodeByName.find((*nI).first);

        // no match
        if (snI == p_sFabric->NodeByName.end()) {
            if (isGUIDBasedName(p_node1)) {
                s << "-W- Discovered Host CA with GUID:"
                << guid2str(p_node1->guid_get())
                << " have no valid IB node name (NodeDesc) and can not be checked."
                << endl;
            } else {
                s << "-E- Discovered Host CA " << p_node1->name
                        << " does not appear in the topology file." << endl;
                anyMiss++;
            }
        } else {
            IBNode *p_node2 = (*snI).second;
            // make sure they are not previously match somehome
            if (p_node1->appData1.ptr || p_node2->appData1.ptr)
                continue;
            TopoMarkNodeAsMatchAlgoVisited(p_node1);
            TopoMarkMatcedNodes(p_node1, p_node2, numMatchedCANodes);
            matchingDiscHosts.push_back(p_node1);
        }
    }

    // now try the spec topology and report missing hosts
    for (map_str_pnode::iterator nI = p_sFabric->NodeByName.begin();
            nI != p_sFabric->NodeByName.end(); nI++) {
        IBNode *p_node1 = (*nI).second;
        if (p_node1->type != IB_CA_NODE)
            continue;

        if (!p_node1->appData1.ptr) {
            s << "-E- Missing Topology Host CA " << p_node1->name
                    << " does not appear in the discovered topology." << endl;
            anyMiss++;
        }
    }
    if (anyMiss)
        s << "-E- Total " << anyMiss << " hosts did not match by name" << endl;
    s << "-I- Matched " << numMatchedCANodes << " hosts by name" << endl;
    return(anyMiss);
}


// Given the set of discovered host nodes check to which switches
// they should have connect and check the grouping matches
// return the number of miss matching groups found and the list
// of matching switches
static int
AnalyzeMatchingCAGroups(IBFabric *p_sFabric, IBFabric *p_dFabric,
        list<IBNode*> &matchingDiscHosts,
        list<IBNode*> &matchingDiscLeafSw, stringstream &s)
{
    s << "-I- Analyzing host groups connected to same leaf switches ..." << endl;
    int numMatchedLeafSw = 0;
    int anyMiss = 0;
    // check all ouput ports of the given set of host nodes
    // build the set of spec switces they should have connected too
    set<IBNode*> specLeafSw;
    for (list<IBNode*>::const_iterator lI = matchingDiscHosts.begin();
            lI != matchingDiscHosts.end(); lI++) {
        IBNode *p_dNode = *lI;
        IBNode *p_sNode = (IBNode *)p_dNode->appData1.ptr;
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            // we do not care for non discoeverd ports
            IBPort *p_dPort = p_dNode->getPort(pn);
            if (!p_dPort)
                continue;
            // we do not care if they are not in the spec
            IBPort *p_sPort = p_sNode->getPort(pn);
            if (!p_sPort)
                continue;

            if (!p_sPort->p_remotePort)
                continue;
            if (p_sPort->p_remotePort->p_node->type != IB_SW_NODE)
                continue;

            specLeafSw.insert(p_sPort->p_remotePort->p_node);
        }
    }

    // for each of the spec switches
    for (set<IBNode*>::const_iterator sI = specLeafSw.begin();
            sI != specLeafSw.end(); sI++) {
        IBNode *p_sNode = (*sI);

        // we need to track disc switches off hosts on this switch
        map< IBNode*, list< IBNode *>, less< IBNode*> > discLeafSw;

        // will finaly hold the matching swith
        IBNode *p_dNode;

        // for each host on the switch
        for (unsigned int pn = 1; pn <= p_sNode->numPorts; pn++) {
            // collect the switch the discovered host connects to
            IBPort *p_sPort = p_sNode->getPort(pn);
            if (!p_sPort)
                continue;
            if (!p_sPort->p_remotePort)
                continue;
            if (p_sPort->p_remotePort->p_node->type != IB_CA_NODE)
                continue;

            // we are about the remote node only if is a matched host
            if (!p_sPort->p_remotePort->p_node->appData1.ptr)
                continue;

            IBNode *p_dCANode =
                    (IBNode *)p_sPort->p_remotePort->p_node->appData1.ptr;
            IBPort *p_dCAPort = p_dCANode->getPort(p_sPort->p_remotePort->num);
            if (!p_dCAPort || !p_dCAPort->p_remotePort)
                continue;
            p_dNode = p_dCAPort->p_remotePort->p_node;
            discLeafSw[p_dNode].push_back(p_dCANode);
        }

        // eventually if there is only one discovered switch
        if (discLeafSw.size() == 1) {
            // match was found
            if (isGUIDBasedName(p_dNode)) {
                // catch the case where the switch is "named" with different name
                // s << "-I- All " << discLeafSw[p_dNode].size()
                //  << " hosts on switch:" << p_sNode->name
                //  << " are connected to switch:" << p_dNode->name << endl;
                TopoMarkNodeAsMatchAlgoVisited(p_dNode);
                TopoMarkMatcedNodes(p_dNode, p_sNode, numMatchedLeafSw);
                matchingDiscLeafSw.push_back(p_dNode);
            } else {
                if (p_dNode->name != p_sNode->name) {
                    s << "-E- All " << discLeafSw[p_dNode].size()
                            << " hosts on switch:" << p_sNode->name
                            << " are connected to switch:" << p_dNode->name
                            << endl;
                    anyMiss++;
                } else {
                    TopoMarkNodeAsMatchAlgoVisited(p_dNode);
                    TopoMarkMatcedNodes(p_dNode, p_sNode, numMatchedLeafSw);
                    matchingDiscLeafSw.push_back(p_dNode);
                }
            }
        } else {
            anyMiss++;
            // else report which host connects to which switch
            s << "-E- Hosts that should connect to switch:" << p_sNode->name
                    << " are connecting to multiple switches: " << endl;
            map< IBNode*, list< IBNode *>, less< IBNode*> >::iterator mI;
            for (mI = discLeafSw.begin(); mI != discLeafSw.end(); mI++) {
                s << "    to:" << (*mI).first->name << " hosts:";
                for (list< IBNode *>::iterator lI = (*mI).second.begin();
                        lI != (*mI).second.end(); lI++) {
                    s << (*lI)->name << ",";
                }
                s << endl;
            }
        }
    }

    if (anyMiss) {
        s << "-E- Total of " << anyMiss
                << " leaf switches could not be matched." << endl <<"    since they are connected to hosts that should connect to different leaf switches." << endl;
    }

    s << "-I- Matched " << matchingDiscLeafSw.size()
            << " leaf switches - connecting to matched hosts" << endl;
  return(anyMiss);
}


// Validate all matching host ports are connected to correct switch port
static int
CheckMatchingCAPortsToMatchingSwPortNums(IBFabric *p_sFabric, IBFabric *p_dFabric,
    list<IBNode*> &matchingDiscHosts,
    stringstream &s)
{
    // go over all matching hosts ports
    // if the host remote port connect to matching switch
    // validate the port numbers match
    int anyMiss = 0;
    int numMatchPorts = 0;
    // check all output ports of the given set of host nodes
    for (list<IBNode*>::const_iterator lI = matchingDiscHosts.begin();
            lI != matchingDiscHosts.end(); lI++) {
        IBNode *p_dNode = *lI;
        IBNode *p_sNode = (IBNode *)p_dNode->appData1.ptr;
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            IBPort *p_sPort = p_sNode->getPort(pn);
            if (!p_sPort || !p_sPort->p_remotePort)
                continue;
            IBNode *p_sRemNode = p_sPort->p_remotePort->p_node;

            IBPort *p_dPort = p_dNode->getPort(pn);
            if (!p_dPort || !p_dPort->p_remotePort) {
                s << "-E- Host " << p_sNode->name << " port " << p_sPort->getName()
                        << " should connect to switch port: "
                        << p_sPort->p_remotePort->getName()
                        << " but is disconnected" << endl;
                anyMiss++;
                continue;
            }
            IBNode *p_dRemNode = p_dPort->p_remotePort->p_node;

            if ((IBNode*)p_sRemNode->appData1.ptr != p_dRemNode)
                continue;

            // now we know they both point to same switch - check the port
            // connection and properties
            if (p_sPort->p_remotePort->num != p_dPort->p_remotePort->num) {
                s << "-E- Host " << p_sNode->name << " port " << p_sPort->getName()
                        << " connects to switch port: " << p_dPort->p_remotePort->getName()
                        << " and not according to topology: "
                        << p_sPort->p_remotePort->getName() << endl;
                anyMiss++;
            } else {
                numMatchPorts++;
            }
            if (p_sPort->width != p_dPort->width) {
                s << "-W- Wrong link width on:" << p_sPort->getName()
                        << ". Expected:" << width2char(p_sPort->width)
                        << " got:" << width2char(p_dPort->width) << endl;
            }
            if (p_sPort->speed != p_dPort->speed) {
                s << "-W- Wrong link speed on:" << p_sPort->getName()
                        << ". Expected:" << speed2char(p_sPort->speed)
                        << " got:" << speed2char(p_dPort->speed) << endl;
            }
        }
    }
    if (anyMiss) {
        s << "-E- Total of " << anyMiss
                << " CA ports did not match connected switch port" << endl;
    }
    s << "-I- Total of " << numMatchPorts
            << " CA ports match connected switch port" << endl;
  return(anyMiss);
}


// Start for the current set of matched switches
// try all ports and look for remote side switches
// if remote ports match collect all the switches reached
// now go over all reached switches and validate back all connections
// to matched switches - report missmatches or report the new set
// return the number of new matches found
static int
TopoMatchSwitches(IBFabric *p_sFabric,
        IBFabric *p_dFabric,
        list<IBNode*> &oldMatchingSws,
        list<IBNode*> &newMatchingSws,
        stringstream &s)
{
    s << "-I- Matching nodes connected to previously matched nodes ..." << endl;
    int anyMiss = 0;
    int numMatchedSws = 0;
    //  map<IBNode*, IBNode*, less<IBNode*> > mapSpecToPropose;
    list< IBNode* > specSW;

    // Collect all next step candidates:
    // - connected to this step sw and
    // - the remote port num match
    // - not previously matched
    for (list<IBNode*>::const_iterator lI = oldMatchingSws.begin();
            lI != oldMatchingSws.end(); lI++) {
        IBNode *p_dNode = *lI;
        IBNode *p_sNode = (IBNode *)p_dNode->appData1.ptr;
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            // we do not care for non discoeverd ports
            IBPort *p_dPort = p_dNode->getPort(pn);
            if (!p_dPort)
                continue;
            // we do not care if they are not in the spec
            IBPort *p_sPort = p_sNode->getPort(pn);
            if (!p_sPort)
                continue;

            if (!p_sPort->p_remotePort || !p_dPort->p_remotePort)
                continue;
            if (p_sPort->p_remotePort->p_node->type != IB_SW_NODE)
                continue;
            if (p_dPort->p_remotePort->p_node->type != IB_SW_NODE)
                continue;
            // but the remote switch must not be a matched one!
            if (p_sPort->p_remotePort->p_node->appData1.ptr)
                continue;
            if (p_dPort->p_remotePort->p_node->appData1.ptr)
                continue;

            // we only consider perfect matches
            if (p_sPort->p_remotePort->num != p_dPort->p_remotePort->num)
                continue;

            specSW.push_back(p_sPort->p_remotePort->p_node);
        }
    }

    // we need to track potential matchings over all this step sw
#define map_pnode_set_pnode  map< IBNode*, set< IBNode *, less<IBNode *> >, less< IBNode*> >
    map_pnode_set_pnode candidDiscSw;
    map_pnode_set_pnode candidSpecSw;
#define set_pair_pnode_pn set< pair< IBNode *, unsigned int>, less< pair< IBNode *, unsigned int> > >
#define map_pnode_list_pair_pnode_pn map< IBNode*, set_pair_pnode_pn , less< IBNode*> >
    map_pnode_list_pair_pnode_pn connectToSwitch;

    // To be declared as matching we check all previously matched
    // remote side spec switches have their disc switches pointing
    // to same un-matched switch
    for (list<IBNode*>::iterator sI = specSW.begin();
            sI != specSW.end(); sI++) {
        IBNode *p_sCandidSw = (*sI);
        // for each port on the proposed switch
        for (unsigned int pn = 1; pn <= p_sCandidSw->numPorts; pn++) {
            // collect the remote switches the discovered host connects to
            // we only care about ports that connect to previously
            // matching switches
            IBPort *p_sPort = p_sCandidSw->getPort(pn);
            if (!p_sPort || !p_sPort->p_remotePort)
                continue;
            if (p_sPort->p_remotePort->p_node->type != IB_SW_NODE)
                continue;
            if (!p_sPort->p_remotePort->p_node->appData1.ptr)
                continue;

            // track the spec connections
            pair< IBNode*, unsigned int> tmp(p_sPort->p_remotePort->p_node,
                               p_sPort->p_remotePort->num);
            connectToSwitch[p_sCandidSw].insert(tmp);

            // now we can obtain the matching discovered switch
            IBNode *p_dRemNode =
                    (IBNode *)p_sPort->p_remotePort->p_node->appData1.ptr;
            if (!p_dRemNode->appData1.ptr) {
                cout << "BUG  No pointer back?" << endl;
                exit(1);
            }
            // to get the discovered candidate we need the port to be connected
            IBPort *p_dRemPort = p_dRemNode->getPort(p_sPort->p_remotePort->num);
            if (!p_dRemPort || !p_dRemPort->p_remotePort)
                continue;

            // must match the spec pin num we started with
            if (p_dRemPort->p_remotePort->num != pn)
                continue;

            IBNode *p_dCandidSw = p_dRemPort->p_remotePort->p_node;
            if (p_dCandidSw->type != IB_SW_NODE)
                continue;
            // can not be previously matched
            if (p_dCandidSw->appData1.ptr)
                continue;

            pair< IBNode*, unsigned int> tmp2(p_dRemNode,
                    p_sPort->p_remotePort->num);
            connectToSwitch[p_dCandidSw].insert(tmp2);

            // collect all candidate discoeverd switches for the spec switch
            candidDiscSw[p_sCandidSw].insert(p_dCandidSw);
            candidSpecSw[p_dCandidSw].insert(p_sCandidSw);
        }
    }

    // now after checking all switches we can use the candidate mapps:
    // go over all disc candidates for each of the spec switches
    map_pnode_set_pnode::iterator smI;
    for (smI = candidDiscSw.begin(); smI != candidDiscSw.end(); smI++) {
        IBNode *p_sNode = (*smI).first;
        // if we only had one matching discovered switch
        if ((*smI).second.size() == 1) {
            IBNode *p_dNode = (*(*smI).second.begin());
            // we still can have two spec switches pointing to it
            if (candidSpecSw[p_dNode].size() == 1) {
                // we got a 1 to 1 match
                //s << "-I- Matching switch:" << p_sNode->name
                //  << " to discovered switch:" << p_dNode->name << endl;
                TopoMarkNodeAsMatchAlgoVisited(p_dNode);
                TopoMarkMatcedNodes(p_dNode, p_sNode, numMatchedSws);
                newMatchingSws.push_back(p_dNode);
            } else {
                s << "-E- " << candidSpecSw[p_dNode].size()
                        << " different topology switches mixed with discovered switch:"
                        << p_dNode->name << endl;
                anyMiss++;
            }
        } else {
            s << "-E- There are:" << candidDiscSw[p_sNode].size()
                    << " candidate discovered switches to match switch:"
                    << p_sNode->name << " which should be connected to: "<< endl;
            set_pair_pnode_pn::iterator lI;
            for (lI = connectToSwitch[p_sNode].begin();
                    lI != connectToSwitch[p_sNode].end(); lI++) {
                IBPort *p_sPort = (*lI).first->getPort((*lI).second);
                if (!p_sPort || !p_sPort->p_remotePort)
                    continue;
                s << "       " << (*lI).first->name << "/P" << (*lI).second
                        << " connects to port:" << p_sPort->p_remotePort->getName()
                        << " which is device port:" << p_sPort->p_remotePort->num << endl;
            }
            s << "   The possible candidates are:" << endl;
            set< IBNode*, less<IBNode*> >::iterator sI;
            for (sI = candidDiscSw[p_sNode].begin();
                    sI != candidDiscSw[p_sNode].end(); sI++) {
                IBNode *p_dNode = (*sI);
                s << "     Switch " << p_dNode->name << " connected to:" << endl;
                for (lI = connectToSwitch[p_dNode].begin();
                        lI != connectToSwitch[p_dNode].end(); lI++) {
                    // this is the discovered switch connected to the candidate
                    IBNode *p_dRemSwitch = (*lI).first;
                    IBPort *p_dRemPort = p_dRemSwitch->getPort((*lI).second);
                    if (!p_dRemPort || !p_dRemPort->p_remotePort)
                        continue;

                    // this is the matching node of the switch connected to the
                    // candidate
                    IBNode *p_mNode = (IBNode*)(p_dRemSwitch->appData1.ptr);
                    if (!p_mNode) {
                        // its a bug since we must have matched those already
                        cout << "BUG : must have matched already" << endl;
                        exit(1);
                    }

                    // to be relevant it must be connected to our spec switch
                    // through the same port num

                    // we must match port number of the remote node with the
                    // expected port num from the spec node
                    IBPort *p_sRemPort = p_sNode->getPort(p_dRemPort->p_remotePort->num);
                    if (p_mNode != p_sRemPort->p_remotePort->p_node)
                        continue;
                    if ((*lI).second != p_sRemPort->p_remotePort->num)
                        continue;
                    s << "       " << p_mNode->name << "/P" << (*lI).second
                            << " connects to port:" << p_dRemPort->p_remotePort->getName() << endl;
                }
            }
            anyMiss++;
        }
    }
    if (numMatchedSws) {
        s << "-I- Successfuly matched " << numMatchedSws
                << " more switches" << endl;
    }
    return(numMatchedSws);
}


// go over entire fabric from edges and in and report
// each missmatch - do not propogate inwards through missmatches
static int
BfsFromEdgReportingMatcStatus(IBFabric *p_sFabric, IBFabric *p_dFabric,
        stringstream &s)
{
    // go over all matching nodes
    // if the remote port connect to matching node
    // validate the port numbers match
    stringstream missMsg;
    stringstream extrMsg;
    stringstream warnMsg;
    stringstream badMsg;
    int numMiss = 0;
    int numExtr = 0;
    int numWarn = 0;
    int numBad  = 0;
    int numMatchPorts = 0;
    // check all output ports of the given set of host nodes
    for (map_str_pnode::iterator nI = p_dFabric-> NodeByName.begin();
            nI != p_dFabric-> NodeByName.end(); nI++) {
        IBNode *p_dNode = (*nI).second;
        if (!p_dNode->appData1.ptr)
            continue;
        IBNode *p_sNode = (IBNode *)p_dNode->appData1.ptr;

        // go over all ports
        for (unsigned int pn = 1; pn <= p_dNode->numPorts; pn++) {
            IBPort *p_sPort = p_sNode->getPort(pn);
            IBPort *p_dPort = p_dNode->getPort(pn);

            bool sConnected = (p_sPort && p_sPort->p_remotePort);
            bool dConnected = (p_dPort && p_dPort->p_remotePort);

            if (!sConnected && !dConnected) {
                continue;
            } else if (sConnected && !dConnected) {
                // avoid double rep
                if (p_sPort < p_sPort->p_remotePort) {
                    missMsg << "-E- Missing cable between " << p_sPort->getName()
                            << " and " << p_sPort->p_remotePort->getName() << endl;
                    numMiss++;
                }
            } else if (!sConnected && dConnected) {
                if (p_dPort < p_dPort->p_remotePort) {
                    // we want to use as much "match" info in the report:
                    IBNode *p_repNode = p_dPort->p_remotePort->p_node;
                    IBPort *p_repPort = p_dPort->p_remotePort;
                    if (p_repNode->appData1.ptr) {
                        p_repNode =
                                (IBNode*)p_dPort->p_remotePort->p_node->appData1.ptr;
                        p_repPort = p_repNode->getPort(p_dPort->p_remotePort->num);
                    }
                    if (p_repPort) {
                        extrMsg << "-W- Extra cable between " << p_sNode->name << "/P"
                                << p_dPort->num << " and " << p_repPort->getName() << endl;
                    } else if (p_repNode) {
                        extrMsg << "-W- Extra cable between " << p_sNode->name << "/P"
                                << p_dPort->num << " and " << p_repNode->name
                                << "/P" << p_dPort->p_remotePort->num << endl;
                    }
                    numExtr++;
                }
            } else {
                // both ports exist - check they match
                IBNode *p_sRemNode = p_sPort->p_remotePort->p_node;
                IBNode *p_dRemNode = p_dPort->p_remotePort->p_node;

                // may be connected to non matched nodes
                if (!p_dRemNode->appData1.ptr || !p_sRemNode->appData1.ptr)
                    continue;

                IBNode *p_sActRemNode = (IBNode *)p_dRemNode->appData1.ptr;
                IBPort *p_sActRemPort =
                        p_sActRemNode->getPort(p_dPort->p_remotePort->num);
                // so they must be same nodes...
                if (p_sActRemNode != p_sRemNode) {
                    badMsg << "-E- Missmatch: port:" << p_sPort->getName()
                            << " should be connected to:"
                            <<  p_sPort->p_remotePort->getName()
                            << " but connects to:" << p_sActRemPort->getName() << endl;
                    numBad++;
                    continue;
                }

                // now we know they both point to same switch - check the port
                // connection and properties
                if (p_sPort->p_remotePort->num != p_dPort->p_remotePort->num) {
                    badMsg << "-E- Missmatch: port " << p_sPort->getName()
                            << " should be connected to:"
                            <<  p_sPort->p_remotePort->getName()
                            << " but connects to port: "
                            << p_sActRemPort->getName() << endl;
                    numBad++;
                } else {
                    numMatchPorts++;
                }
                if (p_sPort->width != p_dPort->width) {
                    warnMsg << "-W- Wrong link width on:" << p_sPort->getName()
                            << ". Expected:" << width2char(p_sPort->width)
                            << " got:" << width2char(p_dPort->width) << endl;
                    numWarn++;
                }
                if (p_sPort->speed != p_dPort->speed) {
                    warnMsg << "-W- Wrong link speed on:" << p_sPort->getName()
                            << ". Expected:" << speed2char(p_sPort->speed)
                            << " got:" << speed2char(p_dPort->speed) << endl;
                    numWarn++;
                }
            } // match both spec and disc
        } // all ports
    }

    if (numMiss) {
        s << "-------------------------------------------------------------------"
                << endl;
        s << "-E- Total of " << numMiss
                << " missing nodes that should have been connected to matched nodes:"
                << endl << missMsg.str();
    }
    if (numExtr) {
        s << "-------------------------------------------------------------------"
                << endl;
        s << "-E- Total of " << numExtr
                << " extra nodes that are connected to matched nodes:"
                << endl << extrMsg.str();
    }
    if (numBad) {
        s << "-------------------------------------------------------------------"
                << endl;
        s << "-E- Total of " << numBad
                << " connections from matched nodes to the wrong nodes:"
                << endl << badMsg.str();
    }
    if (numWarn) {
        s << "-------------------------------------------------------------------"
                << endl;
        s << "-W- Total of " << numBad
                << " connections differ in speed or width:"
                << endl << warnMsg.str();
    }

    s << "-------------------------------------------------------------------"
            << endl;
    s << "-I- Total of " << numMatchPorts
            << " ports match the provided topology" << endl;
    return(numBad+numMiss+numExtr);
}


// Return 0 if fabrics match 1 otherwise.
// The detailed compare messages are returned in messages
int
TopoMatchFabricsFromEdge(IBFabric *p_sFabric,           // The specification fabric
        IBFabric *p_dFabric,                            // The discovered fabric
        char **messages)
{
    stringstream diag, tmpDiag;
    int status = 0;

    // Cleanup the flags we use for tracking matching and progress
    TopoCleanUpBeforeMerge(p_sFabric, p_dFabric);

    diag << "-----------------------------------------------------------------"
            << endl;
    // We start by working the hosts
    list<IBNode*> matchingDiscHosts;
    int nCAMiss =
            TopoMatchCAsByName(p_sFabric, p_dFabric, matchingDiscHosts, diag);
    diag << "-----------------------------------------------------------------"
            << endl;
    // Now classify the match by name hosts to groups
    list<IBNode*> matchingDiscLeafSw;
    int numLeafSwMiss =
            AnalyzeMatchingCAGroups(p_sFabric, p_dFabric, matchingDiscHosts,
                        matchingDiscLeafSw, diag);

    diag << "-----------------------------------------------------------------"
            << endl;

/*
    int numUnmatchCaPorts =
            CheckMatchingCAPortsToMatchingSwPortNums(p_sFabric,
                    p_dFabric, matchingDiscHosts,
                    diag);
    diag << "-----------------------------------------------------------------"
            << endl;
*/

    list<IBNode*> oldMatchingSws = matchingDiscLeafSw;
    list<IBNode*> newMatchingSws;
    // now iterate untill no more new matches
    while (TopoMatchSwitches(p_sFabric, p_dFabric,
                       oldMatchingSws, newMatchingSws, diag)) {
        oldMatchingSws = newMatchingSws;
        newMatchingSws.clear();
        diag << "-----------------------------------------------------------------"
                << endl;
    }

    // produce final matching info report
    status = BfsFromEdgReportingMatcStatus(p_sFabric, p_dFabric, diag);
    diag << "-----------------------------------------------------------------"
            << endl;

Exit:
      string msg(diag.str());
      int msgLen = strlen(msg.c_str());
      if (msgLen) {
          *messages = (char *)malloc(msgLen + 1);
          strncpy(*messages, msg.c_str(), msgLen);
          (*messages)[msgLen] = '\0';
      } else {
          *messages = NULL;
      }
      return(status);
}
