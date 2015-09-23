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

FatTree Utilities:

*/

#include <set>
#include <algorithm>
#include <iomanip>
#include "Fabric.h"
#include "SubnMgt.h"
#include "RouteSys.h"

//////////////////////////////////////////////////////////////////////////////
// Build a Fat Tree data structure for the given topology.
// Prerequisites: Ranking performed and stored at p_node->rank.
/// Ranking is such that roots are marked with rank=0 and leaf switches with
// highest value.
//
// The algorithm BFS from an arbitrary leaf switch.
// It then allocates ID tupples to each switch ID[0..N].
// Only one digit of the ID is allowed to change when going from
// switch node to the other.
//
// We use the term "index" when we refer to
// The digit indexed 0 ID[0] in the tupple is the rank number.
// Going down the tree the digit that might change is D[from->rank+1]
// Going up the tee the digit that might change is D[from->rank]
//
// During the BFS each node is assigned a tupple the first time it is
// visited. During the BFS we also collect the list of ports that connect to
// each value of the changing D.
//
// For a tree to be routable by the following algorithm it must be symmetrical
// in the sense that each node at the same rank must be connected to exact same
// number of sub hierarchy indexes with the exact same number of ports
//

// for comparing tupples
struct FatTreeTuppleLess : public binary_function <vec_byte, vec_byte, bool> {
  bool operator()(const vec_byte& x, const vec_byte& y) const {
    if (x.size() > y.size()) return false;
    if (y.size() > x.size()) return true;

    for (unsigned int i = 0 ; i < x.size() ; i++)
      {
	if (x[i] > y[i]) return false;
	if (x[i] < y[i]) return true;
      }
    return false;
  }
};

typedef map< IBNode *, vec_byte, less< IBNode *> > map_pnode_vec_byte;
typedef vector< list< int > > vec_list_int;

class FatTreeNode {
  IBNode *p_node;          // points to the fabric node for this node
  vec_list_int childPorts; // port nums connected to child by changing digit
  vec_list_int parentPorts;// port nums connected to parent by changing digit
public:
  FatTreeNode(IBNode *p_node);
  FatTreeNode(){p_node = NULL;};
  int numParents();
  int numChildren();
  int numParentGroups();
  int numChildGroups();
  bool goingDown(int lid);
  friend class FatTree;
};

FatTreeNode::FatTreeNode(IBNode *p_n)
{
  p_node = p_n;
  list< int > emptyList;
  for (unsigned int pn = 0; pn <= p_node->numPorts; pn++)
    {
      childPorts.push_back(emptyList);
      parentPorts.push_back(emptyList);
    }
}

// get the total number of children a switch have
int
FatTreeNode::numChildren()
{
  int s = 0;
  for (int i = 0; i < childPorts.size(); i++)
    s += childPorts[i].size();
  return s;
}

// get the total number of children a switch have
int
FatTreeNode::numParents()
{
  int s = 0;
  for (int i = 0; i < parentPorts.size(); i++)
    s += parentPorts[i].size();
  return s;
}

// get the total number of children groups
int
FatTreeNode::numChildGroups()
{
  int s = 0;
  for (int i = 0; i < childPorts.size(); i++)
    if (childPorts[i].size()) s++;
  return s;
}

int
FatTreeNode::numParentGroups()
{
  int s = 0;
  for (int i = 0; i < parentPorts.size(); i++)
    if (parentPorts[i].size()) s++;
  return s;
}

// Check whether there is downwards path towards the given lid
bool FatTreeNode::goingDown(int lid)
{
  int portNum = p_node->getLFTPortForLid(lid);
  if (portNum == IB_LFT_UNASSIGNED)
    return false;
  for (int i=0; i<childPorts.size(); i++)
    for (list<int>::iterator lI = childPorts[i].begin();lI != childPorts[i].end(); lI++) {
      if (portNum == *lI)
	return true;
    }
  return false;
}

typedef map< vec_byte, class FatTreeNode, FatTreeTuppleLess > map_tupple_ftnode;

class FatTree {
  // the node tupple is built out of the following:
  // d[0] = rank
  // d[1..N-1] = ID digits
  IBFabric          *p_fabric;     // The fabric we attach to
  map_pnode_vec_byte TuppleByNode;
  map_tupple_ftnode  NodeByTupple;
  vec_int            LidByIdx;     // store target HCA lid by its index
  unsigned int       N;            // number of levels in the fabric
  map_str_int        IdxByName;

  // obtain the Fat Tree node for a given IBNode
  FatTreeNode* getFatTreeNodeByNode(IBNode *p_node);

  // get the first lowest level switch while making sure all HCAs
  // are connected to same rank
  // return NULL if this check is not met or no ranking available
  IBNode *getLowestLevelSwitchNode();

  // get a free tupple given the reference one and the index to change:
  vec_byte getFreeTupple(vec_byte refTupple, unsigned int changeIdx);

  // convert tupple to string
  string getTuppleStr(vec_byte tupple);

  // simply dump out the FatTree data:
  void dump();

  // track a connection to remote switch
  int trackConnection(
		      FatTreeNode *p_ftNode,
		      vec_byte     tupple,   // the connected node tupple
		      unsigned int rank,     // rank of the local node
		      unsigned int remRank,  // rank of the remote node
		      unsigned int portNum,  // the port number connecting to the remote node
		      unsigned int remDigit  // the digit which changed on the remote node
		      );

