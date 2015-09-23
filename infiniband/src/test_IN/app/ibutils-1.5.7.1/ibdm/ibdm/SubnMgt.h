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

#ifndef IBDM_SUBN_MGT_H
#define IBDM_SUBN_MGT_H


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


// Assign lids
int
SubnMgtAssignLids (IBPort *p_smNodePort, unsigned int lmc = 0);

// Calculate the minhop table for the switches
int
SubnMgtCalcMinHopTables (IBFabric *p_fabric);

// Fill in the FDB tables in an OpesnSM style routing
// which is switch based, uses number of routes per port
// profiling and treat LMC assigned lids sequentialy
// Rely on running the SubnMgtCalcMinHopTables beforehand
int
SubnMgtOsmRoute(IBFabric *p_fabric);

// Fill in the FDB tables in an OpesnSM style routing
// which is switch based, uses number of routes per port
// profiling and treat LMC assigned lids sequentialy.
// Also it will favor runing through a new system or node
// on top of the port profile.
// Rely on running the SubnMgtCalcMinHopTables beforehand
int
SubnMgtOsmEnhancedRoute(IBFabric *p_fabric);

// Perform Fat Tree specific routing by assigning a single LID to
// each root node port a single LID to route through.
int
SubnMgtFatTreeRoute(IBFabric *p_fabric);

// Verify All CA to CA connectivity
int
SubnMgtVerifyAllCaToCaRoutes(IBFabric *p_fabric);

// Verify all point to point connectivity
int
SubnMgtVerifyAllRoutes(IBFabric *p_fabric);

// Calc Up/Down min hop tables using the given ranking per node
int
SubnMgtCalcUpDnMinHopTbls(IBFabric *p_fabric , map_pnode_int &nodesRank);

//Calc by Up-Down Min Hop Algorithm the Switch Tables
int
SubnMgtCalcUpDnMinHopTblsByRootNodesRex(IBFabric *p_fabric , const char * rootNodesNameRex);

// Analyze the fabric to find its root nodes assuming it is
// a pure tree (keeping all levels in place).
list_pnode
SubnMgtFindTreeRootNodes(IBFabric *p_fabric);

// Analyze the fabric to find its root nodes using statistical methods
// on the profiles of min hops to CAs
list_pnode
SubnMgtFindRootNodesByMinHop(IBFabric *p_fabric);

// Given a list of root nodes mark them with a zero rank
// Then BFS and rank min
// note we use the provided map of IBNode* to int for storing the rank
int
SubnRankFabricNodesByRootNodes(
  IBFabric *p_fabric, list_pnode rootNodes, map_pnode_int &nodesRank);

// Find any routes that exist in the FDB's from CA to CA and do not adhare to
// the up/down rules. Report any crossing of the path.
int
SubnReportNonUpDownCa2CaPaths(IBFabric *p_fabric, map_pnode_int &nodesRank);

// Check all multicast groups :
// 1. all switches holding it are connected
// 2. No loops (i.e. a single BFS with no returns).
int SubnMgtCheckFabricMCGrps(IBFabric *p_fabric);

// Check all multicast groups do not have credit loop potential
int
SubnMgtCheckFabricMCGrpsForCreditLoopPotential(
  IBFabric *p_fabric, map_pnode_int &nodesRank);

// Provide sets of port pairs to run BW check from in a way that is
// full bandwidth. Reide in LinkCover.cpp
int
LinkCoverageAnalysis(IBFabric *p_fabric, list_pnode rootNodes);

// Perform FatTree analysis
int
FatTreeAnalysis(IBFabric *p_fabric);

// Perform FatTree optimal permutation routing
int
FatTreeRouteByPermutation(IBFabric* p_fabric, const char* srcs, const char* dsts);

// Report all CA 2 CA Paths through a port
int
SubnReportCA2CAPathsThroughSWPort(IBPort *p_port);

#endif /* IBDM_SUBN_MGT_H */

