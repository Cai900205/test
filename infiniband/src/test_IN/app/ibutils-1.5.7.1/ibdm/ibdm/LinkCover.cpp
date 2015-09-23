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
#include "SubnMgt.h"
#include "TraceRoute.h"
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
using namespace std;

/*
 * The purpose of this file is to provide an algorithm that will
 * provide back a minimal set of paths to execrsize every link on the fabric
 *
 * The implemented algorithm is based on availablity of the FDBs
 */

// Data structure: we keep the pin/dLid table as short int array.
// The first index is the port number and the second is the dLid
typedef map< IBNode *, short int*, less< IBNode * > > map_pnode_p_sint;

int getPinTargetLidTableIndex(IBFabric *p_fabric,
                              int portNum, unsigned int dLid) {
  if (dLid == 0 || dLid > p_fabric->maxLid)
  {
    cout << "-F- Got dLid which is > maxLid or 0" << endl;
    exit(1);
  }
  return ((p_fabric->maxLid)*(portNum - 1)+ dLid - 1);
}

// simply dump out the content for debug ...
void
dumpPortTargetLidTable(IBNode *p_node,
                       map_pnode_p_sint &switchInRtTbl)
{

  IBFabric *p_fabric = p_node->p_fabric;
  map_pnode_p_sint::iterator I = switchInRtTbl.find(p_node);
  if (I == switchInRtTbl.end())
  {
    cout << "-E- fail to find input routing table for"
         << p_node->name << endl;
    return;
  }

  short int *tbl = (*I).second;
  cout << "--------------- IN PORT ROUTE TABLE -------------------------"
       << endl;
  cout << "SWITCH:" << p_node->name << endl;
  cout << "LID   |";

  for (int pn = 1; pn <= p_node->numPorts; pn++)
    cout << " P" << setw(2) << pn << " |";
  cout << " FDB |" << endl;
  for (int lid = 1; lid <= p_fabric->maxLid; lid++) {
    cout << setw(5) << lid << " |";
    for (int pn = 1; pn <= p_node->numPorts; pn++) {
      int val = tbl[getPinTargetLidTableIndex(p_fabric,pn,lid)];
      if (val)
      {
        cout << " " << setw(3) << val << " |";
      }
      else
        cout << "     |";
    }
    cout << setw(3) << p_node->getLFTPortForLid(lid) << " |" << endl;
  }
}

