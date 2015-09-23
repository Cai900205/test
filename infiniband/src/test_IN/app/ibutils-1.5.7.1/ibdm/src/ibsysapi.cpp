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
#include <set>
#include "ibsysapi.h"

static IBFabric *fabric = NULL;
int ibSysVerbose = 0;


/* read the IBNL with specific configuration and prepare the local fabric */
int
ibSysInit(char *sysType, char *cfg)
{
    if (fabric)
        delete fabric;

    fabric = new IBFabric();\
    IBSystem *p_system = fabric->makeSystem("SYS", sysType, cfg);
    if (!p_system) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to make system:%s with cfg:%s\n",sysType, cfg);
        }
        return(1);
    }

    if (ibSysVerbose & IBSYS_INFO)
        printf("Info: initialized fabric with single system of type %s\n", sysType);
    return(0);
}


// DFS in the direction of the destination node adding paths as discovered
static int
dfsFromNodeToNode(IBNode *curNode, IBNode *toNode,
        int *path, int hop, int *numFound, int numReq, int **drPaths,
        set< IBNode *, less< class IBNode *> > &visited,
        map<IBNode*, int > &esprance)
{
    if (ibSysVerbose & IBSYS_DEBUG) {
        printf("Debug: dfsFromNodeToNode at node %s hop:%d\n",
                curNode->name.c_str(), hop);
    }

    if (curNode == toNode) {
        for (int i = 0; i < hop; i++)
            drPaths[*numFound][i] = path[i];
        drPaths[*numFound][hop] = -1;
        if (ibSysVerbose & IBSYS_DEBUG) {
            printf("Debug: found %s at path #%d :", toNode->name.c_str(), *numFound);
            int i = 0;
            while (drPaths[*numFound][i] != -1)
                printf("%d,", drPaths[*numFound][i++]);
            printf("\n");
        }
        (*numFound)++;
        return(0);
    }

    /* sort the ports by the esprance of the next node */
    map< int, list< int > > rankPorts;

    /* try all ports */
    map< int, list< int > >::iterator rI;
    for (int pn = 1; pn <= curNode->numPorts; pn++) {
        IBPort *port = curNode->getPort(pn);
        if (port && port->p_remotePort) {
            IBNode *remNode = port->p_remotePort->p_node;
            int rank = esprance[remNode];
            if (rankPorts.find(rank) == rankPorts.end()) {
                list< int > tmp;
                rankPorts[rank] = tmp;
            }

            rI = rankPorts.find(rank);
            (*rI).second.push_back(pn);
        }
    }

    /* now use the ranks and list of ports and dfs in order */
    int curRank = esprance[curNode];
    for (rI = rankPorts.begin(); rI != rankPorts.end(); rI++) {
        for (list< int >::iterator lI = (*rI).second.begin();
                lI != (*rI).second.end(); lI++) {
            int pn = *lI;
            IBPort *port = curNode->getPort(pn);
            IBNode *remNode = port->p_remotePort->p_node;
            // do not take secenic rouets - always go towards toNode
            int rank = esprance[remNode];
            if (rank >= curRank) continue;
            // only if not already visited
            if (visited.find(remNode) == visited.end()) {
                // we do want to find all paths to toNode ...
                if (remNode != toNode) visited.insert(remNode);
                path[hop] = pn;
                if (!dfsFromNodeToNode(remNode, toNode, path, hop+1,
                        numFound, numReq, drPaths, visited, esprance))
                    if (*numFound == numReq) return(0);
            }
        }
    }

    /* we did not find all requested paths ... */
    return(1);
}


// Mark distance to destination node in "esprance"
static int
bfsFromToNodeMarkEsprance(IBNode *toNode, map<IBNode*, int > &esprance)
{
    list< IBNode *> q;
    int rank;
    esprance[toNode] = 0;
    q.push_back(toNode);
    while (! q.empty()) {
        IBNode *node = q.front();
        q.pop_front();

        // go over all ports of the node and add to q only non visited
        for (unsigned int pn = 1; pn <= node->numPorts; pn++) {
            IBPort *port = node->getPort(pn);
            if (!port || !port->p_remotePort) continue;
            rank = esprance[node];
            if (esprance.find(port->p_remotePort->p_node) == esprance.end()) {
                esprance[port->p_remotePort->p_node] = rank + 1;
                q.push_back(port->p_remotePort->p_node);
            }
        }
    }

    if (ibSysVerbose & IBSYS_DEBUG) {
        for ( map<IBNode*, int >::const_iterator eI = esprance.begin();
                eI != esprance.end(); eI++) {
            printf("Debug: Node:%s Esprance:%d\n", (*eI).first->name.c_str(),
                    (*eI).second);
        }
    }
    return(0);
}


