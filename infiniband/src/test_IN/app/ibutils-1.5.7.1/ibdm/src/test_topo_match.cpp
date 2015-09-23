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

Fabric Topology Matcher Test Program

Reads a Topology file and OpenSM subnet file, try to match and report errors.

*/

#include <Fabric.h>
#include <SubnMgt.h>
#include <CredLoops.h>
#include <TopoMatch.h>
#include <getopt.h>

static char TopoMatchTestUsage[] =
"Usage: ibtopodiff [-v][-h]"
  " -t <topo file> -d <subnet file> {-e | -s <start node name> -p <start port num> -g <start port guid>}" ;

void
show_usage() {
  cout << TopoMatchTestUsage << endl;
}

void
show_help() {
  cout << "\n"
       << " Fabric Topology Matcher\n"
       << "-------------------------------\n"
       << "\n"
       << TopoMatchTestUsage << "\n"
       << "\n"
       << "Description:\n"
       << "This tool compares a topology file and a discovered listing of"
	 << "subnet.lst/ibdiagnet.lst and reports missmatches.\n"
	 << "Two different algorithms provided:\n"
	 << "Using the -e option is more suitible for MANY missmatches\n"
	 << "it applies less heuristics and provide details about the match.\n"
	 << "Providing the -s, -p and -g starts a detailed heuristics that\n"
	 << "should be used when only small number of changes are expected.\n"
       << "\n"
       << "Arguments:\n"
       << "  -t|--topology <file> = Topology file [ibdm.topo].\n"
       << "   The format is defined in the IBDM user manual.\n"
       << "  -d|--discovered <file> = [subnet.lst] file produced by OpenSM.\n"
	 << "  -e|--edge = start processing from the edges using strict match.\n"
       << "  -s|--start-node <name> = The name of the start node [H-1/U1].\n"
       << "  -p|--port-num <num> = The number of the start port [1].\n"
       << "  -g|--port-guid <guid> = The guid of the start port [none].\n"
       << "\n"
       << "Options:\n"
       << "  -v|--verbose = verbsoe mode\n"
       << "  -h|--help = provide this help message\n"
       << "\n"
       << "\n"
       << "Author: Eitan Zahavi, Mellanox Technologies LTD.\n"
       << endl;
}

int main (int argc, char **argv) {
  /*
   * Parseing of Command Line
   */

  const char * startSystemName = "H-1";
  const char * topoFileName = "ibdm.topo";
  const char * subnetFileName = "subnet.lst";
  int startPortNum = 1;
  uint64_t startPortGuid = 0;
  int runEdgeMode = 0;
  char *p_diagnostics;
  int noMatch = 0;
  int next_option;
  const char * const short_option = "vhs:p:g:d:t:e";
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
		{	"help",	     0,	NULL,	'h'},
		{	"start-system",  1,	NULL,	's'},
		{	"port-num",	     1,	NULL,	'p'},
		{	"port-guid",     1,	NULL,	'g'},
		{	"discovered",    1,	NULL,	'd'},
		{	"topology",	     1,	NULL,	't'},
		{	"edge",          0,	NULL,	'e'},
		{	NULL,		     0,	NULL,	 0 }	/* Required at the end of the array */
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

	 case 's':
		/*
		  Specifies Start System
		*/
		startSystemName = optarg;
		break;

	 case 'd':
		/*
		  Specifies Discovered subnet.lst file name
		*/
		subnetFileName = optarg;
		break;

	 case 'p':
		/*
		  Specifies Start Port Num
		*/
		startPortNum = atoi(optarg);
		break;

	 case 'g':
		/*
		  Specifies Start Port Guid
		*/
		startPortGuid = strtoull(optarg, NULL, 16);
		break;

	 case 't':
		/*
		  Specifies Subnet Cabling file
		*/
		topoFileName = optarg;
		break;

	 case 'e':
		/*
		  Specifies Subnet Cabling file
		*/
		runEdgeMode = 1;
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

  if (!runEdgeMode) {
    if (!startPortGuid) {
	printf("-E- Missing -g/--port-guid mandatory argument.\n");
	show_usage();
	exit(1);
    }
  }

  printf(" Fabric Topology Match:\n");
  printf(" Topology File .. %s\n", topoFileName);
  printf(" Subnet File .... %s\n", subnetFileName);
  if (!runEdgeMode) {
    printf(" Running detailed match heuristics.\n");
    printf(" Start Node ..... %s\n", startSystemName);
    printf(" Start Port ..... %u\n", startPortNum);
    printf(" Start Guid ..... 0x%016Lx\n", startPortGuid);
  } else {
    printf(" Running edge mode match heuristics.\n");
  }
  printf("-------------------------------------------------\n");

  IBFabric *p_dFabric, *p_sFabric;
  IBFabric *p_mFabric;

  p_dFabric = new IBFabric();
  p_sFabric = new IBFabric();
  p_mFabric = new IBFabric();

  if (p_sFabric->parseTopology(topoFileName)) {
    cout << "-E- Fail to parse topology file:" << topoFileName << endl;
    exit(1);
  }
  //s_fabric.dump(cout);

  if (p_dFabric->parseSubnetLinks(subnetFileName)) {
    cout << "-E- Fail to parse subnet file:" << subnetFileName << endl;
    exit(1);
  }

  if (!runEdgeMode) {
    TopoMatchFabrics(p_sFabric, p_dFabric, startSystemName, startPortNum,
			   startPortGuid, &p_diagnostics);
  } else {
    TopoMatchFabricsFromEdge(p_sFabric, p_dFabric, &p_diagnostics);
  }

  if (p_diagnostics)
  {
    printf("---- Fabrics Topologies Differ ----\n");
    printf("%s\n", p_diagnostics);
    printf("-----------------------------------\n");
    free(p_diagnostics);
  } else {
    printf("---- Fabrics Topologies Match ----\n");
  }

  if (TopoMergeDiscAndSpecFabrics( p_sFabric, p_dFabric, p_mFabric)) {
    printf("-E- Fail to merge fabrics.\n");
    exit(1);
  }

  p_mFabric->dumpNameMap("ibtopodiff.names");

  if ( FabricUtilsVerboseLevel & FABU_LOG_VERBOSE)
    p_mFabric->dump(cout);

  delete p_mFabric;

  delete p_sFabric;

  delete p_dFabric;

  exit(0);
}