// Trace a route from slid to dlid by LFT marking each input-port,dst-lid
// with the number of hops through
int traceRouteByLFTAndMarkInPins(
  IBFabric *p_fabric,
  IBPort *p_srcPort,
  IBPort *p_dstPort,
  unsigned int dLid,
  map_pnode_p_sint &swInPinDLidTableMap
  ) {

  IBNode *p_node;
  IBPort *p_port = p_srcPort;
  IBPort *p_remotePort = NULL;
  unsigned int sLid = p_srcPort->base_lid;
  int hopCnt = 0;

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
  {
    cout << "-V-----------------------------------------------------" << endl;
    cout << "-V- Tracing from lid:" << sLid << " to lid:"
         << dLid << endl;
  }

  // if the port is not a switch - go to the next switch:
  if (p_port->p_node->type != IB_SW_NODE)
  {
    // try the next one:
    if (!p_port->p_remotePort)
    {
      cout << "-E- Provided starting point is not connected !"
           << "lid:" << sLid << endl;
      return 1;
    }
    // we need gto store this info for marking later
    p_remotePort = p_port->p_remotePort;
    p_node = p_remotePort->p_node;
    hopCnt++;
    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
      cout << "-V- Arrived at Node:" << p_node->name
           << " Port:" << p_port->p_remotePort->num << endl;
  }
  else
  {
    // it is a switch :
    p_node = p_port->p_node;
  }

  // verify we are finally of a switch:
  if (p_node->type != IB_SW_NODE)
  {
    cout << "-E- Provided starting point is not connected to a switch !"
         << "lid:" << sLid << endl;
    return 1;
  }

  // traverse:
  int done = 0;
  while (!done) {
    if (p_remotePort)
    {
      IBNode *p_remoteNode = p_remotePort->p_node;
      if (p_remoteNode->type == IB_SW_NODE)
      {
        // mark the input port p_remotePort we got to:
        map_pnode_p_sint::iterator I =
          swInPinDLidTableMap.find(p_remoteNode);
        if (I == swInPinDLidTableMap.end())
        {
          cout << "-E- No entry for node:" << p_remoteNode->name
               << " in swInPinDLidTableMap" << endl;
          return 1;
        }
        int idx = getPinTargetLidTableIndex(p_fabric, p_remotePort->num, dLid);
        (*I).second[idx] = hopCnt;
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        {
          cout << "-I- Marked node:" << p_remoteNode->name
               << " in port:" << p_remotePort->num << " dlid:" << dLid
               << " with hops:" << hopCnt << endl;
        }
      }
    }

    // calc next node:
    int pn = p_node->getLFTPortForLid(dLid);
    if (pn == IB_LFT_UNASSIGNED)
    {
      cout << "-E- Unassigned LFT for lid:" << dLid
           << " Dead end at:" << p_node->name << endl;
      return 1;
    }

    // if the port number is 0 we must have reached the target node.
    // simply try see that p_remotePort of last step == p_dstPort
    if (pn == 0)
    {
      if (p_dstPort != p_remotePort)
      {
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
    done = (p_remotePort == p_dstPort);

    p_node = p_remotePort->p_node;
    if (hopCnt++ > 256)
    {
      cout << "-E- Aborting after 256 hops - loop in LFT?" << endl;
      return 1;
    }
  }

  return 0;
}

int
cleanupFdbForwardPortLidTables(
  IBFabric *p_fabric,
  map_pnode_p_sint &swInPinDLidTableMap,
  map_pnode_p_sint &outPortCoveredMap,
  map_pnode_p_sint &outPortUsedMap)
{
  IBNode *p_node;

  for( map_pnode_p_sint::iterator I = swInPinDLidTableMap.begin();
       I != swInPinDLidTableMap.end();
       I++) {

    short int *pinPassThroughLids = (*I).second;
    free(pinPassThroughLids);
  }

  for( map_pnode_p_sint::iterator I = outPortCoveredMap.begin();
       I != outPortCoveredMap.end();
       I++) {

    short int *outPortCovered = (*I).second;
    free(outPortCovered);
  }

  for( map_pnode_p_sint::iterator I = outPortUsedMap.begin();
       I != outPortUsedMap.end();
       I++) {

    short int *outPortUsed = (*I).second;
    free(outPortUsed);
  }

}

// Linear program does not interest me now...
#if 0
// filter some illegal chars in name
string
getPortLpName(IBPort *p_port)
{
  string res = p_port->getName();
  string::size_type s;

  while ((s = res.find('-')) != string::npos) {
    res.replace(s, 1, "_");
  }
  return (res);
}


// dump out the linear programming matrix in LP format for covering all links:
// a target line - we want to maximize the number of links excersized
// Link1 + Link2 + Link3  ....
// Per link we want only one pair...
// Link1: 0 = Link1 + Pslid-dlid + ...
// finally declare all Pslid-dlid <= 1 and all Links <= 1
int
dumpLinearProgram(IBFabric *p_fabric,
                  map_pport_src_dst_lid_pairs &switchInPortPairsMap)
{
  set< string > vars;
  int numLinks = 0;
  IBNode *p_node;
  ofstream linProgram("/tmp/ibdmchk.lp");

  // we need a doubel path - first collect all in ports and
  // dump out the target - maximize number of links covered
  for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
       nI != p_fabric->NodeByName.end();
       nI++) {
    p_node = (*nI).second;

    // go over all the input ports of the node and if has paths
    // add to the program target
    for (int pn = 1; pn <= p_node->numPorts; pn++) {
      IBPort *p_port = p_node->getPort(pn);
      if (p_port)
      {
        if (switchInPortPairsMap[p_port].size())
        {
          string varName = string("L") + getPortLpName(p_port);
          vars.insert(varName);
          if (numLinks) linProgram << "+ " ;
          if (numLinks && (numLinks % 8 == 0)) linProgram << endl;
          linProgram << varName << " ";
          numLinks++;
        }
      }
    }
  }

  linProgram << ";" << endl;

  // second pass we write down each link equation:
  for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
       nI != p_fabric->NodeByName.end();
       nI++) {
    p_node = (*nI).second;

    // go over all the input ports of the node and if has paths
    // add to the program target
    for (int pn = 1; pn <= p_node->numPorts; pn++) {
      IBPort *p_port = p_node->getPort(pn);
      if (p_port)
      {
        if (switchInPortPairsMap[p_port].size())
        {
          string varName = string("L") + getPortLpName(p_port);
          linProgram << varName;
          for (src_dst_lid_pairs::iterator lI = switchInPortPairsMap[p_port].begin();
               lI != switchInPortPairsMap[p_port].end();
               lI++) {
            char buff[128];
            sprintf(buff, "P%u_%u", (*lI).first,  (*lI).second);
            string pName = string(buff);
            vars.insert(pName);
            linProgram << " -" << pName ;
          }
          linProgram << " = 0;" << endl;
        }
      }
    }
  }

  // now dump out the int and bounds for each variable
  for (set<string>::iterator sI = vars.begin(); sI != vars.end(); sI++)
  {
    linProgram << *sI << " <= 1;" << endl;
  }

  for (set<string>::iterator sI = vars.begin(); sI != vars.end(); sI++) {
    linProgram << "int " << *sI << " ;" << endl;
  }
  linProgram.close();
}
#endif // linear program

