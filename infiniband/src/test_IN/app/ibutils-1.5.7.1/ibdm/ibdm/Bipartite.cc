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

#include "Bipartite.h"

//CLASS edge///////////////////////////////////

bool edge::isMatched() {
  vertex* ver1 = (vertex*)v1;
  vertex* ver2 = (vertex*)v2;

  if (((this == ver1->getPartner()) && (this != ver2->getPartner())) || ((this == ver2->getPartner()) && (this != ver1->getPartner())))
    cout << "-E- Error in edge matching" << endl;

  return (this == ver1->getPartner()) && (this == ver2->getPartner());
}

//CLASS vertex/////////////////////////////////

vertex::vertex(int n, side sd, int rad):id(n),s(sd),radix(rad)
{
  connections = new edge*[radix];
  pred = new edge*[radix];
  succ = new edge*[radix];

  partner = NULL;
  for (int i=0; i<radix; i++)
    connections[i] = pred[i] = succ[i] = NULL;

  predCount = succCount = 0;
  maxUsed = -1;
}

vertex::~vertex()
{
  delete[] connections;
  delete[] pred;
  delete[] succ;
}

int vertex::getID() const
{
  return id;
}

side vertex::getSide() const
{
  return s;
}

void vertex::delConnection(edge* e)
{
  int my_idx, other_idx;
  vertex* v;

  // Find the side we are connected at and edge index
  if (e->v1 == this) {
    my_idx = e->idx1;
    other_idx = e->idx2;
    v = (vertex*)(e->v2);
  }
  else if (e->v2 == this) {
    my_idx = e->idx2;
    other_idx = e->idx1;
    v = (vertex*)(e->v1);
  }
  else {
    cout << "-E- Edge not connected to current vertex" << endl;
    return;
  }

  if (my_idx >= radix || other_idx >= radix) {
    cout << "-E- Edge index illegal" << endl;
    return;
  }

  // Now disconnect
  connections[my_idx] = NULL;
  maxUsed--;
  v->connections[other_idx] = NULL;
  v->maxUsed--;
}

void vertex::pushConnection(edge* e)
{
  maxUsed++;
  if (maxUsed == radix) {
    cout << "-E- Can't push connection to vertex: " << id << ", exceeding radix!" << endl;
    return;
  }
  // Mark our side
  if (e->v1 == NULL) {
    e->v1 = this;
    e->idx1 = maxUsed;
  }
  else if (e->v2 == NULL) {
    e->v2 = this;
    e->idx2 = maxUsed;
  }
  else {
    cout << "-E- Can't push connection both edges are already filled" << endl;
    return;
  }

  if (maxUsed >= radix) {
    cout << "-E- maxUsed illegal" << endl;
    return;
  }

  // Now connect
  connections[maxUsed] = e;
}

edge* vertex::popConnection()
{
  // Look for a connection
  int i=0;
  while ((i<radix) && !connections[i])
    i++;
  // No real connection found
  if (i == radix)
    return NULL;

  edge* tmp = connections[i];

  // Disconnect chosen edge
  connections[i] = NULL;
  if (tmp->v1 == this) {
    vertex* v = (vertex*)(tmp->v2);
    v->connections[tmp->idx2] = NULL;
  }
  else if (tmp->v2 == this) {
    vertex* v = (vertex*)(tmp->v1);
    v->connections[tmp->idx1] = NULL;
  }
  else {
    cout << "-E- Edge not connected to current vertex" << endl;
    return NULL;
  }

  if (tmp->idx1 >= radix || tmp->idx2 >= radix) {
    cout << "-E- Edge index illegal" << endl;
    return NULL;
  }

  return tmp;
}

// For unmacthed vertex, find an unmatched neighbor and match the pair
bool vertex::match()
{
  // Already matched
  if (partner)
    return false;
  // Look for neighbor
  for (int i=0; i<radix; i++) {
    if (connections[i]) {
      vertex* v = (vertex*)(connections[i]->otherSide(this));
      if (!v->partner) {
        // Match
	partner = connections[i];
	v->partner = connections[i];
	return true;
      }
    }
  }
  return false;
}

