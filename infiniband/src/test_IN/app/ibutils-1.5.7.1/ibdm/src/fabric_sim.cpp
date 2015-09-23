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
 * Fabric Simulator
 *
 * Reads a cabling list file and run OpenSM like initialization
 * providing statistics and analysis.
 */

#include "Fabric.h"
#include "SubnMgt.h"
#include "CredLoops.h"

#include <getopt.h>

static char FabricSimUsage[] =
"Usage: ibdmsim [-v][-h] {-c <cbaling file>|-t <topo file>} -s <sm node name> -p <sm port num> [-l <lmc>] [-a][-e]";

void
show_usage() {
  cout << FabricSimUsage << endl;
}

void
show_help() {
  cout << "\n"
       << " Fabric Simulator\n"
       << "------------------\n"
       << "\n"
       << FabricSimUsage << "\n"
       << "\n"
       << "Description:\n"
       << "  This utility parses a cabling list or topology files describing the\n"
       << " systems connections that make a fabric.\n"
       << " Then it performs the following initialization\n"
       << "  algorithms and reports back statistics:\n"
       << "  1. Assign Lids\n"
       << "  2. Calculate Min-Hop tables for all switch nodes\n"
       << "  3. Route (populate the LFT in the switches)\n"
       << "  4. Trace route of all CA - to - CA pairs.\n"
       << "  5. Optionaly run check for Credit Loops.\n"
       << "\n"
       << "Arguments:\n"
       << "  -t|--topology <file> = Topology file.\n"
       << "   The format is defined in the IBDM user manual.\n"
       << "  -c|--cables <file> = Cabling list file. Following the line format:\n"
       << "   <Sys1Type> <Sys1Name> <Port1Name> <Sys2Type> <Sys2Name> <Port2Name>\n"
       << "  -s|--sm-node <name> = The name of the SM node (not system). E.g. OSM/U1.\n"
       << "  -p|--port-num <num> = The number of the port SM is connected to.\n"
       << "\n"
       << "Options:\n"
       << "  -v|--verbose = verbsoe mode\n"
       << "  -h|--help = provide this help message\n"
       << "  -a|--analyze-loops = Analyze credit loops\n"
       << "  -e|--enhanced-route = Use LMC aware Enhanced Routing algorithm.\n"
       << "  -l|--lmc <lmc> = Use the LMC when assigning lids.\n"
       << "\n"
       << "\n"
       << "Author: Eitan Zahavi, Mellanox Technologies LTD.\n"
       << endl;
}