  // set of coefficients that represent the structure
  int maxHcasPerLeafSwitch;
  vec_int childrenPerRank; // not valid for leafs
  vec_int parentsPerRank;
  vec_int numSwInRank;     // number of switches for that level
  vec_int downByRank;      // number of remote child switches s at rank
  vec_int upByRank;        // number of remote parent switches at rank

  // extract fat tree coefficients and update validity flag
  // return 0 if OK
  int extractCoefficients();

public:
  // construct the fat tree by matching the topology to it.
  // note that this might return an invalid tree for routing
  // as indicated by isValid flag
  FatTree(IBFabric *p_fabric);

  // true if the fabric can be mapped to a fat tree
  bool isValid;

  // propagate FDB assignments going up the tree ignoring the out port
  int assignLftUpWards(FatTreeNode *p_ftNode, uint16_t dLid, int outPortNum, int switchPathOnly);

  // set FDB values as given in the input
  int forceLftUpWards(FatTreeNode *p_ftNode, uint16_t dLid, vec_int ports);

  // propagate FDB assignments going down the tree
  int assignLftDownWards(FatTreeNode *p_ftNode, uint16_t dLid, int outPortNum, int switchPathOnly, int downOnly);

  // route the fat tree
  int route();

  // route requested permutation in the fat tree
  int permRoute(vector<string> src, vector<string> dst);

  // create the file ftree.hcas with the list of HCA port names
  // and LIDs in the correct order
  void dumpHcaOrder();
};

FatTreeNode* FatTree::getFatTreeNodeByNode(IBNode *p_node) {
  FatTreeNode* p_ftNode;
  vec_byte tupple(N, 0);
  tupple = TuppleByNode[p_node];
  p_ftNode = &NodeByTupple[tupple];
  return p_ftNode;
}

// get the first lowest level switch while making sure all HCAs
// are connected to same rank
// return NULL if this check is not met or no ranking available
IBNode *FatTree::getLowestLevelSwitchNode()
{
  unsigned int leafRank = 0;
  IBNode *p_leafSwitch = NULL;
  IBPort *p_port;

  // go over all HCAs and track the rank of the node connected to them
  for( map_str_pnode::iterator nI = p_fabric->NodeByName.begin();
       nI != p_fabric->NodeByName.end();
       nI++)
    {
      IBNode *p_node = (*nI).second;
      if (p_node->type != IB_CA_NODE) continue;

      for (unsigned int pn = 1; pn <= p_node->numPorts; pn++)
	{
	  p_port = p_node->getPort(pn);
	  if (p_port && p_port->p_remotePort)
	    {
	      IBNode *p_remNode = p_port->p_remotePort->p_node;

	      if (p_remNode->type != IB_SW_NODE) continue;

	      // is the remote node ranked?
	      if (!p_remNode->rank)  continue;

	      // must be identical for all leaf switches:
	      if (!leafRank)
		{
		  leafRank = p_remNode->rank;
		  p_leafSwitch = p_remNode;
		}
	      else
		{
		  // get the lowest name
		  if (p_remNode->name < p_leafSwitch->name )
		    p_leafSwitch = p_remNode;

		  if (p_remNode->rank != leafRank)
		    {
		      cout << "-E- Given topology is not a fat tree. HCA:"
			   << p_remNode->name
			   << " found not on lowest level!" << endl;
		      return(NULL);
		    }
		}
	    }
	}
    }
  return(p_leafSwitch);
}

// get a free tupple given the reference one and the index to change:
// also track the max digit allocated per index
vec_byte FatTree::getFreeTupple(vec_byte refTupple, unsigned int changeIdx)
{
  vec_byte res = refTupple;
  int rank = changeIdx - 1;
  for (uint8_t i = 0; i < 255; i++)
    {
      res[changeIdx] = i;
      map_tupple_ftnode::const_iterator tI = NodeByTupple.find(res);
      if (tI == NodeByTupple.end())
	return res;
    }
  cout << "ABORT: fail to get free tupple! (in 255 indexies)" << endl;
  abort();
}

string FatTree::getTuppleStr(vec_byte tupple)
{
  char buf[128];
  buf[0] = '\0';
  for (unsigned int i = 0; i < tupple.size(); i++)
    {
      if (i) strcat(buf,".");
      sprintf(buf, "%s%d", buf, tupple[i]);
    }
  return(string(buf));
}

// track connection going up or down by registering the port in the
// correct fat tree node childPorts and parentPorts
int FatTree::trackConnection(
			     FatTreeNode *p_ftNode, // the connected node
			     vec_byte     tupple,   // the connected node tupple
			     unsigned int rank,     // rank of the local node
			     unsigned int remRank,  // rank of the remote node
			     unsigned int portNum,  // the port number connecting to the remote node
			     unsigned int remDigit  // the digit of the tupple changing to the remote node
			     )
{
  if ( rank < remRank )
    {
      // going down
      // make sure we have enough entries in the vector
      if (remDigit >= p_ftNode->childPorts.size())
	{
	  list<  int > emptyPortList;
	  for (unsigned int i = p_ftNode->childPorts.size();
	       i <= remDigit; i++)
            p_ftNode->childPorts.push_back(emptyPortList);
	}
      p_ftNode->childPorts[remDigit].push_back(portNum);
    }
  else
    {
      // going up
      // make sure we have enough entries in the vector
      if (remDigit >= p_ftNode->parentPorts.size())
	{
	  list< int > emptyPortList;
	  for (unsigned int i = p_ftNode->parentPorts.size();
	       i <= remDigit; i++)
            p_ftNode->parentPorts.push_back(emptyPortList);
	}
      p_ftNode->parentPorts[remDigit].push_back(portNum);
    }

  return(0);
}