edge* vertex::getPartner() const
{
  return partner;
}

bool vertex::getInLayers() const
{
  return inLayers;
}

void vertex::setInLayers(bool b)
{
  inLayers = b;
}

void vertex::resetLayersInfo()
{
  inLayers = false;
  succCount = predCount = 0;
  for (int i=0; i<radix; i++)
    succ[i] = pred[i] = NULL;
}

void vertex::addPartnerLayers(list<vertex*>& l)
{
  // No partner
  if (!partner)
    return;
  vertex* p = (vertex*)(partner->otherSide(this));
  // Partner already in one of the layers
  if (p->inLayers)
    return;
  // Add partner to the layer
  l.push_front(p);
  p->inLayers = true;
  // Update pred/succ relations
  if (succCount >= radix) {
    cout << "-E- More successors than radix" << endl;
    return;
  }
  succ[succCount] = partner;
  succCount++;

  if (p->predCount >= radix) {
    cout << "-E- More predecessors than radix" << endl;
    return;
  }
  p->pred[p->predCount] = partner;
  p->predCount++;
}

bool vertex::addNonPartnersLayers(list<vertex*>& l)
{
  vertex* prtn = NULL;
  bool res = false;

  if (partner)
    prtn = (vertex*)(partner->otherSide(this));

  for (int i=0; i<radix; i++) {
    vertex* v = (vertex*)(connections[i]->otherSide(this));
    if ((v != prtn) && (!v->inLayers)) {
      // free vertex found
      if (!v->partner)
	res = true;
      // Add vertex to the layer
      l.push_front(v);
      v->inLayers = true;
      // Update pred/succ relations
      if (succCount >= radix) {
	cout << "-E- More successors than radix" << endl;
	return false;
      }
      succ[succCount] = connections[i];
      succCount++;

      if (v->predCount >= radix) {
	cout << "-E- More predecessors than radix" << endl;
	return false;
      }
      v->pred[v->predCount] = connections[i];
      v->predCount++;
    }
  }
  return res;
}

vertex* vertex::getPredecessor() const
{
  vertex* v = NULL;
  // Find a valid predecessor still in layers
  for (int i=0; i<radix; i++) {
    if (pred[i]) {
      vertex* v2 = (vertex*)(pred[i]->otherSide(this));
      if (v2->inLayers) {
	v = v2;
	break;
      }
    }
  }
  return v;
}

// Flip edge status on augmenting path
void vertex::flipPredEdge(int idx)
{
  int i=0;
  // Find an edge to a predecessor
  for (i=0; i<radix; i++)
    if (pred[i]) {
      vertex* v1 = (vertex*)pred[i]->v1;
      vertex* v2 = (vertex*)pred[i]->v2;
      if (v1->getInLayers() && v2->getInLayers())
	break;
    }

  if (i == radix) {
    cout << "-E- Could find predecessor for flip" << endl;
    return;
  }
  // The predecessor vertex
  vertex* v = (vertex*) pred[i]->otherSide(this);

  // Unmatch edge
  if (idx)
	v->partner = NULL;
  // Match edge
  else {
    partner = pred[i];
    v->partner = pred[i];
  }
}

// Removing vertex from layers and updating successors/predecessors
void vertex::unLink(list<vertex*>& l)
{
  inLayers = false;
  for (int i=0; i<radix; i++) {
    if (succ[i]) {
      vertex* v = (vertex*)succ[i]->otherSide(this);
      if (v->inLayers) {
	v->predCount--;
	// v has no more predecessors, schedule for removal from layers...
	if (!v->predCount)
	  l.push_back(v);
	succ[i] = NULL;
      }
    }
  }
  succCount = 0;
}

//CLASS Bipartite

// C'tor

