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
 * IB Management Simulator
 *
 * Interface File for TCL SWIG
 *
 */

%title "IB Management Simulator - TCL Extension"

//
// FIX OF SWIG TO SUPPORT NAME ALTERNATE MANGLING
//
%include "ibdm.i"

%module ibms
%{
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <ibdm/Fabric.h>
#include <errno.h>
#include "sim.h"
#include "node.h"
#include "randmgr.h"

# if __WORDSIZE == 64
#  define __PRI64_PREFIX   "l"
#  define __PRIPTR_PREFIX  "l"
# else
#  define __PRI64_PREFIX   "ll"
#  define __PRIPTR_PREFIX
# endif
#ifndef PRIx64
# define PRIx64         __PRI64_PREFIX "x"
#endif
%}

%{
  /* GLOBALS */
  static char ibms_tcl_error_msg[1024];
  static int  ibms_tcl_error;

  static IBMgtSim Simulator;

  /*
     As we do not want to expose our own objects we
     rely on ibdm objects mapping. All IBMSNode calls are
     then mapped to their Nodes.
  */

  /* forward declarations for ibdm functions */
  int ibdmGetFabricIdxByPtr(IBFabric *p_fabric);
  IBFabric *ibdmGetFabricPtrByIdx(unsigned int idx);
  int ibdmGetObjTclNameByPtr(Tcl_Obj *objPtr, void *ptr, char *type);
  int ibdmGetObjPtrByTclName(Tcl_Obj *objPtr, void **ptr);

  /* Given the Object Pointer and Type provide it's TCL name */
  int ibmsGetSimNodeTclNameByPtr(Tcl_Obj *objPtr, void *ptr, char *type) {
	 char tclName[128];
	 char name[128];
	 IBFabric *p_fabric;
	 string uiType;

	 if (!strcmp(type, "IBMSNode *")) {
		IBNode *p_node = ((IBMSNode *)ptr)->getIBNode();
		p_fabric = p_node->p_fabric;
		sprintf(name, ":%s", p_node->name.c_str());
		uiType = "simnode";
	 } else {
		sprintf(tclName, "-E- Unrecognized Object Type:%s", type);
		Tcl_SetStringObj(objPtr, tclName, -1);
		return TCL_ERROR;
	 }

	 /* get the fabric index */
	 int idx = ibdmGetFabricIdxByPtr(p_fabric);
	 if (idx == 0) {
		Tcl_SetStringObj(objPtr, "-E- Fail to find fabric by ptr", -1);
		return TCL_ERROR;
	 }

	 sprintf(tclName, "%s:%u%s", uiType.c_str(), idx, name);
	 Tcl_SetStringObj(objPtr, tclName, -1);
	 return TCL_OK;
  }

  /* Given the Object TCL Name Get it's pointer */
  int ibmsGetSimNodePtrByTclName(Tcl_Obj *objPtr, void **ptr) {
	 /* we need to parse the name and get the type etc. */
	 char buf[256];
	 char *type, *name = 0, *fabIdxStr;
	 char *colonIdx, *slashIdx;
	 int fabricIdx;
	 *ptr = NULL;

	 strcpy(buf, Tcl_GetStringFromObj(objPtr,0));

	 /* the format is always: <type>:<idx>[:<name>] */

	 /* first separate the type */
	 colonIdx = index(buf,':');
	 if (!colonIdx) {
		printf("-E- Bad formatted (no :) ibdm object:%s\n", buf);
		return TCL_ERROR;
	 }
	 *colonIdx = '\0';

	 type = buf;
	 fabIdxStr = ++colonIdx;

	 /* now separate the fabric section if type is not fabric */
	 if (strcmp(type, "fabric")) {
		slashIdx = index(fabIdxStr,':');
		if (!slashIdx) {
		  printf( "-E- Bad formatted ibdm fabric object:%s\n",
					 Tcl_GetStringFromObj(objPtr,0));
		  return TCL_ERROR;
		}
		*slashIdx = '\0';
		name = ++slashIdx;
	 }

	 /* OK so now get the fabric pointer */
	 fabricIdx = atoi(fabIdxStr);

	 IBFabric *p_fabric = ibdmGetFabricPtrByIdx(fabricIdx);
	 if (! p_fabric) {
		*ptr = NULL;
		return TCL_ERROR;
	 }

    if (!strcmp(type, "simnode")) {
		IBNode *p_node = p_fabric->getNode(string(name));
      if (!p_node) {
		  printf("-E- Fail to get node:%s\n", name);
		  return TCL_ERROR;
		}
      IBMSNode *pSimNode = ibmsGetIBNodeSimNode(p_node);
		if (! pSimNode) {
		  printf("-E- Fail to get node:%s\n", name);
		  return TCL_ERROR;
		}
		*ptr = pSimNode;
	 } else {
		printf("-E- Unrecognized Object Type:%s\n", type);
		return TCL_ERROR;
	 }
	 return TCL_OK;
  }
%}


