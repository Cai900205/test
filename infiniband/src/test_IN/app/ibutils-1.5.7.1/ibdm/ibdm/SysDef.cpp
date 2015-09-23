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

IB Systems Definition Parsing and Building Systems.

*/

//////////////////////////////////////////////////////////////////////////////

#include "SysDef.h"
#include "Fabric.h"
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

// build all nodes recursively
int
IBSystemsCollection::makeSysNodes(
  IBFabric *p_fabric,        // the fabric we belong to
  IBSystem *p_system,        // the system we build
  IBSysDef *p_parSysDef,     // the sysdef of the parent
  string    parHierName,     // the hier name of the parent "" for top
  map_str_str &mods          // hier name based modifiers list
  )
{
  int anyErr = 0;

  // go over all child sysdefs
  for(map_str_psysinsts::iterator sI = p_parSysDef->SystemsInstByName.begin();
      sI != p_parSysDef->SystemsInstByName.end();
      sI++) {
    string hierInstName = parHierName + (*sI).first;
    IBSysInst *p_inst = (*sI).second;

    // are we at the bottom ?
    if (p_inst->isNode) {
      string nodeName = p_system->name + "/" + hierInstName;
      // simply make this node ...
      IBNode *p_node = new IBNode(nodeName, p_fabric, p_system,
                                  p_inst->nodeType, p_inst->nodeNumPorts);
      if (! p_node) {
        cout << "-E- Fail to create node:" << nodeName << endl;
        anyErr = 1;
      }

      // the device number should be embedded in the master name of
      // the node: MT23108 ...
      const char *p_digit;
      if ((p_digit = strpbrk(p_inst->master.c_str(), "0123456789")) != NULL)
        sscanf(p_digit,"%u", &p_node->devId);

    } else {
      // find the definition of this inst:
      IBSysDef *p_sysDef =
        getInstSysDef(p_parSysDef, p_inst, hierInstName, mods);
      if (p_sysDef) {
        // recurse down
        anyErr |=
          makeSysNodes(
            p_fabric,
            p_system,
            p_sysDef,
            hierInstName + string("/"),
            mods);
      }
    }
  }
  return anyErr;
}

// Get a system definition for an instance applying any modifiers
IBSysDef *
IBSystemsCollection::getInstSysDef(
  IBSysDef  *p_sysDef,  // parent system def
  IBSysInst *p_inst,
  string     hierInstName,
  map_str_str  &mods    // hier name based modifiers list
  )
{
  // find the definition of this inst:
  string master = p_sysDef->fileName + string("/") + p_inst->master;
  map_str_str::iterator mI = mods.find(hierInstName);
  if (mI != mods.end()) {
    string mod = (*mI).second;
    // we support several acronims for a removed subsystem
    if ((mod == string("Removed")) ||
        (mod == string("X")) ||
        (mod == string("R")))
      return NULL;
    master += string(":") + mod;
  }

  // try to find the master:
  IBSysDef *p_subSysDef = getSysDef(master);
  if (! p_subSysDef) {
      cout << "-E- Fail to find definition for system:" << master << endl;
      dump();
      return NULL;
  }
  return p_subSysDef;
}

//
IBPort *
IBSystemsCollection::makeNodePortByInstAndPortName(
  IBSystem     *p_system,    // the system we build the node port in
  IBSysDef     *p_sysDef,     // the system definition holding the inst
  IBSysInst    *p_inst,       // the instance
  string        instPortName, // the port name
  string        hierInstName, // the hier name of the instance
  map_str_str  &mods          // hier name based modifiers list
  )
{
  IBSysDef *p_subSysDef;

  // find the definition of this inst:
  p_subSysDef = getInstSysDef(p_sysDef, p_inst, hierInstName, mods);
  if (p_subSysDef) {
    // can we find the sys port by the given name ?
    map_str_psysportdef::iterator pI =
      p_subSysDef->SysPortsDefs.find(instPortName);

    // not all connections exists - since we might very the boards...
    if (pI == p_subSysDef->SysPortsDefs.end()) return NULL;

    IBSysPortDef *p_subInstPort = (*pI).second;

    // recurse downwards
    return(
      makeNodePortBySysPortDef(
        p_system, p_subSysDef, p_subInstPort, hierInstName + string("/"),
        mods)
      );
  }
  return NULL;
}