// Extract fat tree coefficiants and double check its
// symmetry
int
FatTree::extractCoefficients()
{
  // Go over all levels of the tree.
  // Collect number of nodes per each level
  // Require the number of children is equal
  // Require the number of parents is equal

  int prevLevel = -1;
  int anyErr = 0;

  // go over all nodes
  for (map_tupple_ftnode::iterator tI = NodeByTupple.begin();
       tI != NodeByTupple.end();
       tI++)
    {
      FatTreeNode *p_ftNode = &((*tI).second);
      int level = (*tI).first[0];
      bool isFirstInLevel;

      isFirstInLevel = (level != prevLevel);
      prevLevel = level;

      if (isFirstInLevel)
	{
	  numSwInRank.push_back(1);
	  parentsPerRank.push_back(p_ftNode->numParents());
	  childrenPerRank.push_back(p_ftNode->numChildren());
	  downByRank.push_back(p_ftNode->numChildGroups());
	  upByRank.push_back(p_ftNode->numParentGroups());
	}
      else
	{
	  numSwInRank[level]++;
	  if (parentsPerRank[level] != p_ftNode->numParents())
	    {
	      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
		cout << "-E- node:" << p_ftNode->p_node->name
		     << " has unequal number of parent ports to its level"
		     << endl;
	      anyErr++;
	    }

	  // we do not require symmetrical routing for leafs
	  if (level < N-1)
	    {
	      if (childrenPerRank[level] != p_ftNode->numChildren())
		{
		  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
		    cout << "-E- node:" << p_ftNode->p_node->name <<
		      " has unequal number of child ports to its level" << endl;
		  anyErr++;
		}
	    }
	}
    }

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    {
      for (int rank = 0; rank < numSwInRank.size(); rank++) {
	cout << "-I- rank:" << rank
	     << " switches:" << numSwInRank[rank]
	     << " parents: " << parentsPerRank[rank]
	     << " (" << upByRank[rank] << " groups)"
	     << " children:" << childrenPerRank[rank]
	     << " (" << downByRank[rank] << " groups)"
	     << endl;
      }
    }

  if (anyErr) return 1;

  vec_byte firstLeafTupple(N, 0);
  firstLeafTupple[0] = N-1;
  maxHcasPerLeafSwitch = 0;
  for (map_tupple_ftnode::iterator tI = NodeByTupple.find(firstLeafTupple);
       tI != NodeByTupple.end();
       tI++)
    {
      FatTreeNode *p_ftNode = &((*tI).second);
      IBNode *p_node = p_ftNode->p_node;
      int numHcaPorts = 0;
      for (unsigned int pn = 1; pn <= p_node->numPorts; pn++)
	{
	  IBPort *p_port = p_node->getPort(pn);
	  if (p_port && p_port->p_remotePort &&
	      (p_port->p_remotePort->p_node->type == IB_CA_NODE))
	    {
	      numHcaPorts++;
	    }

	}
      if (numHcaPorts > maxHcasPerLeafSwitch)
	maxHcasPerLeafSwitch = numHcaPorts;
    }

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-I- HCAs per leaf switch set to:"
	 << maxHcasPerLeafSwitch << endl;

  cout << "-I- Topology is a valid Fat Tree" << endl;
  isValid = 1;

  return 0;
}