int
initFdbForwardPortLidTables(
  IBFabric *p_fabric,
  map_pnode_p_sint &swInPinDLidTableMap,
  map_pnode_p_sint &outPortCoveredMap,
  map_pnode_p_sint &outPortUsedMap)
{
  IBNode *p_node;
  int anyError = 0;
  int numPaths = 0;

  // check the given map is empty or return error
  if (!swInPinDLidTableMap.empty())
  {
    cout << "-E- initFdbForwardPortLidTables: provided non empty map" << endl;
    return 1;
  }

  // go over all switches and allocate and initialize the "pin target lids"
  // table
  for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
       nI != p_fabric->NodeByName.end();
       nI++) {
    p_node = (*nI).second;

    short int *outPortCovered =
      (short int *)calloc(sizeof(short int), p_node->numPorts);
    if (! outPortCovered)
    {
      cout << "-E- initFdbForwardPortLidTables: fail to allocate table"
           << endl;
      return 1;
    }
    outPortCoveredMap[p_node] = outPortCovered;

    short int *outPortUsed =
      (short int *)calloc(sizeof(short int), p_node->numPorts);
    if (! outPortUsed)
    {
      cout << "-E- initFdbForwardPortLidTables: fail to allocate table"
           << endl;
      return 1;
    }
    outPortUsedMap[p_node] = outPortUsed;

    // we should not assign hops for non SW nodes:
    if (p_node->type != IB_SW_NODE) continue;

    // allocate a new table by the current fabric max lid ...
    int tableSize = p_fabric->maxLid * p_node->numPorts;
    short int *pinPassThroughLids =
      (short int *)calloc(sizeof(short int), tableSize);
    if (! pinPassThroughLids)
    {
      cout << "-E- initFdbForwardPortLidTables: fail to allocate table"
           << endl;
      return 1;
    }
    swInPinDLidTableMap[p_node] = pinPassThroughLids;
  }

  // go from all HCA to all other HCA and mark the input pin target table
  // go over all ports in the fabric
  IBPort *p_srcPort, *p_dstPort;
  unsigned int sLid, dLid;
  int hops; // dummy - but required by the LFT traversal
  int res; // store result of traversal
  for (sLid = p_fabric->minLid; sLid <= p_fabric->maxLid; sLid++) {
    IBPort *p_srcPort = p_fabric->PortByLid[sLid];

    if (!p_srcPort || (p_srcPort->p_node->type == IB_SW_NODE)) continue;

    // go over all the rest of the ports:
    for (dLid = p_fabric->minLid; dLid <= p_fabric->maxLid; dLid++ ) {
      IBPort *p_dstPort = p_fabric->PortByLid[dLid];

      // Avoid tracing to itself
      if ((dLid == sLid) || (! p_dstPort) ||
          (p_dstPort->p_node->type == IB_SW_NODE)) continue;

      numPaths++;

      // Get the path from source to destination
      res = traceRouteByLFTAndMarkInPins(p_fabric, p_srcPort, p_dstPort, dLid,
                                         swInPinDLidTableMap);
      if (res)
      {
        cout << "-E- Fail to find a path from:"
             << p_srcPort->p_node->name << "/" << p_srcPort->num
             << " to:" << p_dstPort->p_node->name << "/" << p_dstPort->num
             << endl;
        anyError++;
      }
    } // each dLid
  } // each sLid

  // Dump out what we found for each switch
  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
  {
    for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
         nI != p_fabric->NodeByName.end();
         nI++) {
      p_node = (*nI).second;

      if (p_node->type != IB_SW_NODE) continue;
      dumpPortTargetLidTable(p_node, swInPinDLidTableMap);
    }
  } // verbose

  return(anyError);
}
//////////////////////////////////////////////////////////////////////////////