// find the lowest point connection of this port and make it if a node port
// assumes the nodes were already created for the
IBPort *
IBSystemsCollection::makeNodePortBySysPortDef(
  IBSystem      *p_system,    // the system we build the node port in
  IBSysDef      *p_sysDef,    // the system definition the port is on
  IBSysPortDef  *p_sysPortDef,// system port definition
  string         parHierName, // the hier name of the parent "" for top
  map_str_str   &mods         // hier name based modifiers list
  )
{

  IBPort *p_port = NULL;
  // find the instance def:
  map_str_psysinsts::iterator sI =
    p_sysDef->SystemsInstByName.find(p_sysPortDef->instName);

  if (sI == p_sysDef->SystemsInstByName.end()) {
    cout << "-E- Fail to find the instance:" << p_sysPortDef->instName
         << " connected to port:" << p_sysPortDef->name << endl;
    return NULL;
  }

  IBSysInst *p_inst = (*sI).second;

  // is it a node?
  if (p_inst->isNode) {
    // Try to find the node in the system
    string nodeName = p_system->name + "/" + parHierName + p_inst->name;
    IBNode *p_node = p_system->getNode(nodeName.c_str());
    if (! p_node) {
      cout << "-E- Fail to find node:" << nodeName
           << " connected to port:" << p_sysPortDef->name <<  endl;
      return NULL;
    }

    // simply make this node port ...
    p_port = p_node->makePort(atoi(p_sysPortDef->instPortName.c_str()));
    if (! p_port) {
      cout << "-E- Fail to make port:" << nodeName << "/"
           << p_sysPortDef->instPortName << endl;
      return NULL;
    }
    p_port->width = p_sysPortDef->width;
    p_port->speed = p_sysPortDef->speed;
    return p_port;
  } else {
    // obtain the sys port making to the inst port
    string hierInstName = parHierName + p_inst->name;

    return (
      makeNodePortByInstAndPortName(
        p_system,
        p_sysDef,
        p_inst,
        p_sysPortDef->instPortName,
        hierInstName,
        mods)
      );
  }
  return NULL;
}

// find the lowest point connection of this sub sysport and make a node port
// assumes the nodes were already created for the
IBPort *
IBSystemsCollection::makeNodePortBySubSysInstPortName(
  IBSystem      *p_system,    // the system we build the node port in
  IBSysDef      *p_sysDef,    // the system definition the inst is in
  string         instName,    // Name of the instance
  string         instPortName,// Name of instance port
  string         parHierName, // the hier name of the parent "" for top
  map_str_str   &mods         // hier name based modifiers list
  )
{
  // find the instance
  IBPort *p_port = NULL;

  // find the instance def:
  map_str_psysinsts::iterator sI =
    p_sysDef->SystemsInstByName.find(instName);

  if (sI == p_sysDef->SystemsInstByName.end()) {
    cout << "-E- Fail to find the instance:" << instName << endl;
    return NULL;
  }

  IBSysInst *p_inst = (*sI).second;

  // is it a node?
  if (p_inst->isNode) {
    // Try to find the node in the system
    string nodeName = p_system->name + "/" + parHierName + p_inst->name;
    IBNode *p_node = p_system->getNode(nodeName.c_str());
    if (! p_node) {
      cout << "-E- Fail to find node:" << nodeName
           <<  endl;
      return NULL;
    }

    // simply make this node port ...
    p_port = p_node->makePort(atoi(instPortName.c_str()));
    return p_port;
  } else {
    // obtain the sys port making to the inst port
    string hierInstName = parHierName + p_inst->name;

    return (
      makeNodePortByInstAndPortName(
        p_system,
        p_sysDef,
        p_inst,
        instPortName,
        hierInstName,
        mods)
      );
  }
  return NULL;
}