// construct the fat tree by matching the topology to it.
FatTree::FatTree(IBFabric *p_f)
{
  isValid = 0;
  p_fabric = p_f;

  IBNode *p_node = getLowestLevelSwitchNode();
  IBPort *p_port;
  FatTreeNode *p_ftNode;

  if (! p_node) return;
  N = p_node->rank + 1; // N = number of levels (our first rank is 0 ...)

  // BFS from the first switch connected to HCA found on the fabric
  list< IBNode * > bfsQueue;
  bfsQueue.push_back(p_node);

  // also we always allocate the address 0..0 with "rank" digits to the node:
  vec_byte tupple(N, 0);

  // adjust the level:
  tupple[0] = p_node->rank;
  TuppleByNode[p_node] = tupple;
  NodeByTupple[tupple] = FatTreeNode(p_node);
  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-I- Assigning tupple:" << getTuppleStr(tupple) << " to:"
	 << p_node->name << endl;

  while (! bfsQueue.empty())
    {
      p_node = bfsQueue.front();
      bfsQueue.pop_front();
      // we must have a tupple stored - get it
      tupple = TuppleByNode[p_node];
      // we also need to get the fat tree node...
      p_ftNode = &NodeByTupple[tupple];

      // go over all the node ports
      for (unsigned int pn = 1; pn <= p_node->numPorts; pn++)
	{
	  p_port = p_node->getPort(pn);
	  if (!p_port || !p_port->p_remotePort) continue;

	  IBNode *p_remNode = p_port->p_remotePort->p_node;

	  if (p_remNode->type != IB_SW_NODE)
	    {
	      // for HCAs we only track the conenctions
	      list< int > tmpList;
	      tmpList.push_back(pn);
	      p_ftNode->childPorts.push_back(tmpList);
	      continue;
	    }

	  // now try to see if this node has already a map:
	  map_pnode_vec_byte::iterator tI = TuppleByNode.find(p_remNode);

	  // we are allowed to change the digit based on the direction we go:
	  unsigned int changingDigitIdx;
	  if (p_node->rank < p_remNode->rank)
            // going down the tree = use the current rank + 1
            // (save one for level)
            changingDigitIdx = p_node->rank + 1;
	  else if (p_node->rank > p_remNode->rank)
            // goin up the tree = use current rank (first one is level)
            changingDigitIdx = p_node->rank;
	  else
	    {
	      cout << "-E- Connections on the same rank level "
		   << " are not allowed in Fat Tree routing." << endl;
	      cout << "    from:" << p_node->name << "/P" << pn
		   << " to:" << p_remNode->name << endl;
	      return;
	    }

	  // do we need to allocate a new tupple?
	  if (tI == TuppleByNode.end())
	    {

	      // the node is new - so get a new tupple for it:
	      vec_byte newTupple = tupple;
	      // change the level accordingly
	      newTupple[0] = p_remNode->rank;
	      // obtain a free one
	      newTupple = getFreeTupple(newTupple, changingDigitIdx);

	      // assign the new tupple and add to next steps:
	      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
		cout << "-I- Assigning tupple:" << getTuppleStr(newTupple)
		     << " to:" << p_remNode->name << " changed idx:"
		     << changingDigitIdx << " from:" << getTuppleStr(tupple)
		     << endl;

	      TuppleByNode[p_remNode] = newTupple;
	      NodeByTupple[newTupple] = FatTreeNode(p_remNode);

	      unsigned int digit = newTupple[changingDigitIdx];

	      // track the connection
	      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
		cout << "-I- Connecting:" << p_node->name << " to:"
		     << p_remNode->name << " through port:" << pn
		     << " remDigit:" << digit << endl;
	      if (trackConnection(
				  p_ftNode, tupple, p_node->rank, p_remNode->rank, pn, digit))
		return;

	      bfsQueue.push_back(p_remNode);
	    }
	  else
	    {
	      // other side already has a tupple - so just track the connection
	      vec_byte remTupple = (*tI).second;
	      vec_byte mergedTupple = remTupple;

	      unsigned int digit = remTupple[changingDigitIdx];

	      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
		cout << "-I- Connecting:" << p_node->name  << " to:"
		     << p_remNode->name << " through port:" << pn
		     << " remDigit:" << digit  << endl;
	      if (trackConnection(
				  p_ftNode, tupple, p_node->rank, p_remNode->rank, pn, digit))
		return;
	    }

	} // all ports
    } // anything to do

  // make sure the extracted tropology can be declared "fat tree"
  if (extractCoefficients()) return;

  // build mapping between HCA index and LIDs.
  // We need to decide what will be the K of the lowest switches level.
  // It is possible that for all of them the number of HCAs is < num
  // left ports thus we should probably use the lowest number of all
  vec_byte firstLeafTupple(N, 0);
  firstLeafTupple[0] = N-1;

  // now restart going over all leaf switches by their tupple order and
  // allocate mapping
  int hcaIdx = 0;
  for (map_tupple_ftnode::iterator tI = NodeByTupple.find(firstLeafTupple);
       tI != NodeByTupple.end();
       tI++)
    {
      // we collect HCAs connected to the leaf switch and set their childPort
      // starting at the index associated with the switch tupple.
      FatTreeNode *p_ftNode = &((*tI).second);
      IBNode *p_node = p_ftNode->p_node;
      unsigned int pIdx = 0;
      for (unsigned int pn = 1; pn <= p_node->numPorts; pn++)
	{
	  IBPort *p_port = p_node->getPort(pn);
	  if (p_port && p_port->p_remotePort &&
	      (p_port->p_remotePort->p_node->type == IB_CA_NODE))
	    {
	      LidByIdx.push_back(p_port->p_remotePort->base_lid);
	      IdxByName[p_port->p_remotePort->p_node->name] = hcaIdx;
	      pIdx++;
	      hcaIdx++;
	    }
	}
      // we might need some padding
      for (; pIdx < maxHcasPerLeafSwitch; pIdx++) {
	LidByIdx.push_back(0);
	hcaIdx++;
      }
    }

  cout << "-I- Fat Tree Created" << endl;

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    dump();
}

//////////////////////////////////////////////////////////////////////////////
// Route a the Fat Tree
// Prerequisites: Fat Tree structure was built.
//
// Algorithm:
// For each leaf switch (in order)
//   For each HCA index (even if it does not have a LID - invent one)
//     Traverse up the tree selecting "first" lowest utilized port going down
//     Mark utilization on that port
//     Perform backward traversal marking up ports to that remote node
//
// Data Model:
// We use the fat tree to get ordering.
// "main" routing is the routing from HCA to HCA.
// "side" routing is used from all SW to all HCAs (and dynamic routing)
// Track port utilization for the "main" routing by the "counter1"
// Track port utilzation of the "side" routing in "counter2" field of the port
//
//////////////////////////////////////////////////////////////////////////////