int main (int argc, char **argv) {
  /*
   * Parseing of Command Line
   */

  int EnhancedRouting = 0;
  int AnalyzeLoops = 0;
  string SmNodeName = string("");
  string CablingFile = string("");
  string TopoFile = string("");
  int SmPortNum  = -1;
  int lmc = 0;

  int next_option;
  const char * const short_option = "vheal:c:t:s:p:";
  /*
	 In the array below, the 2nd parameter specified the number
	 of arguments as follows:
	 0: no arguments
	 1: argument
	 2: optional
  */
  const option long_option[] =
	 {
		{	"verbose",	     0,	NULL,	'v'},
		{	"help",		     0,	NULL,	'h'},
		{	"analyze-loops", 0,	NULL,	'a'},
		{	"sm-node",	     1,	NULL,	's'},
		{  "enhanced-route",0,  NULL, 'e'},
		{	"port-num",	     1,	NULL,	'p'},
		{	"lmc",	        1,	NULL,	'l'},
		{	"cables",	     1,	NULL,	'c'},
		{	"topology",	     1,	NULL,	't'},
		{	NULL,		0,	NULL,	 0 }	/* Required at the end of the array */
	 };

  printf("-------------------------------------------------\n");
  do
  {
	 next_option = getopt_long(argc, argv, short_option, long_option, NULL);
	 switch(next_option)
	 {
	 case 'v':
		/*
		  Verbose Mode
		*/
		FabricUtilsVerboseLevel |= FABU_LOG_VERBOSE;
		printf(" Verbose Mode\n");
		break;

	 case 'a':
		/*
		  Analyze Loops
		*/
		AnalyzeLoops = 1;
		printf(" Analyze Credit Loops\n");
		break;

	 case 'e':
		/*
		  Enhanced Routing
		*/
		EnhancedRouting = 1;
		printf(" Use Enhanced Routing\n");
		break;

	 case 's':
		/*
		  Specifies SM Node
		*/
		SmNodeName = string(optarg);
		break;

	 case 'l':
		/*
		  Specifies SM Node
		*/
		lmc = atoi(optarg);
		break;

	 case 'p':
		/*
		  Specifies SM Port Num
		*/
		SmPortNum = atoi(optarg);
		break;

	 case 'c':
		/*
		  Specifies Subnet Cabling file
		*/
		CablingFile = string(optarg);
		break;

	 case 't':
		/*
		  Specifies Subnet Cabling file
		*/
		TopoFile = string(optarg);
		break;

	 case 'h':
		show_help();
		return 0;
		break;

	 case -1:
		break; /* done with option */
	 default: /* something wrong */
		show_usage();
		exit(1);
	 }
  }
  while(next_option != -1);

  if (!(CablingFile.size() || TopoFile.size()) || !SmNodeName.size() || 0 > SmPortNum) {
	 printf("-E- Missing some mandatory arguments.\n");
	 show_usage();
	 exit(1);
  }

  printf(" Fabric Simulation:\n");
  if (CablingFile.size())
  {
    printf(" Cabling File ... %s\n", CablingFile.c_str());
  } else {
    printf(" Topology File .. %s\n", TopoFile.c_str());
  }

  printf(" SM Node ........ %s\n", SmNodeName.c_str());
  printf(" SM Port ........ %u\n", SmPortNum);
  printf(" LMC ............ %u\n", lmc);
  printf("-------------------------------------------------\n");

  IBFabric fabric;

  if (CablingFile.size()) {
    if (fabric.parseCables(CablingFile)) {
      cout << "-E- Fail to parse cables file:" << CablingFile << endl;
      exit(1);
    }
  } else {
    if (fabric.parseTopology(TopoFile)) {
      cout << "-E- Fail to parse topology file:" << TopoFile << endl;
      exit(1);
    }
  }

	// get the SM Port
	IBNode *p_smNode = fabric.getNode(SmNodeName);
	if (! p_smNode ) {
	  cout << "-E- Fail to find SM node:" << SmNodeName << endl;
	  exit(1);
	}

	IBPort *p_smPort = p_smNode->getPort(SmPortNum);
	if (! p_smPort) {
	  cout <<  "-E- Fail to find SM Port: " << SmNodeName
			 << "/" << SmPortNum << endl;
	  exit(1);
	}

	// assign lids
	if (SubnMgtAssignLids(p_smPort,lmc)) {
	  cout << "-E- Fail to assign LIDs." << endl;
	  exit(1);
	}

	// propagate lid hop count matrix:
	if (SubnMgtCalcMinHopTables(&fabric)) {
	  cout << "-E- Fail to update Min Hops Tables." << endl;
	  exit(1);
	}

	if (!EnhancedRouting) {

	  if (SubnMgtOsmRoute(&fabric)) {
		 cout << "-E- Fail to update LFT Tables." << endl;
		 exit(1);
	  }
	} else {
	  if (SubnMgtOsmEnhancedRoute(&fabric)) {
		 cout << "-E- Fail to update LFT Tables." << endl;
		 exit(1);
	  }
	}

	if (SubnMgtVerifyAllCaToCaRoutes(&fabric)) {
	  cout << "-E- Some Point to Point Traversals Failed." << endl;
	  exit(1);
	}

	if (AnalyzeLoops) {
     list <IBNode *> rootNodes;

     rootNodes = SubnMgtFindRootNodesByMinHop(&fabric);
     if (!rootNodes.empty()) {
       cout << "-I- Recognized " << rootNodes.size() << " root nodes:" << endl;
       for (list <IBNode *>::iterator nI = rootNodes.begin();
            nI != rootNodes.end(); nI++) {
         cout << " " << (*nI)->name << endl;
       }
       cout << "-----------------------------------------" << endl;

       // rank the fabric by these roots
       map_pnode_int nodesRank;
       SubnRankFabricNodesByRootNodes(&fabric, rootNodes, nodesRank);

       // report non up down paths:
       SubnReportNonUpDownCa2CaPaths(&fabric,nodesRank);

     } else {
       cout << "-I- Fail to recognize any root nodes. Using full credit loop check." << endl;
       CrdLoopAnalyze(&fabric);
     }
   }

  exit(0);
}