//  DFS from top on each level connect all connected SysInst ports
int
IBSystemsCollection::makeSubSystemToSubSystemConns(
  IBSystem      *p_system,    // the system we build the node port in
  IBSysDef      *p_sysDef,    // the system definition the port is on
  string         parHierName, // the hier name of the parent "" for top
  map_str_str   &mods         // hier name based modifiers list
  )
{
  int anyErr = 0;

  // go over all instances
  for (map_str_psysinsts::iterator iI = p_sysDef->SystemsInstByName.begin();
       iI != p_sysDef->SystemsInstByName.end();
       iI++) {

    IBSysInst *p_inst = (*iI).second;

    // go through all inst ports aand connect them
    for (map_str_pinstport::iterator pI = p_inst->InstPorts.begin();
         pI != p_inst->InstPorts.end();
         pI++) {
      IBSysInstPort *p_instPort = (*pI).second;

      IBPort *p_port =
        makeNodePortBySubSysInstPortName(
          p_system,
          p_sysDef,
          p_inst->name,
          p_instPort->name,
          parHierName,
          mods);

      // we might not find this inst port on this mod:
      if (! p_port) continue;

      // make the remote inst port
      IBPort *p_remPort =
        makeNodePortBySubSysInstPortName(
          p_system,
          p_sysDef,
          p_instPort->remInstName,
          p_instPort->remPortName,
          parHierName,
          mods);

      // can it be disconnected on the other side?
      if (! p_remPort) continue;

      // do the connection:
      p_port->connect(p_remPort, p_instPort->width, p_instPort->speed);
      p_remPort->connect(p_port, p_instPort->width, p_instPort->speed);
    }

    // descend the hirarchy if not a node:
    if (! p_inst->isNode) {
      IBSysDef  *p_instSysDef =
        getInstSysDef(p_sysDef, p_inst, parHierName + p_inst->name, mods);
      // might be a removed subsys...
      if (p_instSysDef) {
        anyErr |=
          makeSubSystemToSubSystemConns(
            p_system, p_instSysDef, parHierName + p_inst->name + string("/"),
            mods);
      }
    }
  }
  return anyErr;
}

// Given the name and type of the system - build its system by applying any
// modifiers by hierarchical instance name.
IBSystem *
IBSystemsCollection::makeSystem(
  IBFabric *p_fabric,
  string name,
  string master,
  map_str_str mods)
{
  // ALGO:

  // Find the master system definition
  IBSysDef *p_sysDef = getSysDef(master);
  if (! p_sysDef) {
    cout << "-E- Fail to find definition for system:" << master << endl;
    return NULL;
  }

  // Create the top level system:
  IBSystem *p_system = new IBSystem(name, p_fabric, master);

  // create all node insts:
  //   DFS down through all subsys apply any inst modifier rules
  //   create any nodes found.
  if (makeSysNodes(p_fabric, p_system, p_sysDef, "", mods)) {
    delete p_system;
    return NULL;
  }

  // perform connections:
  //   1. On top level create connections from SysPorts.
  for ( map_str_psysportdef::iterator pI = p_sysDef->SysPortsDefs.begin();
        pI != p_sysDef->SysPortsDefs.end();
        pI++) {

    // find the lowest point connection of this port and make it if a node port
    IBPort *p_port =
      makeNodePortBySysPortDef(p_system, p_sysDef, (*pI).second,"", mods);

    // might have been a disconnected port.
    if (! p_port) continue;

    // make the system port - since it is connected.
    IBSysPort *p_sysPort = new IBSysPort((*pI).first, p_system);
    p_sysPort->p_nodePort = p_port;
    p_port->p_sysPort = p_sysPort;
  }

  //   2. BFS from top on each level connect all connected SysPortDefs
  if (makeSubSystemToSubSystemConns(p_system, p_sysDef, "", mods)) {
    delete p_system;
    return NULL;
  }

  // Last step is to use given set of sub-inst attributes adn assign
  // node attributes:
  for (map_str_str::iterator siA = p_sysDef->SubInstAtts.begin();
       siA != p_sysDef->SubInstAtts.end();
       siA++) {
    string nodeName = p_system->name + "/" + (*siA).first;
    // try to find the node:
    IBNode *p_node =
      p_system->getNode(nodeName);
    if (p_node) {
      p_node->attributes =  (*siA).second ;
    } else {
      cout << "-W- Fail to set attributes:" << (*siA).second
           << " on non-existing Node:" << nodeName << endl;
    }
  }

  return p_system;
}