int
FatTree::assignLftUpWards(FatTreeNode *p_ftNode, uint16_t dLid,
                          int outPortNum, int switchPathOnly)
{
  IBPort* p_port;
  IBNode *p_node = p_ftNode->p_node;

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-V- assignLftUpWards invoked on node:" << p_node->name
	 << " out-port:" << outPortNum
	 << " to dlid:" << dLid
	 << " switchPathOnly:" << switchPathOnly
	 << endl;

  // Foreach one of the child port groups select the port which is
  // less utilized and set its LFT - then recurse into it
  // go over all child ports
  for (int i = 0; i < p_ftNode->childPorts.size(); i++) {
    if (!p_ftNode->childPorts[i].size()) continue;

    // we can skip handling the remote node if
    // it already has an assigned LFT for this target lid
    int firstPortNum = p_ftNode->childPorts[i].front();
    IBPort *p_firstPort = p_node->getPort(firstPortNum);
    IBNode *p_remNode = p_firstPort->p_remotePort->p_node;
    if (p_remNode->getLFTPortForLid(dLid) != IB_LFT_UNASSIGNED)
      {
	if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	  cout << "-V- assignLftUpWards skip already assigned remote node:"
	       << p_remNode->name
	       << " switchPathOnly:" << switchPathOnly
	       << endl;
	continue;
      }

    int bestUsage = 0;
    IBPort *p_bestPort = NULL;
    int found = 0;

    // we only need one best port on each group
    for (list<int>::iterator lI = p_ftNode->childPorts[i].begin();
	 !found && (lI != p_ftNode->childPorts[i].end());
	 lI++) {

      // can not have more then one port in group...
      int portNum = *lI;

      // we do not want to descend back to the original port
      if (portNum == outPortNum)
	{
	  p_bestPort = NULL;
	  found = 1;
	  continue;
	}

      IBPort *p_port = p_node->getPort(portNum);
      // not required but what the hack...
      if (!p_port || !p_port->p_remotePort) continue;
      IBPort *p_remPort = p_port->p_remotePort;

      // ignore remote HCA nodes
      if (p_remPort->p_node->type != IB_SW_NODE) continue;

      // look on the local usage as we mark usage entering a port
      int usage = p_port->counter1;
      if (switchPathOnly)
	usage += p_port->counter2;
      if ((p_bestPort == NULL) || (usage < bestUsage))
	{
	  p_bestPort = p_port;
	  bestUsage = usage;
	}
    }

    if (p_bestPort != NULL)
      {
	// mark utilization
	if (switchPathOnly)
	  p_bestPort->counter2++;
	else
	  p_bestPort->counter1++;

	IBPort *p_bestRemPort = p_bestPort->p_remotePort;
	p_remNode->setLFTPortForLid(dLid, p_bestRemPort->num);

	if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	  cout << "-V- assignLftUpWards setting lft on:" << p_remNode->name
	       << " to port:" << p_bestRemPort->num
	       << " to dlid:" << dLid  << endl;

	FatTreeNode *p_remFTNode =
	  getFatTreeNodeByNode(p_bestRemPort->p_node);
	assignLftUpWards(p_remFTNode, dLid, p_bestRemPort->num,
			 switchPathOnly);
      }
  }

  return(0);
}

// to allocate a port downwards we look at all ports
// going up from this node and select the one which is
// less used
// we also start an upwards assignment to this node
int
FatTree::assignLftDownWards(FatTreeNode *p_ftNode, uint16_t dLid,
                            int outPortNum, int switchPathOnly, int downOnly)
{
  IBPort *p_port;
  IBNode *p_node = p_ftNode->p_node;

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-V- assignLftDownWards from:" << p_node->name
	 << " dlid:" << dLid
	 << " through port:" << outPortNum
	 << " switchPathOnly:" << switchPathOnly
	 << endl;

  if (outPortNum != 0xFF)
    {
      // Set FDB to that LID only if not preset or we are on "main" route
      if (!switchPathOnly || (p_node->getLFTPortForLid(dLid) == IB_LFT_UNASSIGNED)) {
	p_node->setLFTPortForLid(dLid, outPortNum);

	p_port = p_node->getPort(outPortNum);

	// mark the usage of this port
	if (p_port) {
	  if (switchPathOnly) {
	    p_port->counter2++;
	  } else {
	    p_port->counter1++;
	  }
	}
      }
    }

  // find the remote port (following the parents list order)
  // that is not used or less used.
  int bestUsage = 0;
  int bestGroup = -1;
  IBPort *p_bestRemPort = NULL;
  int found = 0;
  // go over all child ports
  for (int i = 0; !found && (i < p_ftNode->parentPorts.size()); i++) {
    if (!p_ftNode->parentPorts[i].size()) continue;

    for (list<int>::iterator lI = p_ftNode->parentPorts[i].begin();
	 !found && (lI != p_ftNode->parentPorts[i].end());
	 lI++) {

      // can not have more then one port in group...
      int portNum = *lI;
      IBPort *p_port = p_node->getPort(portNum); // must be if marked parent
      IBPort *p_remPort = p_port->p_remotePort;
      if (p_remPort == NULL) continue;
      int usage = p_remPort->counter1;
      if (switchPathOnly)
	usage += p_remPort->counter2;

      if ((p_bestRemPort == NULL) || (usage < bestUsage))
	{
	  p_bestRemPort = p_remPort;
	  bestUsage = usage;
	  bestGroup = i;
	  // can not have better usage then no usage
	  if (usage == 0)
	    found = 1;
	}
    }
  }

  FatTreeNode *p_remFTNode;
  // first visit the official path!
  if (bestGroup != -1) {
    p_remFTNode = getFatTreeNodeByNode(p_bestRemPort->p_node);
    if (!p_remFTNode)
      cout << "-E- Fail to get FatTree Node for node:"
	   << p_bestRemPort->p_node->name << endl;
    else
      assignLftDownWards(p_remFTNode, dLid, p_bestRemPort->num,switchPathOnly,downOnly);
  }

  // need to go all up all the possible ways to make sure all switch are
  // connected to all HCAs
  for (int i = 0; i < p_ftNode->parentPorts.size(); i++) {
    if (!p_ftNode->parentPorts[i].size()) continue;
    IBPort* p_remPort;
    // if we are on the "best group" we know the best port
    if (bestGroup == i) continue;

    // find the best port of the group i
    p_bestRemPort = NULL;
    found = 0;
    for (list<int>::iterator lI = p_ftNode->parentPorts[i].begin();!found && (lI != p_ftNode->parentPorts[i].end()); lI++) {
      // can not have more then one port in group...
      int portNum = *lI;
      IBPort *p_port = p_node->getPort(portNum); // must be if marked parent
      IBPort *p_remPort = p_port->p_remotePort;
      if (p_remPort == NULL) continue;
      int usage = p_remPort->counter1 + p_remPort->counter2;

      if ((p_bestRemPort == NULL) || (usage < bestUsage)) {
	p_bestRemPort = p_remPort;
	bestUsage = usage;
	// can not have better usage then no usage
	if (usage == 0)
	  found = 1;
      }
    }
    p_remFTNode = getFatTreeNodeByNode(p_bestRemPort->p_node);
    if (!p_remFTNode)
      cout << "-E- Fail to get FatTree Node for node:"
	   << p_bestRemPort->p_node->name << endl;
    else
      assignLftDownWards(p_remFTNode, dLid, p_bestRemPort->num, 1, downOnly);
  }

  // Perform Backward traversal through all ports connected to lower
  // level switches in-port = out-port
  if (!downOnly)
    assignLftUpWards(p_ftNode, dLid, outPortNum, switchPathOnly);

  return(0);
}