typedef pair< IBNode *, short int > pair_pnode_sint;
typedef vector< pair< IBNode *, short int > > vec_pnode_sint;
struct greater_by_rank :
  public std::binary_function<const pair_pnode_sint *, const pair_pnode_sint*, bool> {
  bool operator()(pair_pnode_sint const &p1, pair_pnode_sint const &p2)
  {
    return (p1.second > p2.second);
  }
};

int
getFabricSwitchesByRank(
  IBFabric *p_fabric,
  map_pnode_int &nodesRank,
  list_pnode &sortByRankSwList)
{

  // create a vector with all node,rank pairs
  vec_pnode_sint SwitchRankVec;
  for( map_pnode_int::iterator nI = nodesRank.begin();
       nI != nodesRank.end();
       nI++)
  {
    IBNode *p_node = (*nI).first;
    if (p_node->type != IB_SW_NODE) continue;
    SwitchRankVec.push_back(*nI);
  }

  // sort it:
  sort(SwitchRankVec.begin(),SwitchRankVec.end(),  greater_by_rank());

  // build the results:
  for (int i = 0; i < SwitchRankVec.size(); i++)
  {
    sortByRankSwList.push_back(SwitchRankVec[i].first);
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
// cleanup "covered" mark for all nodes
int
cleanUpNodeMarkings(map_pnode_p_sint &switchOutPortMap)
{
  for( map_pnode_p_sint::iterator I = switchOutPortMap.begin();
       I != switchOutPortMap.end();
       I++) {

    short int *outPortUsed = (*I).second;
    for (int pIdx = 0; pIdx < ((*I).first)->numPorts; pIdx++)
    {
      outPortUsed[pIdx] = 0;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// Find all LIDs that are FWDed to the output port
int getLidsThroughPort(
  IBNode *p_node,
  int portNum,
  list< short int> &lidsThroughPort)
{
  // we currently use LFT...
  // HACK we use LFT directly as we do not know the max lid.
  for (int i = 1; i <= p_node->p_fabric->maxLid; i++)
    if (p_node->getLFTPortForLid(i) == portNum)
      lidsThroughPort.push_back(i);
  return(0);
}

//////////////////////////////////////////////////////////////////////////////
// Given a list of dLids sort them out by the sum of the hops to
// the lid and from the shorter path to local node going to that lid
typedef pair< short int, short int > pair_sint_sint;
typedef vector< pair_sint_sint > vec_sint_sint;
struct less_by_hops :
  public std::binary_function<const pair_sint_sint* , const pair_sint_sint*, bool> {
  bool operator()(pair_sint_sint const &p1, pair_sint_sint const &p2)
  {
    return (p1.second < p2.second);
  }
};

int
orderDLidsBySumOfFwdAndBwdHops(
  IBNode *p_node,
  list< short int> &lidsThroughPort,
  short int *swInPinDLidTable)
{
  // prepare the vector of all dLid and the hops sum
  vec_sint_sint dLidHopsPairs;

  for (list< short int>::iterator lI = lidsThroughPort.begin();
       lI != lidsThroughPort.end();
       lI++)
  {
    short int dLid = (*lI);
    // get the FWD Hops
    int fwdHops = p_node->getHops(NULL, dLid);

    // Add the min backward hops:
    int minBwdHops = 255;
    for (int pn = 1; pn <= p_node->numPorts; pn++)
    {
      int idx = getPinTargetLidTableIndex(p_node->p_fabric, pn, dLid);
      if (swInPinDLidTable[idx] != 0 && swInPinDLidTable[idx] < minBwdHops)
        minBwdHops = swInPinDLidTable[idx];
    }

    dLidHopsPairs.push_back(pair_sint_sint(dLid, minBwdHops + fwdHops));
  }

  sort(dLidHopsPairs.begin(),dLidHopsPairs.end(), less_by_hops());

  // rebuid the list
  lidsThroughPort.clear();
  for (int i = 0; i < dLidHopsPairs.size(); i++)
  {
    lidsThroughPort.push_back(dLidHopsPairs[i].first);
  }
  return(0);
}

//////////////////////////////////////////////////////////////////////////////
// Starting at the given node walk the LFT towards the dLid and check that none
// of the out ports is marked used
int
isFwdPathUnused(
  IBNode *p_node,
  short int dLid,
  map_pnode_p_sint &outPortUsedMap)
{
  int hops = 0;
  IBNode *pNode = p_node;
  stringstream vSt;

  while (hops < 16)
  {
    hops++;

    // get the output port
    int portNum = pNode->getLFTPortForLid(dLid);
    if (portNum == IB_LFT_UNASSIGNED)
      return(0);

    vSt << "Out on node:" << pNode->name << " port:" << portNum << endl;
    // also we want to ignore switch targets so any map to port 0
    // is ignored
    if (portNum == 0) return(0);

    IBPort *p_port = pNode->getPort(portNum);
    if (!p_port) return(0);

    // check the port is connected
    IBPort *p_remPort = p_port->p_remotePort;
    if (!p_remPort) return(0);

    // can not go there if already used!
    short int *outPortUsedVec = outPortUsedMap[pNode];
    if (outPortUsedVec[portNum - 1]) return(0);

    // HACK assume we got to the point
    if (p_remPort->p_node->type != IB_SW_NODE) return(1);

    pNode = p_remPort->p_node;
  }

  cout << "-E- Found loop on the way to:" << dLid
       << " through:" << pNode->name << endl;
  cout << vSt.str();
  return(0);
}

//////////////////////////////////////////////////////////////////////////////
// Starting at the given node walk backwards and try to find the unused path
// to a HCA with min #hops
int
isBwdPathUnused(
  IBNode *p_node,
  short int dLid,
  map_pnode_p_sint &outPortCoveredMap,
  map_pnode_p_sint &outPortUsedMap,
  map_pnode_p_sint &swInPinDLidTableMap,
  short int &sLid)
{
  // HACK: for now I assume all input paths have same hop count...
  // Simply BFS from the local port
  list_pnode nodesQueue;
  nodesQueue.push_back(p_node);

  // we do not care about hop
  while(!nodesQueue.empty())
  {
    p_node = nodesQueue.front();
    nodesQueue.pop_front();

    // go over all input ports marked as providing paths to that dlid
    // we do it twice - first only through ports not marked covered
    // then through covered ports
    for (int throughCoeverd = 0; throughCoeverd <= 1; throughCoeverd++)
    {
      for (int pn = 1; pn <= p_node->numPorts; pn++)
      {
        IBPort *p_port = p_node->getPort(pn);
        if (! p_port) continue;

        // the port should have a remote port
        IBPort *p_remPort = p_port->p_remotePort;
        if (! p_remPort) continue;

        // The remote port should not be marked used:
        short int *outPortUsedVec = outPortUsedMap[p_remPort->p_node];
        if (outPortUsedVec[p_remPort->num - 1]) continue;

        // if we limit to uncovered ports first do those.
        short int *outPortCoveredVec = outPortCoveredMap[p_remPort->p_node];
        if (!throughCoeverd)
        {
          if (outPortCoveredVec[p_remPort->num - 1]) continue;
        }
        else
        {
          if (!outPortCoveredVec[p_remPort->num - 1]) continue;
        }

        // is there a path to the dLid through that port?
        short int *swInPinDLidTable = swInPinDLidTableMap[p_node];
        int idx = getPinTargetLidTableIndex(p_node->p_fabric, pn, dLid);
        if (!swInPinDLidTable[idx]) continue;

        // if we have got to HCA we are done !
        if (p_remPort->p_node->type != IB_SW_NODE)
        {
          sLid = p_remPort->base_lid;
          return 1;
        }

        // otherwise push into next check
        nodesQueue.push_back(p_remPort->p_node);
      }
    }
  }
  return(0);
}

//////////////////////////////////////////////////////////////////////////////
// Traverse the LFT path from SRC to DST and Mark each step
int
markPathUsedAndCovered(
  IBFabric *p_fabric,
  short int sLid,
  short int dLid,
  map_pnode_p_sint &outPortUsedMap,
  map_pnode_p_sint &outPortCoveredMap)
{
  IBPort *p_port = p_fabric->getPortByLid(sLid);
  IBNode *p_node;
  IBPort *p_remPort;
  int hopCnt = 0;
  unsigned int lidStep = 1 << p_fabric->lmc;

  // make sure:
  if (! p_port) {
	 cout << "-E- Provided source:" << sLid
			<< " lid is not mapped to a port!" << endl;
	 return(1);
  }

  short int *outPortUsedVec;
  short int *outPortCoveredVec;
  short int outPortNum;

  // traverse:
  int done = 0;
  while (!done) {
    p_node = p_port->p_node;
    outPortUsedVec = outPortUsedMap[p_node];
    outPortCoveredVec = outPortCoveredMap[p_node];

    // based on node type we know how to get the remote port
    if (p_node->type == IB_SW_NODE) {
      // calc next node:
      outPortNum = p_node->getLFTPortForLid(dLid);
      if (outPortNum == IB_LFT_UNASSIGNED) {
        cout << "-E- Unassigned LFT for lid:" << dLid
             << " Dead end at:" << p_node->name << endl;
        return 1;
      }

      p_port = p_node->getPort(outPortNum);
      if (! p_port) {
        cout << "-E- Dead end for lid:" << dLid
             << " Dead end at:" << p_node->name
             << " trying port:" << outPortNum << endl;
        return 1;
      }
    }

    // mark the output port as covered:
    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
      cout << "-V- Marking covered:" << p_port->getName() << endl;

    outPortCoveredVec[p_port->num - 1] = 1;
    outPortUsedVec[p_port->num - 1] = 1;

    // get the remote port
	 if (! (p_port->p_remotePort &&
			  p_port->p_remotePort->p_node)) {
		cout << "-E- Dead end at:" << p_node->name << endl;
		return 1;
	 }

	 p_port = p_port->p_remotePort;
	 // check if we are done:
	 done = ((p_port->base_lid <= dLid) &&
				(p_port->base_lid+lidStep - 1 >= dLid));

	 if (hopCnt++ > 256) {
		cout << "-E- Aborting after 256 hops - loop in LFT?" << endl;
		return 1;
	 }
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Look for an available shortest path that would go through the given switch
// port.
// Return 0 if path found
int
findPathThroughPort(
  IBNode *p_node,
  int portNum,
  short int &foundSLid,
  short int &foundDLid,
  map_pnode_p_sint &swInPinDLidTableMap,
  map_pnode_p_sint &outPortUsedMap,
  map_pnode_p_sint &outPortCoveredMap)
{

  short int *swInPinDLidTable = swInPinDLidTableMap[p_node];

  // Obtain the list of Dst that go through this port
  list< short int> lidsThroughPort;
  getLidsThroughPort(p_node, portNum, lidsThroughPort);
  orderDLidsBySumOfFwdAndBwdHops(p_node, lidsThroughPort, swInPinDLidTable);

  for(list< short int>::iterator lI = lidsThroughPort.begin();
      lI != lidsThroughPort.end();
      lI++)
  {
    short int dLid = (*lI);
    short int sLid;
    if (isFwdPathUnused(p_node, dLid, outPortUsedMap))
    {
      // BFS backwards (sorting the min hops as we go)
      if (isBwdPathUnused(p_node, dLid,
                          outPortCoveredMap, outPortUsedMap,
                          swInPinDLidTableMap,
                          sLid))
      {
        // mark the fwd and backward paths
        markPathUsedAndCovered(p_node->p_fabric, sLid, dLid,
                               outPortUsedMap, outPortCoveredMap);
        // return the slid and dlid
        foundSLid = sLid;
        foundDLid = dLid;
        return(0);
      } else {
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
          cout << "-V- No BWD path through port:" << p_node->name
               << "/P" << portNum << " to dlid:" << dLid << endl;
      }
    } else {
      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
        cout << "-V- No FWD path through port:" << p_node->name
             << "/P" << portNum << " to dlid:" << dLid << endl;
    }
  }
  // if got here did not find any path
  return(1);
}

//////////////////////////////////////////////////////////////////////////////
typedef list< pair_sint_sint > list_pair_sint_sint;
typedef list< list_pair_sint_sint > list_list_pair_sint_sint;

int
LinkCoverageAnalysis(IBFabric *p_fabric, list_pnode rootNodes)
{
  int status = 0;

  // map switch nodes to a table of hop(in pin, dlid)
  map_pnode_p_sint swInPinDLidTableMap;
  // map switch nodes to a vector for each out port that tracks if covered
  map_pnode_p_sint outPortCoveredMap;
  // map switch nodes to a vector for each out port that tracks if used in this
  // iteration only.
  map_pnode_p_sint outPortUsedMap;
  cout << "-I- Generating non blocking full link coverage plan into:"
       << "/tmp/ibdmchk.non_block_all_links" << endl;
  ofstream linkProgram("/tmp/ibdmchk.non_block_all_links");

  // initialize the data structures
  if (initFdbForwardPortLidTables(
    p_fabric,
    swInPinDLidTableMap,
    outPortCoveredMap,
    outPortUsedMap)) {
    return(1);
  }

  // get an ordered list of the switched by their rank
  map_pnode_int nodesRank;
  if (SubnRankFabricNodesByRootNodes(p_fabric, rootNodes, nodesRank))
    return(1);

  list_pnode sortedSwByRankList;
  getFabricSwitchesByRank(p_fabric, nodesRank, sortedSwByRankList);

  // init stepsList to an empty list
  list_list_pair_sint_sint linkCoverStages;

  int failToFindAnyUnUsedPath = 0;
  int somePortsUncovered = 0;
  int stage = 0;
  linkProgram << "# STAGE FROM-LID TO-LID" << endl;
  // while not stuck with no ability to make progress
  while (failToFindAnyUnUsedPath == 0)
  {
    somePortsUncovered = 0;
    stage++;
    linkProgram << "#------------------------------" << endl;
    // Clear all UsedPort markers
    cleanUpNodeMarkings(outPortUsedMap);

    // init thisStepList
    list_pair_sint_sint srcDstPairs;

    // we should know we did not add even one pair...
    failToFindAnyUnUsedPath = 1;

    // go over all nodes in the sorted list
    for (list_pnode::iterator lI = sortedSwByRankList.begin();
         lI != sortedSwByRankList.end();
         lI++)
    {
      IBNode *p_node = (*lI);

      // get the usage and coverage tables of this switch:
      short int *portUsed = outPortUsedMap[p_node];
      short int *portCovered = outPortCoveredMap[p_node];

      for (int pn = 1; pn <= p_node->numPorts; pn++)
      {
        // skip floating or non existing ports:
        IBPort *p_port = p_node->getPort(pn);
        if (!p_port || ! p_port->p_remotePort) continue;

        // if port is marked as covered - ignore it
        if (portCovered[pn - 1]) continue;

        short int dLid, sLid;

        // find a path (src,dst lid pair) going through that port that is unused
        if (findPathThroughPort(p_node,pn,sLid,dLid,swInPinDLidTableMap,
                                outPortUsedMap, outPortCoveredMap))
        {
          somePortsUncovered = 1;
          if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-W- Fail to cover path on:" << p_node->name
                 << "/P" << pn << endl;
        }
        else
        {
          // add the pair to thisStepList
          srcDstPairs.push_back(pair<short int, short int>(sLid,dLid));
          linkProgram << setw(3) << stage << " "
                      << setw(4) << sLid << " "
                      << setw(4) << dLid << endl;
          failToFindAnyUnUsedPath = 0;
        }
      }
    }
    // add thisStepList o stepList
    linkCoverStages.push_back(srcDstPairs);
  }

  if (somePortsUncovered)
  {
    cout << "-E- After " << stage << " stages some switch ports are still not covered: " << endl;
    status = 1;
  }
  else
  {
    cout << "-I- Covered all links in " << stage - 1 << " stages" << endl;
  }

  // scan for all uncovered out ports:
  // go over all nodes and require out port that is connected to
  // be covered
  for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
       nI != p_fabric->NodeByName.end();
       nI++) {
    IBNode *p_node = (*nI).second;

    short int *outPortCovered = outPortCoveredMap[p_node];

    for (int pn = 1; pn <= p_node->numPorts; pn++)
    {
      IBPort *p_port = p_node->getPort(pn);
      if (p_port && p_port->p_remotePort)
        if (! outPortCovered[pn - 1])
          cout << "-W- Fail to cover port:" << p_port->getName() << endl;
    }
  }

  linkProgram.close();

  // cleanup
  cleanupFdbForwardPortLidTables(
    p_fabric,
    swInPinDLidTableMap,
    outPortCoveredMap,
    outPortUsedMap);

  return(status);
}