Bipartite::Bipartite(int s, int r):size(s),radix(r)
{
  leftSide = new vertex*[size];
  rightSide = new vertex*[size];

  for (int i=0; i<size; i++) {
    leftSide[i] = new vertex(i,LEFT,radix);
    rightSide[i] = new vertex(i,RIGHT,radix);
  }
}

///////////////////////////////////////////////////////////

// D'tor

Bipartite::~Bipartite()
{
  // Free vertices
  for (int i=0; i<size; i++) {
    delete leftSide[i];
    delete rightSide[i];
  }
  delete[] leftSide;
  delete[] rightSide;

  // Free edges
  while (List.size()) {
    edge* e = (edge*)(List.front());
    List.pop_front();
    delete e;
  }
}

////////////////////////////////////////////////////////////

// Get radix

int Bipartite::getRadix() const
{
  return radix;
}

////////////////////////////////////////////////////////////

// Set edges listt iterator to first

bool Bipartite::setIterFirst()
{
  it = List.begin();
  if (it == List.end())
    return false;
  return true;
}

///////////////////////////////////////////////////////////

// Set edges list iterator to next

bool Bipartite::setIterNext()
{
  if (it == List.end())
    return false;
  it++;
  if (it == List.end())
    return false;
  return true;
}

////////////////////////////////////////////////////////////

// Get current edge's request data

inputData Bipartite::getReqDat()
{
  if (it == List.end()) {
    cout << "-E- Iterator points to list end" << endl;
  }
  return ((edge*)(*it))->reqDat;
}

/////////////////////////////////////////////////////////////

// Add connection between the nodes to the graph

void Bipartite::connectNodes(int n1, int n2, inputData reqDat)
{
  if ((n1 >= size) || (n2 >= size)) {
    cout << "-E- Node index exceeds size" << endl;
    return;
  }
  // Create new edge
  edge* newEdge = new edge;

  // Init edge fields and add to it the edges list
  newEdge->it = List.insert(List.end(), newEdge);
  newEdge->reqDat = reqDat;
  newEdge->v1 = newEdge->v2 = NULL;

  // Connect the vertices
  leftSide[n1]->pushConnection(newEdge);
  rightSide[n2]->pushConnection(newEdge);
}

////////////////////////////////////////////////////////////////

// Find maximal matching

void Bipartite::maximalMatch()
{
  // Invoke match on left-side vertices
  for (int i=0; i<size; i++)
    leftSide[i]->match();
}

////////////////////////////////////////////////////////////////

// Find maximum matching

// Hopcroft-Karp algorithm
Bipartite* Bipartite::maximumMatch()
{
  // First find a maximal match
  maximalMatch();

  list<void*>::iterator it2 = List.begin();
  list<vertex*> l1, l2;
  list<vertex*>::iterator it;

  // Loop on algorithm phases
  while (1) {
    bool hardStop = false;
    // First reset layers info
    for (int i=0; i<size; i++) {
      leftSide[i]->resetLayersInfo();
      rightSide[i]->resetLayersInfo();
    }
    // Add free left-side vertices to l1 (building layer 0)
    l1.clear();
    for (int i=0; i<size; i++) {
      if (!leftSide[i]->getPartner()) {
	l1.push_front(leftSide[i]);
	leftSide[i]->setInLayers(true);
      }
    }
    // This is our termination condition
    // Maximum matching achieved
    if (l1.empty())
      break;
    // Loop on building layers
    while (1) {
      bool stop = false;
      l2.clear();
      // Add all non-partners of vertices in l1 to layers (l2)
      // We signal to stop if a free (right-side) vertex is entering l2
      for (it = l1.begin(); it != l1.end(); it++)
	if ((*it)->addNonPartnersLayers(l2))
	  stop = true;
      // Found free vertex among right-side vertices
      if (stop) {
	// There are augmenting paths, apply them
	augment(l2);
	break;
      }
      // This is a terminal condition
      if (l2.empty()) {
	hardStop = true;
	break;
      }
      // Add partners of vertices in l2 to layers (l1)
      l1.clear();
      for (it = l2.begin(); it!= l2.end(); it++)
	(*it)->addPartnerLayers(l1);
      // This is a terminal condition
      if (l1.empty()) {
	hardStop = true;
	break;
      }
    }
    // Maximum matching achieved
    if (hardStop)
      break;
  }
  // Build the matching graph
  Bipartite* M = new Bipartite(size, 1);
  // Go over all edges and move matched one to the new graph
  it2 = List.begin();
  while (it2 != List.end()) {
    edge* e = (edge*)(*it2);
    if (e->isMatched()) {
      vertex* v1 = (vertex*)(e->v1);
      vertex* v2 = (vertex*)(e->v2);
      // Unlink vertices
      ((vertex*)(e->v1))->delConnection(e);
      // Update edges list
      it2 = List.erase(it2);
      // Add link to the new graph
      if (v1->getSide() == LEFT)
	M->connectNodes(v1->getID(), v2->getID(), e->reqDat);
      else
	M->connectNodes(v2->getID(), v1->getID(), e->reqDat);
      // Free memory
      delete e;
    }
    else
      it2++;
  }
  return M;
}