static
list< string > getDirIbnlFiles(string dir) {
  DIR *dp;
  struct dirent *ep;
  list< string > res;
  char *lastDot;

  dp = opendir (dir.c_str());
  if (dp != NULL)
  {
    while ((ep = readdir (dp)))
    {
      lastDot = strrchr(ep->d_name,'.');
      if (lastDot && !strcmp(lastDot, ".ibnl"))
      {
        res.push_back(ep->d_name);
      }
    }
    closedir(dp);
  }

  return res;
}

// The parser itself is part of the ibnl_parser.yy
extern int
ibnlParseSysDefs (IBSystemsCollection *p_sysColl, const char *fileName);

// Parse a system definition netlist
int
IBSystemsCollection::parseIBSysdef(string fileName) {
  return ibnlParseSysDefs(this, fileName.c_str());
}

// Read all IBNL files available from the given path list.
int
IBSystemsCollection::parseSysDefsFromDirs(list< string > dirs)
{
  int anyErr = 0;
  // go through all directories
  for (list< string >::iterator dI = dirs.begin();
       dI != dirs.end();
       dI++) {
    string dirName = (*dI);

    // find all matching files
    list <string > ibnlFiles = getDirIbnlFiles(dirName);

    // parse them all:
    for (list< string >::iterator fI = ibnlFiles.begin();
         fI != ibnlFiles.end();
         fI++) {
      string fileName = dirName + string("/") + (*fI);

      if (ibnlParseSysDefs(this, fileName.c_str())) {
        cout << "-E- Error parsing System definition file:"
             <<  fileName << endl;
        anyErr |= 1;
      } else {
        if (FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
          cout << "-I- Loaded system definition from:"  << fileName
               << endl;
      }
    }
  }
  return anyErr;
}

// dump out the available systems:
void
IBSystemsCollection::dump()
{
  for (map_str_psysdef::iterator sI = SysDefByName.begin();
       sI != SysDefByName.end();
       sI++) {
    cout << "-I- Found Definition for:" << (*sI).first <<endl;
  }

}

// we use a singleton system repository
IBSystemsCollection *theSysDefsCollection()
{
  static IBSystemsCollection *p_sysDefsColl = NULL;

  // we only need to initialize once.
  if (! p_sysDefsColl) {
    p_sysDefsColl = new IBSystemsCollection();
    list< string > dirs;
#ifdef IBDM_IBNL_DIR
    dirs.push_back(string(IBDM_IBNL_DIR "/ibnl"));
#endif
	 char *ibnlDirs = getenv("IBDM_IBNL_PATH");
	 if (ibnlDirs != NULL)
	 {
		 string delimiters(":, ");
		 string str = string(ibnlDirs);
		 // Skip delimiters at beginning.
		 string::size_type lastPos = str.find_first_not_of(delimiters, 0);
		 // Find first "non-delimiter".
		 string::size_type pos     = str.find_first_of(delimiters, lastPos);

		 while (string::npos != pos || string::npos != lastPos)
		 {
			 // Found a token, add it to the vector.
			 dirs.push_back(str.substr(lastPos, pos - lastPos));
			 // Skip delimiters.  Note the "not_of"
			 lastPos = str.find_first_not_of(delimiters, pos);
			 // Find next "non-delimiter"
			 pos = str.find_first_of(delimiters, lastPos);
		 }
	 }

	 if (dirs.size() == 0)
	 {
		 cout << "-E- No IBNL directories provided. " << endl;
		 cout << "    Please provide environment variable IBDM_IBNL_PATH" <<endl;
		 cout << "    with a colon separated list of ibnl directories." << endl;
	 }
    p_sysDefsColl->parseSysDefsFromDirs(dirs);
  }
  return p_sysDefsColl;
}