/* query DR paths to a node  - paths are pre allocated */
/* return the number of filled paths in numPaths */
/* end of DR is a -1 */
int
ibSysGetDrPathsToNode(char *fromNode, char *toNode,
        int *numPaths, int**drPaths)
{
    if (!fabric) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: fabric was not initialized.\n");
        }
        return(1);
    }

    if (!drPaths) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No output path table provided\n");
        }
        return(1);
    }

    if (!numPaths) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No number of paths provided\n");
        }
        return(1);
    }

    if (!fromNode) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No source node provided\n");
        }
        return(1);
    }

    if (!toNode) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No destination node provided\n");
        }
        return(1);
    }

    IBNode *sNode = fabric->getNode(fromNode);
    if (!sNode) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find from-node %s\n", fromNode);
        }
        return(1);
    }
    IBNode *dNode = fabric->getNode(toNode);
    if (!dNode) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find to-node %s\n", toNode);
        }
        return(1);
    }

    map<IBNode*, int > esprance;
    set< IBNode *, less< class IBNode *> > visited;

    int path[64];
    int hop = 0;
    path[0] = 0;
    path[1] = -1;

    /* will decrement as we go */
    int numFound = 0;

    bfsFromToNodeMarkEsprance(dNode, esprance);

    /* DFS to the Node from the local node */
    dfsFromNodeToNode(sNode, dNode, path, hop+1, &numFound, *numPaths,
                  drPaths, visited, esprance);

    *numPaths = numFound;
    if (ibSysVerbose & IBSYS_INFO) {
        printf("Info: found %d paths from %s to %s\n",
                *numPaths, fromNode, toNode);
    }
    return(0);
}


/* query all node names in the system */
int
ibSysGetNodes(int *numNodes, const char **nodeNames)
{
    int i;
    if (!numNodes) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: no num nodes provided.\n");
        }
        return(1);
    }

    if (!nodeNames) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: no nodeNames array provided.\n");
        }
        return(1);
    }

    if (!fabric) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: fabric was not initialized.\n");
        }
        return(1);
    }

    i = 0;
    for(map_str_pnode::const_iterator I = fabric->NodeByName.begin();
            (i < *numNodes) && (I != fabric->NodeByName.end()); I++) {
        nodeNames[i++]=(*I).first.c_str();
    }

    *numNodes = i;
    if (ibSysVerbose & IBSYS_INFO) {
        printf("Info: found %d nodes\n", i);
    }
    return(0);
}


/* query name of front panel port of a node port */
int
ibSysGetNodePortSysPort(char *nodeName, int portNum,
        const char **sysPortName)
{
    if (!fabric) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: fabric was not initialized.\n");
        }
        return(1);
    }

    if (!nodeName) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No node name provided\n");
        }
        return(1);
    }

    if (!sysPortName) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No destination port name provided\n");
        }
        return(1);
    }

    IBNode *node = fabric->getNode(nodeName);
    if (!node) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find node:%s\n", nodeName);
        }
        return(1);
    }

    IBPort *port = node->getPort(portNum);
    if (!port) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: no connection at node:%s port:%d\n",
                    nodeName, portNum);
        }
        *sysPortName = NULL;
        return(1);
    }

    if (!port->p_sysPort) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: no front pannel port at node:%s port:%d\n",
                    nodeName, portNum);
        }
        *sysPortName = NULL;
        return(1);
    }
    *sysPortName = port->p_sysPort->name.c_str();

    if (ibSysVerbose & IBSYS_INFO) {
        printf("Info: node %s port %d connects to system port %s\n",
                nodeName, portNum, *sysPortName);
    }
    return(0);
}


/* query node name and port given front pannel port name */
int
ibSysGetNodePortOnSysPort(char *sysPortName,
        const char**nodeName, int *portNum)
{
    if (!fabric) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: fabric was not initialized.\n");
        }
        return(1);
    }

    if (!sysPortName) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No front pannel port name provided\n");
        }
        return(1);
    }

    if (!nodeName) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No node name provided\n");
        }
        return(1);
    }

    if (!portNum) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No port number provided\n");
        }
        return(1);
    }

    IBSystem *system = fabric->getSystem("SYS");
    if (!system) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find system SYS\n");
        }
        return(1);
    }

    IBSysPort *sysPort = system->getSysPort(sysPortName);
    if (!sysPort) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find system front pannel port:%s\n",
                    sysPortName);
        }
        return(1);
    }

    if (!sysPort->p_nodePort) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find system front pannel port:%s node port?\n",
                    sysPortName);
        }
        return(1);
    }

    *nodeName = sysPort->p_nodePort->p_node->name.c_str();
    *portNum = sysPort->p_nodePort->num;

    if (ibSysVerbose & IBSYS_INFO) {
        printf("Info: system port %s connects to node %s port %d\n",
                sysPortName, *nodeName, *portNum);
    }
    return(0);
}


/* query other side of the port node port */
int
ibSysGetRemoteNodePort(char *nodeName, int portNum,
        const char **remNode, int *remPortNum)
{
    if (!fabric) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: fabric was not initialized.\n");
        }
        return(1);
    }

    if (!nodeName) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No node name provided\n");
        }
        return(1);
    }

    if (!remNode) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No remote node name provided\n");
        }
        return(1);
    }

    if (!remPortNum) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: No remote port number provided\n");
        }
        return(1);
    }

    IBNode *node = fabric->getNode(nodeName);
    if (!node) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: failed to find node:%s\n", nodeName);
        }
        return(1);
    }

    IBPort *port = node->getPort(portNum);
    if (!port || !port->p_remotePort) {
        if (ibSysVerbose & IBSYS_ERROR) {
            printf("Error: no connection at node:%s port:%d\n",
                        nodeName, portNum);
        }
        *remNode = NULL;
        return(1);
    }

    *remNode = port->p_remotePort->p_node->name.c_str();
    *remPortNum = port->p_remotePort->num;
    if (ibSysVerbose & IBSYS_INFO) {
        printf("Info: node %s port %d connects to node %s port %d\n",
                nodeName, portNum, *remNode, *remPortNum );
    }
    return(0);
}

