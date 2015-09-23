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

#include "RouteSys.h"
#include "Bipartite.h"

// Helper power function

int RouteSys::myPow(int base, int pow) {
  int res = 1;
  for (int i=0; i<pow; i++)
    res = res*base;

  return res;
}

/////////////////////////////////////////////////////////////////////////////

// C'tor

RouteSys::RouteSys(int rad, int hgth, int s):radix(rad),height(hgth),step(s) {
  // Calculate num in/out ports
  ports = myPow(rad,height);
  // Create and init ports
  inPorts = new inputData[ports];
  outPorts = new bool[ports];

  for (int i=0; i<ports; i++) {
    inPorts[i].used = false;
    outPorts[i] = false;
  }
  // Create sub-systems
  if (height > 1) {
    subSys = new RouteSys* [rad];
    for (int i=0; i<radix; i++)
      subSys[i] = new RouteSys(rad,height-1,s+1);
  }
}

//////////////////////////////////////////////////////////////////////////////

// D'tor

RouteSys::~RouteSys() {
  // Just kill everything
  delete[] inPorts;
  delete[] outPorts;

  if (height > 1) {
    for (int i=0; i<radix; i++)
      delete subSys[i];
    delete[] subSys;
  }
}

///////////////////////////////////////////////////////////////////////////////

// Add requests to the system

int RouteSys::pushRequests(vec_int req)
{
  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-V- Pushing requests" << endl;

  for (int i=0; i<req.size(); i++) {
    // Extract comm pair
    int src = i;
    int dst = req[i];

    if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
      cout << "-V- Req: " << src << "->" << dst << endl;

    // Check port existence
    if ((src >= ports) || (dst >= ports)) {
      cout << "-E- Port index exceeds num ports! Ports: " << ports << ", src: " << src << ", dst: " << dst << endl;
      return 1;
    }
    // Check port availability
    if (inPorts[src].used || outPorts[dst]) {
      cout << "-E- Port already used! src: " << src << ", dst: " << dst << endl;
      return 1;
    }
    // Mark ports as used
    inPorts[src].used = true;
    inPorts[src].src = src;
    inPorts[src].dst = dst;
    inPorts[src].inputNum = src;
    inPorts[src].outNum = dst;

    outPorts[dst] = true;
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////////////

// Perform routing after requests were pushed

int RouteSys::doRouting (vec_vec_int& out)
{
  // Atomic system nothing to do
  if (ports == radix)
    return 0;

  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-V- Starting routing, step: " << step << ", height " << height << endl;

  // Init the output structure
  if (out.size() < ports) {
    out.resize(ports);
    for (int i=0; i<ports; i++) {
      out[i].resize(height-1);
      for (int j=0; j<height-1; j++)
	out[i][j] = -1;
    }
  }

  // We use three graph arrays for coloring
  Bipartite** buff[3];
  buff[0] = new Bipartite*[radix];
  buff[1] = new Bipartite*[radix];
  buff[2] = new Bipartite*[radix];

  for (int i=0; i<radix; i++)
    buff[0][i] = buff[1][i] = buff[2][i] = NULL;

  // Number of colors derived through perfect matching
  int matchGraphs = 0;
  int currRadix = radix;
  int idx = 0;
  int idx_new = 1;

  // Create the first graph
  if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    cout << "-V- Ports: " << ports << ", radix: " << radix << endl;
  buff[0][0] = new Bipartite(ports/radix,radix);

  // Add connections
  for (int i=0; i<ports; i++)
    buff[0][0]->connectNodes(i/radix,inPorts[i].outNum/radix,inPorts[i]);

  // Now decompose the graph to radix-1 graphs
  while (1 < currRadix) {
    for (int i=0; buff[idx][i] && i<radix; i++) {
      // Odd radix, perfect matching is required
      if (currRadix % 2) {
	if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	  cout << "-V- Perfect matching is required" << endl;
	buff[2][matchGraphs] = buff[idx][i]->maximumMatch();
	matchGraphs++;
      }
      // Now we can perform Euler decomposition
      if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
	cout << "-V- Performing Euler decompostion" << endl;
      if (2*i+1 >= radix) {
	cout << "-E- Graph index illegal"<< endl;
	return 1;
      }
      buff[idx][i]->decompose(&buff[idx_new][2*i],&buff[idx_new][2*i+1]);
      delete buff[idx][i];
      buff[idx][i] = NULL;
    }
    idx = idx_new;
    idx_new = (idx_new+1)%2;
    currRadix = currRadix / 2;
  }
  // Collect all result graphs to array buff[2][i]
  for (int i=matchGraphs; i<radix; i++)
    buff[2][i] = buff[idx][i-matchGraphs];

  // Apply decomposition results
  for (int i=0; i < radix; i++) {
    Bipartite* G = buff[2][i];
    if (!G->setIterFirst()) {
      cout << "-E- Empty graph found!" << endl;
      return 1;
    }
    bool stop = false;
    while (!stop) {
      inputData d = G->getReqDat();
      // Build output
      if (out.size() <= d.src || out[d.src].size() <= step) {
	cout << "Output index illegal" << endl;
	return 1;
      }
      out[d.src][step] = i;
      // Add request to sub-system
      RouteSys* sub = subSys[i];
      int inPort = d.inputNum/radix;
      int outPort = d.outNum/radix;
      if (sub->inPorts[inPort].used || sub->outPorts[outPort]) {
	cout << "Port already used! inPort: " << inPort << ", outPort: " << outPort << endl;
	return 1;
      }
      // Mark ports as used
      sub->inPorts[inPort].used = true;
      sub->inPorts[inPort].src = d.src;
      sub->inPorts[inPort].dst = d.dst;
      sub->inPorts[inPort].inputNum = inPort;
      sub->inPorts[inPort].outNum = outPort;

      outPorts[outPort] = true;
      // Next request
      if (!G->setIterNext()) {
	stop = true;
      }
    }
  }

  // Free memory
  for (int i=0; i<radix; i++)
    delete buff[2][i];

  delete[] buff[0];
  delete[] buff[1];
  delete[] buff[2];

  // Run routing on sub-systems
  for (int i=0; i<radix; i++) {
    if (subSys[i]->doRouting(out)) {
      cout << "-E- Subsystem routing failed!" << endl;
      return 1;
    }
  }

  return 0;
}