//
// exception handling wrapper based on the MsgMgr interfaces
//

// it assumes we do not send the messages to stderr
%except(tcl8) {
  ibms_tcl_error = 0;
  $function;
  if (ibms_tcl_error) {
	 Tcl_SetStringObj(Tcl_GetObjResult(interp), ibms_tcl_error_msg, -1);
 	 return TCL_ERROR;
  }
}

//
// TYPE MAPS:
//

// Convert a TCL Object to C++ world.
%typemap(tcl8,in) IBMSNode * {

  void *ptr;
  if (ibmsGetSimNodePtrByTclName($source, &ptr) != TCL_OK) {
	 char err[128];
	 sprintf(err, "-E- fail to find ibdm obj by id:%s",Tcl_GetString($source) );
	 // Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
  }

  $target = ($type)ptr;
}

// Convert C++ pointer to TCL
%typemap(tcl8,out) IBMSNode * {
  if ($source)
	 ibmsGetSimNodeTclNameByPtr($target, $source, "$type");
}

%typemap(tcl8,check)  IBMSNode * {
  /* the format is always: <type>:<idx>[:<name>] */

  // get the type from the given source
  char buf[128];
  strcpy(buf, Tcl_GetStringFromObj($source,0));
  char *colonIdx = index(buf,':');
  if (!colonIdx) {
	 char err[128];
	 sprintf(err, "-E- Bad formatted ibdm object:%s", buf);
	 Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
  }
  *colonIdx = '\0';

  if (!strcmp("$basetype", "IBMSNode ")) {
    if (strcmp(buf, "simnode")) {
      char err[256];
      sprintf(err, "-E- basetype is $basetype but received obj of type %s", buf);
      Tcl_SetStringObj(tcl_result, err, strlen(err));
      return TCL_ERROR;
    }
  } else {
	 char err[256];
	 sprintf(err, "-E- basetype '$basetype' is unknown");
	 Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
  }
}

/* we describe a port err profile as a record of key value pairs */
%typemap(tcl8,in) IBMSPortErrProfile *RefIn (IBMSPortErrProfile tmp) {
  if (sscanf(Tcl_GetStringFromObj($source,0),
             "-drop-rate-avg %g -drop-rate-var %g",
             &tmp.packetDropRate,
             &tmp.packetDropRateVar) != 2)
  {
	 char err[256];
	 sprintf(err, "-E- bad format for IBMSPortErrProfile:%s",
            Tcl_GetStringFromObj($source,0));
    Tcl_SetStringObj(tcl_result, err, strlen(err));
    return TCL_ERROR;
  }

  $target = &tmp;
}

%typemap(tcl8,out)  IBMSPortErrProfile *(IBMSPortErrProfile tmp) {
  char buff[128];
  if ($source) {
    sprintf(buff, "-drop-rate-avg %g -drop-rate-var %g",
            $source->packetDropRate,
            $source->packetDropRateVar);
    Tcl_SetStringObj(tcl_result, buff, strlen(buff));
  }
}

%typemap(tcl8,ignore) IBMSPortErrProfile *OUTPUT(IBMSPortErrProfile temp) {
  $target = &temp;
}

%typemap(tcl8,argout)  IBMSPortErrProfile *OUTPUT {
    /* argout */
  char buff[128];
  if ($source) {
    sprintf(buff, "-drop-rate-avg %g -drop-rate-var %g",
            $source->packetDropRate,
            $source->packetDropRateVar);
    Tcl_SetStringObj(tcl_result, buff, strlen(buff));
  }
}