// perform the routing by filling in the fabric LFTs
int FatTree::route()
{
  int hcaIdx = 0;
  int lid; // the target LID we propagate for this time

  // go over all fat tree nodes of the lowest level
  vec_byte firstLeafTupple(N, 0);
  firstLeafTupple[0] = N-1;
  for (map_tupple_ftnode::iterator tI = NodeByTupple.find(firstLeafTupple);
       tI != NodeByTupple.end();
       tI++)
    {

      FatTreeNode *p_ftNode = &((*tI).second);
      IBNode *p_node = p_ftNode->p_node;
      // we need to track the number of ports to handle case of missing HCAs
      int numPortWithHCA = 0;

      // go over all child ports
      for (int i = 0; i < p_ftNode->childPorts.size(); i++) {
	if (!p_ftNode->childPorts[i].size()) continue;
	// can not have more then one port in group...
	int portNum = p_ftNode->childPorts[i].front();
	numPortWithHCA++;

	lid = LidByIdx[hcaIdx];

	if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	  cout << "-V- Start routing LID:" << lid
	       << " at HCA idx:" << hcaIdx << endl;
	assignLftDownWards(p_ftNode, lid, portNum, 0,0);

	hcaIdx++;
      }

      // for ports without HCA we assign dummy LID but need to
      // propagate
      for (; numPortWithHCA < maxHcasPerLeafSwitch; numPortWithHCA++)
	{
	  // HACK: for now we can propagate 0 as lid
	  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- adding dummy LID to switch:"
                 << p_node->name
                 << " at HCA idx:" << hcaIdx << endl;

	  assignLftDownWards(p_ftNode, 0, 0xFF, 0,0);

	  hcaIdx++;
	}
    }

  // now go over all switches and route to them
  for (map_tupple_ftnode::iterator tI = NodeByTupple.begin();
       tI != NodeByTupple.end();
       tI++)
    {

      FatTreeNode *p_ftNode = &((*tI).second);
      IBNode *p_node = p_ftNode->p_node;

      if (p_node->type != IB_SW_NODE) continue;

      // find the LID of the switch:
      int lid = 0;
      for (unsigned int pn = 1; (lid == 0) && (pn <= p_node->numPorts); pn++)
	{
	  IBPort *p_port = p_node->getPort(pn);
	  if (p_port)
	    lid = p_port->base_lid;
	}
      if (lid == 0)
	{
	  cout << "-E- failed to find LID for switch:" << p_node->name << endl;
	} else {
	  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- routing to LID:" << lid << " of switch:"
                 << p_node->name << endl;
	  assignLftDownWards(p_ftNode, lid, 0, 0, 0);
	}
    }

  return(0);
}

//////////////////////////////////////////////////////////////////////////////
// Optimally Route a permutation in the Fat Tree
// Prerequisites: Fat Tree structure was built.
//
// Algorithm:
// For each leaf switch (in order)
//   For each HCA index (even if it does not have a LID - invent one)
//     Setup downward paths as previously
//     Traverse up from destination HCA and force output ports as
//     computed by the optimal routing
//
// Data Model:
// We use the fat tree to get ordering.
// "main" routing is the routing from HCA to HCA.
// "side" routing is used from all SW to all HCAs (and dynamic routing)
// Track port utilization for the "main" routing by the "counter1"
// Track port utilzation of the "side" routing in "counter2" field of the port
//
//////////////////////////////////////////////////////////////////////////////

