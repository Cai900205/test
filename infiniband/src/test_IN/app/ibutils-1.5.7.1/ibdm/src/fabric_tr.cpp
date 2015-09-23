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
#include "TraceRoute.h"

#include <getopt.h>

static char FabricTrUsage[] =
"Usage: ibdmtr [-v][-h] {-c <cbaling file>|-t <topo file>} -s <sm node name> "
" -p <sm port num> -d <comma-sep-dr-path>";

void
show_usage() {
  cout << FabricTrUsage << endl;
}

void
show_help() {
  cout << "\n"
       << " Fabric Trace Route\n"
       << "--------------------\n"
       << "\n"
       << FabricTrUsage << "\n"
       << "\n"
       << "Description:\n"
       << "  This utility parses a cabling list or topology file\n"
       << " describing the systems connections that make a fabric.\n"
       << " Then it start following the direct route provided and\n"
       << " print out the systems and nodes on the route.\n"
       << "\n"
       << "Arguments:\n"
       << "  -t|--topology <file> = Topology file.\n"
       << "   The format is defined in the IBDM user manual.\n"
       << "  -c|--cables <file> = Cabling list file. Following the line format:\n"
       << "   <Sys1Type> <Sys1Name> <Port1Name> <Sys2Type> <Sys2Name> <Port2Name>\n"
       << "  -s|--sm-node <name> = The name of the SM node (not system). E.g. OSM/U1.\n"
       << "  -p|--port-num <num> = The number of the port SM is connected to.\n"
       << "  -d|--dr-path <comm-sep-dr-path> = a list of ports to go out through\n"
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

  string SmNodeName = string("");
  string CablingFile = string("");
  string TopoFile = string("");
  char   drPathStr[1024];
  char   *p_drPathStr  = NULL;
  int SmPortNum  = -1;

  int next_option;
  const char * const short_option = "vheal:c:t:s:p:d:";
  /*
    In the array below, the 2nd parameter specified the number
    of arguments as follows:
    0: no arguments
    1: argument
    2: optional
  */
  const option long_option[] =
    {
      {  "verbose",       0,  NULL, 'v'},
      {  "help",          0,  NULL, 'h'},
      {  "sm-node",       1,  NULL, 's'},
      {  "port-num",      1,  NULL, 'p'},
      {  "dr-path",       1,  NULL, 'd'},
      {  "cables",        1,  NULL, 'c'},
      {  "topology",      1,  NULL, 't'},
      {  NULL,    0, NULL,  0 }  /* Required at the end of the array */
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
        Specifies SM Node
      */
      SmNodeName = string(optarg);
      break;

    case 'd':
      /*
        Specifies SM Node
      */
      strcpy(drPathStr, optarg);
      p_drPathStr = &(drPathStr[0]);
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

  if (!(CablingFile.size() || TopoFile.size()) || !SmNodeName.size() ||
      0 > SmPortNum || ! p_drPathStr) {
    printf("-E- Missing some mandatory arguments.\n");
    show_usage();
    exit(1);
  }

  printf(" Fabric Trace Route:\n");
  if (CablingFile.size())
  {
    printf(" Cabling File ... %s\n", CablingFile.c_str());
  }
  else
  {
    printf(" Topology File .. %s\n", TopoFile.c_str());
  }

  printf(" SM Node ........ %s\n", SmNodeName.c_str());
  printf(" SM Port ........ %u\n", SmPortNum);
  printf(" DR Path ........ %s\n", p_drPathStr);
  printf("-------------------------------------------------\n");

  IBFabric fabric;

  if (CablingFile.size())
  {
    if (fabric.parseCables(CablingFile))
    {
      cout << "-E- Fail to parse cables file:" << CablingFile << endl;
      exit(1);
    }
  }
  else
  {
    if (fabric.parseTopology(TopoFile))
    {
      cout << "-E- Fail to parse topology file:" << TopoFile << endl;
      exit(1);
    }
  }

  // get the SM Node
  IBNode *p_smNode = fabric.getNode(SmNodeName);
  if (! p_smNode )
  {
    cout << "-E- Fail to find SM node:" << SmNodeName << endl;
    if (FabricUtilsVerboseLevel | FABU_LOG_VERBOSE)
      fabric.dump(cout);
    exit(1);
  }

  // get the SM Port
  IBPort *p_smPort = p_smNode->getPort(SmPortNum);
  if (! p_smPort)
  {
    cout <<  "-E- Fail to find SM Port: " << SmNodeName << "/" << SmPortNum << endl;
    exit(1);
  }

  // Convert the DR arg to array of integers
  list_int drPathPortNums;
  char *p_pos;
  while ((p_pos = strchr(p_drPathStr,','))) {
    *p_pos = '\0';
    drPathPortNums.push_back(atoi(p_drPathStr));
    p_drPathStr = ++p_pos;
  }
  drPathPortNums.push_back(atoi(p_drPathStr));

  TraceDRPathRoute(p_smPort, drPathPortNums);

  exit(0);
}