%include inttypes.i
%include ib_types.i

///////////////////////////////////////////////////////////////////////////////
%section "IBMgtSim Constants"
/* These constants are provided by IBMgtSim */

%subsection "Massage Manager Log Verbosity Flags",before,pre
/* To be or'ed and used as the value of setVerbLevel */
%readonly
const int MsgShowFatal  = 0x01;
const int MsgShowError  = 0x02;
const int MsgShowWarning= 0x04;
const int MsgShowInfo   = 0x08;
const int MsgShowVerbose= 0x10;
const int MsgShowContext= 0x20;
const int MsgShowSource = 0x40;
const int MsgShowTime   = 0x80;
const int MsgShowModule = 0x100;
const int MsgShowMads   = 0x200;
const int MsgShowFrames = 0x400;
const int MsgShowAll    = 0xffff;
const int MsgDefault    = 0x62f;
%readwrite

%{
static string MsgAllModules("");
 %}

%section "Message Manager Objects",pre

class msgManager {
/* This is the Message Manager class */
 public:
  int getVerbLevel(string module = MsgAllModules);
  int clrVerbLevel(string module  = MsgAllModules);
  void setVerbLevel(int vl, string module = MsgAllModules);
  // we will provide a wrapper method giving file name ...
  // void setOutStream(std::ostream * o) {outStreamP = o;};

  // get number of outstanding messages of the given severity
  int outstandingMsgCount(int vl = MsgShowFatal | MsgShowError);

  // get all outstanding messages
  string outstandingMsgs(int vl = MsgShowFatal | MsgShowError);

  // return the next message string
  string getNextMessage();

  // null the list of outstanding messages:
  void nullOutstandingMsgs();

};

%addmethods msgManager {
  int setLogFile(char *fileName) {
    std::ofstream *pos = new ofstream(fileName);
    if (pos)
    {
      self->setOutStream(pos);
      return 0;
    }
    else
      return 1;
  }
};

%section "Random Manager Functions",pre
%{
  float rmRand() {
    return RandMgr()->random();
  }

  int rmSeed(int seed) {
    return RandMgr()->setRandomSeed(seed);
  }
%}

float rmRand();
/* obtain a random number in the range 0.0 - 1.0 */

int rmSeed(int seed);
/* initialize the seed for the random manager */

///////////////////////////////////////////////////////////////////////////////

%section "IBMgtSim Simulator Objects",pre
class IBMgtSim {

 public:

  /* access function */
  IBFabric *getFabric() { return pFabric;};
  IBMSServer *getServer() { return pServer; };
  IBMSDispatcher *getDispatcher() { return pDispatcher; };

  /* Initialize the fabric server and dispatcher */
  // We can not expose this method as we want to register the created fabric
  // in the ibdm_fabrics. So we provide our own wrapper for it...
  //   int init(string topoFileName, int serverPortNum, int numWorkers);
};

%addmethods IBMgtSim {
  int init(string topoFileName, int serverPortNum, int numWorkers) {
    int res =
      self->init(topoFileName, serverPortNum, numWorkers);
    if (! res)
      ibdm_fabrics.push_back(self->getFabric());
    return res;
  };
};

///////////////////////////////////////////////////////////////////////////////
%apply IBMSPortErrProfile *OUTPUT {IBMSPortErrProfile &errProfileOut};
%apply IBMSPortErrProfile *RefIn {IBMSPortErrProfile &errProfileIn};
%apply ib_mft_table_t *OUTPUT {ib_mft_table_t *outMftBlock};

/* Every IB node have this simulator node attached */
class IBMSNode {
 public:

  IBNode *getIBNode() {return pNode;};
  /* get the IBNode of the IBMSNode */

  int getLinkStatus(uint8_t outPortNum);
  /* get the link status of the given port */

  int setPhyPortErrProfile(uint8_t portNum, IBMSPortErrProfile &errProfileIn);
  /* set a particular port err profile */

  int getPhyPortErrProfile(uint8_t portNum, IBMSPortErrProfile &errProfileOut);
  /* get a particular port err profile */