// set FDB values as given in the input
int FatTree::forceLftUpWards(FatTreeNode *p_ftNode, uint16_t dLid, vec_int ports)
{
  // go over all steps
  for (int i=0; i<ports.size(); i++) {
    // if LID is going down we are finished
    if (p_ftNode->goingDown(dLid))
      return 0;
    // sanity check
    if ((ports[i] < 0) || (ports[i] > p_ftNode->parentPorts.size())) {
      cout << "-E- Illegal port number!" << endl;
      return 1;
    }
    IBNode *p_node = p_ftNode->p_node;
    int portNum = p_ftNode->parentPorts[ports[i]].front();

    IBPort* p_port = p_node->getPort(portNum);
    if (!p_port || !p_port->p_remotePort) {
      cout << "-E- Ports do not exist!" << endl;
      return 1;
    }
    IBNode* p_remNode = p_port->p_remotePort->p_node;
    // Set LFT entry
    p_node->setLFTPortForLid(dLid, portNum);
    // Move to next step
    p_ftNode = getFatTreeNodeByNode(p_remNode);
  }
  return 0;
}

// perform the routing by filling in the fabric LFTs
int FatTree::permRoute(vector<string> src, vector<string> dst)
{
  int hcaIdx = 0;
  int lid; // the target LID we propagate for this time
  // extract radix
  vec_byte tmpLeafTupple(N,0);
  tmpLeafTupple[0] = N-1;
  FatTreeNode* p_ftN = &NodeByTupple[tmpLeafTupple];
  int radix = 0;
  for (int i = 0; i < p_ftN->parentPorts.size(); i++)
    if(p_ftN->parentPorts[i].size())
      radix++;

  // create requests vector
  vec_int req;
  req.resize(src.size());
  for (int i=0; i<src.size(); i++)
    req[IdxByName[src[i]]] = IdxByName[dst[i]];

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-V- Tree height: " << N << ", radix: " << radix << endl;

  // create routing system
  RouteSys rS(radix,N);
  // push requests
  rS.pushRequests(req);
  // compute optimal routing
  vec_vec_int routeOutput;
  rS.doRouting(routeOutput);

  // build the LFTs
  // go over all fat tree nodes of the lowest level
  vec_byte firstLeafTupple(N, 0);
  firstLeafTupple[0] = N-1;
  // First prepare downwards routing
  for (map_tupple_ftnode::iterator tI = NodeByTupple.find(firstLeafTupple); tI != NodeByTupple.end(); tI++) {
    FatTreeNode *p_ftNode = &((*tI).second);
    IBNode *p_node = p_ftNode->p_node;
    // we need to track the number of ports to handle case of missing HCAs
    int numPortWithHCA = 0;

    // go over all child ports
    for (int i = 0; i < p_ftNode->childPorts.size(); i++) {
      if (!p_ftNode->childPorts[i].size()) continue;
      // can not have more then one port in group...
      int portNum = p_ftNode->childPorts[i].front();
      numPortWithHCA++;

      lid = LidByIdx[hcaIdx];

      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	cout << "-V- Start routing LID:" << lid
	     << " at HCA idx:" << hcaIdx << endl;

      // Assign downward LFT values
      if (assignLftDownWards(p_ftNode, lid, portNum, 0, 1))
	return 1;

      hcaIdx++;
    }

    // for ports without HCA we assign dummy LID but need to
    // propagate
    for (; numPortWithHCA < maxHcasPerLeafSwitch; numPortWithHCA++) {
      // HACK: for now we can propagate 0 as lid
      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	cout << "-V- adding dummy LID to switch:"
	     << p_node->name
	     << " at HCA idx:" << hcaIdx << endl;

      assignLftDownWards(p_ftNode, 0, 0xFF, 0, 0);

      hcaIdx++;
    }
  }

  // Now prepare upwards routing
  hcaIdx = 0;
  for (map_tupple_ftnode::iterator tI = NodeByTupple.find(firstLeafTupple); tI != NodeByTupple.end(); tI++) {
    FatTreeNode *p_ftNode = &((*tI).second);
    IBNode *p_node = p_ftNode->p_node;

    // go over all child ports
    for (int i = 0; i < p_ftNode->childPorts.size(); i++) {
      if (!p_ftNode->childPorts[i].size()) continue;
      // can not have more then one port in group...
      int portNum = p_ftNode->childPorts[i].front();

      lid = LidByIdx[hcaIdx];

      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	cout << "-V- Start routing LID:" << lid
	     << " at HCA idx:" << hcaIdx << endl;

      lid = LidByIdx[req[hcaIdx]];

      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	cout << "-V- Creating routing from "<<hcaIdx<<"-"<<src[hcaIdx]<<"(lid: "<<LidByIdx[hcaIdx]<<") to "
	     <<req[hcaIdx]<<"-"<<dst[hcaIdx]<<"(lid: "<<LidByIdx[req[hcaIdx]]<<") using up-ports:" << endl;

      // Assign upward LFT values
      if (forceLftUpWards(p_ftNode, lid, routeOutput[hcaIdx]))
	return 1;

      hcaIdx++;
    }

  }

  // now go over all switches and route to them
  for (map_tupple_ftnode::iterator tI = NodeByTupple.begin();
       tI != NodeByTupple.end();
       tI++)
    {

      FatTreeNode *p_ftNode = &((*tI).second);
      IBNode *p_node = p_ftNode->p_node;

      if (p_node->type != IB_SW_NODE) continue;

      // find the LID of the switch:
      int lid = 0;
      for (unsigned int pn = 1; (lid == 0) && (pn <= p_node->numPorts); pn++)
	{
	  IBPort *p_port = p_node->getPort(pn);
	  if (p_port)
	    lid = p_port->base_lid;
	}
      if (lid == 0)
	{
	  cout << "-E- failed to find LID for switch:" << p_node->name << endl;
	} else {
	  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
            cout << "-V- routing to LID:" << lid << " of switch:"
                 << p_node->name << endl;
	  assignLftDownWards(p_ftNode, lid, 0, 0, 0);
	}
    }

  return(0);
}