//////////////////////////////////////////////////////////////////////

// Apply augmenting paths on the matching

void Bipartite::augment(list<vertex*>& l)
{
  // Use delQ to store vertex scheduled for the removal from layers
  list<vertex*> delQ;
  // Remove non-free vertices
  list<vertex*>::iterator it = l.begin();
  while (it != l.end()) {
    if ((*it)->getPartner()) {
      delQ.push_front((*it));
      it = l.erase(it);
    }
    else
      it++;
  }
  // Get rid of non-free vertices
  while (!delQ.empty()) {
    vertex* fr = delQ.front();
    delQ.pop_front();
    fr->unLink(delQ);
  }

  if (l.empty()) {
    cout << "-E- No free vertices left!" << endl;
    return;
  }
  // Augment and reset pred/succ relations
  while (!l.empty()) {
    vertex* curr = l.front();
    l.pop_front();
    int idx = 0;
    // Backtrace the path and augment
    int length=0;
    while (1) {
      delQ.push_front(curr);
      // Its either the end of a path or an error
      if (!curr->getPredecessor()) {
	if (!idx && length) {
	  cout << "-E- This vertex must have predecessor" << endl;
	  return;
	}
	break;
      }
      // Flip edge status
      curr->flipPredEdge(idx);
      // Move back
      curr = curr->getPredecessor();
      idx = (idx+1)%2;
      length++;
    }
    // Now clean the delQ
    while (!delQ.empty()) {
      vertex* fr = delQ.front();
      delQ.pop_front();
      fr->unLink(delQ);
    }
  }
}

////////////////////////////////////////////////////////////////////////

// Perform Euler decomposition

void Bipartite::decompose(Bipartite** bp1, Bipartite** bp2)
{
  if (radix < 2) {
    cout << "-E- Radix value illegal: " << radix << endl;
    return;
  }

  // Create new graphs
  Bipartite* arr[2];
  arr[0] = new Bipartite(size, radix/2);
  arr[1] = new Bipartite(size, radix/2);

  // Continue as long as unassigned edges left
  while (!List.empty()) {
    int idx = 0;
    edge* e = (edge*)List.front();
    vertex* current = (vertex*)e->v1;
    e = current->popConnection();

    while (e) {
      // Connect nodes in the new graphs
      vertex* v1 = (vertex*)e->v1;
      vertex* v2 = (vertex*)e->v2;
      if (v1->getSide() == LEFT)
	arr[idx]->connectNodes(v1->getID(), v2->getID(), e->reqDat);
      else
	arr[idx]->connectNodes(v2->getID(), v1->getID(), e->reqDat);
      idx = (idx+1)%2;
      // Remove edge from list
      List.erase(e->it);
      // Pick next vertex
      current = (vertex*) e->otherSide(current);
      // Free memory
      delete e;
      // Pick next edge
      e = current->popConnection();
    }
  }
  *bp1 = arr[0];
  *bp2 = arr[1];
}