  int setPhyPortPMCounter(uint8_t portNum, uint32_t counterSelect,
                          ib_pm_counters_t &countersVal);
  /* set a specific port counter */

  ib_pm_counters_t *
    getPhyPortPMCounter(uint8_t portNum, uint32_t counterSelect);
  /* get a specific port counter */

  ib_port_info_t * getPortInfo(uint8_t portNum);
  /* get a specific port info */

  int setLinkStatus(uint8_t portNum, uint8_t newState);
  /* set the Link status including sending trap128 */

  ib_node_info_t * getNodeInfo();
  /* get the node info */

  ib_switch_info_t *getSwitchInfo();
  /* get the switch info */

  ib_guid_info_t *getGuidInfoBlock(uint8_t portNum, uint16_t blockNum);
  /* get GuidInfo table block */

  int setGuidInfoBlock(uint8_t portNum, uint16_t blockNum, ib_guid_info_t *tbl);
  /* set GuidInfo table block */

  ib_pkey_table_t *getPKeyTblBlock(uint8_t portNum, uint16_t blockNum);
  /* get pkey table block */

  int setPKeyTblBlock(uint8_t portNum, uint16_t blockNum, ib_pkey_table_t *tbl);
  /* set pkey table block */

  ib_vl_arb_table_t * getVLArbLTable(uint8_t portNum, uint8_t blockIndex);
  /* get a specific VLArb Table */

  int setVLArbLTable(uint8_t portNum, uint8_t blockIndex, ib_vl_arb_table_t *tbl);
  /* set a specific SL2VL Table */

  ib_slvl_table_t * getSL2VLTable(uint8_t inPortNum, uint8_t outPortNum);
  /* get a specific SL2VL Table */

  int setSL2VLTable(uint8_t inPortNum, uint8_t outPortNum, ib_slvl_table_t *tbl);
  /* set a specific SL2VL Table */

  int setCrSpace(uint32_t startAddr,uint32_t length,uint32_t data[] );
  /* set CR Space Value */

  int getCrSpace(uint32_t startAddr,uint32_t length,uint32_t data[] );
  /* get CR Space Value */

  int getMFTBlock(uint16_t blockIdx, uint8_t portIdx, ib_mft_table_t *outMftBlock);
  /* get MFT block */

  int setMFTBlock(uint16_t blockIdx, uint8_t portIdx, ib_mft_table_t *inMftBlock);
  /* set MFT block */
};

%include mads.i

%{
  void ibmssh_exit(ClientData clientData ) {

  }
%}

extern char * ibmsSourceVersion;

//
// INIT CODE
//
%init %{

  /* mixing declarations .... */
  {
	 Tcl_PkgProvide(interp,"ibms", "1.0");
#ifdef OSM_BUILD_OPENIB
    Tcl_CreateExitHandler(ibmssh_exit, NULL);
#endif

	 // Register the objects for alternate mangling
    SWIG_AlternateObjMangling["_IBMSNode_p"] = &ibmsGetSimNodeTclNameByPtr;
    SWIG_AlternateNameToObj  ["_IBMSNode_p"] = &ibmsGetSimNodePtrByTclName;

	 // Register the objects for alternate mangling
    SWIG_AlternateObjMangling["_ib_node_info_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_node_info_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_switch_info_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_switch_info_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_port_info_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_port_info_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_node_desc_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_node_desc_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_lft_record_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_lft_record_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_pm_counters_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_pm_counters_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_vl_arb_table_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_vl_arb_table_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_slvl_table_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_slvl_table_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_guid_info_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_guid_info_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    SWIG_AlternateObjMangling["_ib_pkey_table_t_p"] = &ibmsGetIBStructObjNameByPtr;
    SWIG_AlternateNameToObj  ["_ib_pkey_table_t_p"] = &ibmsGetIBStructObjPtrByTclName;

    // declare the simulator object :
    Tcl_CreateObjCommand(interp,"IBMgtSimulator",
								 TclIBMgtSimMethodCmd,
								 (ClientData)&Simulator, 0);

    // declare the message manager
    Tcl_CreateObjCommand(interp,"MsgMgr",
								 TclmsgManagerMethodCmd,
								 (ClientData)&msgMgr(), 0);

  }

%}