// dumps out the HCA order into a file ftree.hca
void FatTree::dumpHcaOrder()
{
  ofstream f("ftree.hcas");
  for (unsigned int i = 0; i < LidByIdx.size(); i++)
    {
      // find the HCA node by the base lid given
      unsigned int lid = LidByIdx[i];
      if (lid <= 0)
	{
	  f << "DUMMY_HOST LID" << endl;
	}
      else
	{
	  IBPort *p_port = p_fabric->PortByLid[lid];

	  if (! p_port)
	    {
	      cout << "-E- fail to find port for lid:" << lid << endl;
	      f << "ERROR_HOST LID" << endl;
	    }
	  else
	    {
	      f << p_port->p_node->name << "/" << p_port->num << " " << lid << endl;
	    }
	}
    }
  f.close();
}

void FatTree::dump()
{
  unsigned int level, prevLevel = 2;
  cout << "---------------------------------- FAT TREE DUMP -----------------------------" << endl;
  for (map_tupple_ftnode::const_iterator tI = NodeByTupple.begin();
       tI != NodeByTupple.end();
       tI++)
    {
      level = (*tI).first[0];
      if (level != prevLevel)
	{
	  prevLevel = level;
	  cout << "LEVEL:" << level << endl;
	}

      FatTreeNode const *p_ftNode = &((*tI).second);
      cout << "    " << p_ftNode->p_node->name << " tupple:" << getTuppleStr((*tI).first) << endl;
      for (unsigned int i = 0; i < p_ftNode->parentPorts.size(); i++)
	{
	  if (p_ftNode->parentPorts[i].size())
	    {
	      cout << "       Parents:" << i << endl;
	      for (list< int >::const_iterator lI = p_ftNode->parentPorts[i].begin();
		   lI != p_ftNode->parentPorts[i].end();
		   lI++)
		{
		  unsigned int portNum = *lI;
		  cout << "          p:" << portNum << " ";
		  IBPort *p_port = p_ftNode->p_node->getPort(portNum);
		  if (!p_port || !p_port->p_remotePort)
		    cout << " ERROR " << endl;
		  else
		    cout << p_port->p_remotePort->p_node->name << endl;
		}
	    }
	}

      for (unsigned int i = 0; i < p_ftNode->childPorts.size(); i++)
	{
	  if (p_ftNode->childPorts[i].size())
	    {
	      cout << "       Children:" << i << endl;
	      for (list< int >::const_iterator lI = p_ftNode->childPorts[i].begin();
		   lI != p_ftNode->childPorts[i].end();
		   lI++)
		{
		  unsigned int portNum = *lI;
		  cout << "         p:" << portNum << " ";
		  IBPort *p_port = p_ftNode->p_node->getPort(portNum);
		  if (!p_port || !p_port->p_remotePort)
		    cout << "ERROR " << endl;
		  else
		    cout << p_port->p_remotePort->p_node->name << endl;
		}
	    }
	}
    }

  // now dump the HCA by index:
  cout << "\nLID BY INDEX" << endl;
  for (unsigned int i = 0; i < LidByIdx.size(); i++) {
    int lid = LidByIdx[i];
    IBPort *p_port;

    if (lid != 0)
      {
	p_port = p_fabric->PortByLid[lid];
	if (p_port)
	  {
            cout << "   " << i << " -> " << LidByIdx[i]
                 << " " << p_port->getName() << endl;
	  }
	else
	  {
            cout << "   ERROR : no port for lid:" << lid << endl;
	  }
      }
  }
}

// perform the whole thing
int FatTreeAnalysis(IBFabric *p_fabric)
{
  FatTree ftree(p_fabric);
  if (!ftree.isValid) return(1);
  ftree.dumpHcaOrder();
  if (ftree.route()) return(1);
  return(0);
}

int FatTreeRouteByPermutation( IBFabric *p_fabric, const char *srcs, const char* dsts )
{
  vector <string> sources;
  vector <string> destinations;

  char *s1, *s2, *cp;
  char *saveptr;
  s1 = strdup(srcs);
  s2 = strdup(dsts);
  cp = strtok_r(s1, " \t", &saveptr);
  do {
    sources.push_back(cp);
    cp = strtok_r(NULL, " \t", &saveptr);
  }
  while (cp);

  cp = strtok_r(s2, " \t", &saveptr);
  do {
    destinations.push_back(cp);
    cp = strtok_r(NULL, " \t", &saveptr);
  }
  while (cp);

  if (sources.size() != destinations.size()) {
    cout << "-E- Different number of sources and destinations" << endl;
    return 1;
  }

  FatTree ftree(p_fabric);
  if (!ftree.isValid) return(1);
  //ftree.dumpHcaOrder();
  if (ftree.permRoute(sources, destinations)) return(1);
  return(0);
}
