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
 * IB Fabric Data Model (and Utilities)
 *
 * Data Model Interface File for TCL SWIG
 *
 */

%title "IB Fabric Data Model - TCL Extention"

%module ibdm
%{
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <sstream>
#include "Fabric.h"
#include "SubnMgt.h"
#include "CredLoops.h"
#include "TraceRoute.h"
#include "TopoMatch.h"
#include "Congestion.h"

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
  static char ibdm_tcl_error_msg[1024];
  static int  ibdm_tcl_error;
  static vector< IBFabric *> ibdm_fabrics;
  static IBLinkWidth UnknownLinkWidth = IB_UNKNOWN_LINK_WIDTH;
  static IBLinkSpeed UnknownLinkSpeed = IB_UNKNOWN_LINK_SPEED;
  static IBLinkWidth DefaultLinkWidth = IB_LINK_WIDTH_4X;
  static IBLinkSpeed DefaultLinkSpeed = IB_LINK_SPEED_2_5;

  /*
	  MAPPING IBDM OBJECTS TO TCL and BACK:
	  The idea is that we have specifc rules for naming
	  Node, Port, System and SystemPort for a specific Fabric.

	  All Fabrics are stored by id in a global vector.

	  So the object names will follow:
	  <type>:<fabricIdx>/<name>

  */

  /* Given a fabric pointer return its idx (starting w 1) or 0 */
  int ibdmGetFabricIdxByPtr(IBFabric *p_fabric) {
	 /* go over all fabrics and find it's index: */
	 for (unsigned int i = 0; i < ibdm_fabrics.size(); i++) {
		if (ibdm_fabrics[i] == p_fabric) {
		  return(i+1);
		}
	 }
	 return(0);
  }

  /* Given a fabric idx return it's pointer */
  /* Note the index is 1-N and thus we need to -1 it before access */
  IBFabric *ibdmGetFabricPtrByIdx(unsigned int idx) {
	 if ((idx > ibdm_fabrics.size()) || (idx < 1)) {
		return NULL;
	 }
	 return ibdm_fabrics[idx - 1];
  }

  /*
	 we provide our own constructor such that all IBFabrics are
	 registered in the global vector;
  */
  IBFabric *new_IBFabric(void) {
	 IBFabric *p_fabric = new IBFabric();
    unsigned int i;
	 if (p_fabric) {
      /* look for an open index in the vector of fabrics */
      for (i = 0; i < ibdm_fabrics.size(); i++)
      {
        if (ibdm_fabrics[i] == NULL)
        {
          ibdm_fabrics[i] = p_fabric;
          return p_fabric;
        }
      }
      ibdm_fabrics.push_back(p_fabric);
	 }
	 return p_fabric;
  }

  /*
	 we provide our own destructor such that the deleted fabric is
    de-registered from the global fabrics vector
  */
  void delete_IBFabric(IBFabric *p_fabric) {
    int idx = ibdmGetFabricIdxByPtr(p_fabric);
    if (! idx) {
      printf("ERROR: Fabric idx:%p does not exist in the global vector!\n",
             p_fabric);
    } else {
      ibdm_fabrics[idx-1] = NULL;
    }
    delete p_fabric;
  }

  /* Given the Object Pointer and Type provide it's TCL name */
  int ibdmGetObjTclNameByPtr(Tcl_Obj *objPtr, void *ptr, char *type) {
	 char tclName[128];
	 char name[128];
	 IBFabric *p_fabric;
	 string uiType;

	 if (!strcmp(type, "IBNode *")) {
		IBNode *p_node = (IBNode *)ptr;
		p_fabric = p_node->p_fabric;
		sprintf(name, ":%s", p_node->name.c_str());
		uiType = "node";
	 } else if (!strcmp(type, "IBPort *")) {
		IBPort *p_port = (IBPort *)ptr;
		sprintf(name,":%s/%u", p_port->p_node->name.c_str(), p_port->num);
		p_fabric = p_port->p_node->p_fabric;
		uiType = "port";
	 } else if (!strcmp(type, "IBSystem *")) {
		IBSystem *p_system = (IBSystem *)ptr;
		sprintf(name, ":%s", p_system->name.c_str());
		uiType = "system";
		p_fabric = p_system->p_fabric;
	 } else if (!strcmp(type, "IBSysPort *")) {
		IBSysPort *p_sysPort = (IBSysPort *)ptr;
		sprintf(name, ":%s:%s",  p_sysPort->p_system->name.c_str(),
				  p_sysPort->name.c_str());
		uiType = "sysport";
		p_fabric = p_sysPort->p_system->p_fabric;
	 } else if (!strcmp(type, "IBFabric *")) {
		p_fabric = (IBFabric *)ptr;
		uiType = "fabric";
		name[0] = '\0';
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
  int ibdmGetObjPtrByTclName(Tcl_Obj *objPtr, void **ptr) {
	 /* we need to parse the name and get the type etc. */
	 char buf[256];
	 char *type, *name=0, *fabIdxStr;
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

	 /* now separate the fabric section if tyep is not fabric */
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

	 /* Ok so now get the fabic pointer */
	 fabricIdx = atoi(fabIdxStr);

	 IBFabric *p_fabric = ibdmGetFabricPtrByIdx(fabricIdx);
	 if (! p_fabric) {
		*ptr = NULL;
		return TCL_ERROR;
	 }

	 if (!strcmp(type, "fabric")) {
		*ptr = p_fabric;
	 } else if (!strcmp(type, "node")) {
		IBNode *p_node = p_fabric->getNode(string(name));
		if (! p_node) {
		  printf("-E- Fail to get node:%s\n", name);
		  return TCL_ERROR;
		}
		*ptr = p_node;
	 } else if (!strcmp(type, "port")) {
		slashIdx = rindex(name,'/');
		if (!slashIdx) {
		  printf("-E- Bad formatted ibdm node object:%s\n",
					Tcl_GetStringFromObj(objPtr,0));
		  return TCL_ERROR;
		}
		*slashIdx = '\0';
		int portNum = atoi(++slashIdx);
		IBNode *p_node = p_fabric->getNode(string(name));
		if (! p_node) {
		  printf("-E- Fail to get node:%s\n", name);
		  return TCL_ERROR;
		}
		IBPort *p_port = p_node->getPort(portNum);
		if (! p_port) {
		  printf("-E- Fail to get node:%s port:%u\n",
					 name, portNum);
		  return TCL_ERROR;
		}
		*ptr = p_port;
	 } else if (!strcmp(type, "system")) {
		IBSystem *p_system = p_fabric->getSystem(string(name));
		if (! p_system) {
		  printf("-E- Fail to get system:%s\n", name);
		  return TCL_ERROR;
		}
		*ptr = p_system;
	 } else if (!strcmp(type, "sysport")) {
		/* the format of system port is:  <type>:<idx>:<sys>:<port> */
		colonIdx = index(name,':');
		if (!colonIdx) {
		  printf("-E- Bad formatted ibdm sysport object:%s\n",
					Tcl_GetStringFromObj(objPtr,0) );
		  return TCL_ERROR;
		}
		*colonIdx = '\0';
		IBSystem *p_system = p_fabric->getSystem(string(name));
		if (! p_system) {
		  printf("-E- Fail to get system:%s\n", name);
		  return TCL_ERROR;
		}
		IBSysPort *p_sysPort = p_system->getSysPort(string(++colonIdx));
		if (! p_sysPort) {
		  printf("-E- Fail to get system:%s port:%s\n", name, colonIdx);
		  return TCL_ERROR;
		}
		*ptr = p_sysPort;
	 } else {
		printf("-E- Unrecognized Object Type:%s\n", type);
		return TCL_ERROR;
	 }
	 return TCL_OK;
  }

  int ibdmReportNonUpDownCa2CaPaths(IBFabric *p_fabric, list_pnode rootNodes) {
    map_pnode_int nodesRank;
    if (SubnRankFabricNodesByRootNodes(p_fabric, rootNodes, nodesRank))
    {
      printf("-E- fail to rank the fabric by the given root nodes.\n");
      return(1);
    }
    return( SubnReportNonUpDownCa2CaPaths(p_fabric, nodesRank));
  }

  int ibdmFatTreeRoute(IBFabric *p_fabric, list_pnode rootNodes) {
    map_pnode_int nodesRank;
    if (SubnRankFabricNodesByRootNodes(p_fabric, rootNodes, nodesRank))
    {
      printf("-E- fail to rank the fabric by the given root nodes.\n");
      return(1);
    }
    return( SubnMgtFatTreeRoute(p_fabric));
  }

  int ibdmCheckFabricMCGrpsForCreditLoopPotential(IBFabric *p_fabric, list_pnode rootNodes) {
    map_pnode_int nodesRank;
    if (SubnRankFabricNodesByRootNodes(p_fabric, rootNodes, nodesRank))
    {
      printf("-E- fail to rank the fabric by the given root nodes.\n");
      return(1);
    }
    return( SubnMgtCheckFabricMCGrpsForCreditLoopPotential(p_fabric, nodesRank));
  }

  int ibdmRankFabricByRoots(IBFabric *p_fabric, list_pnode rootNodes) {
    map_pnode_int nodesRank;
    if (SubnRankFabricNodesByRootNodes(p_fabric, rootNodes, nodesRank))
    {
      printf("-E- fail to rank the fabric by the given root nodes.\n");
      return(1);
    }
    return(0);
  }

%}


//
// exception handling wrapper based on the MsgMgr interfaces
//

// it assumes we do not send the messages to stderr
%except(tcl8) {
  ibdm_tcl_error = 0;
  $function;
  if (ibdm_tcl_error) {
	 Tcl_SetStringObj(Tcl_GetObjResult(interp), ibdm_tcl_error_msg, -1);
	 return TCL_ERROR;
  }
}

//
// TYPE MAPS:
//
%include typemaps.i

// Convert a TCL Object to C++ world.
%typemap(tcl8,in) IBFabric *, IBNode *, IBSystem *, IBPort *, IBSysPort * {

  void *ptr;
  if (ibdmGetObjPtrByTclName($source, &ptr) != TCL_OK) {
	 char err[128];
	 sprintf(err, "-E- fail to find ibdm obj by id:%s",Tcl_GetString($source) );
	 // Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
  }

  $target = ($type)ptr;
}

// Convert C++ pointer to TCL
%typemap(tcl8,out) IBFabric *, IBNode *, IBSystem *, IBPort *, IBSysPort * {
  if ($source)
	 ibdmGetObjTclNameByPtr($target, $source, "$type");
}

%typemap(tcl8,check)  IBFabric *, IBNode *, IBSystem *, IBPort *, IBSysPort * {
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

  if (!strcmp("$basetype", "IBFabric ")) {
	if (strcmp(buf, "fabric")) {
	 char err[256];
	 sprintf(err, "-E- basetype is $basetype but received obj of type %s", buf);
	 Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
	}
  } else if (!strcmp("$basetype", "IBSystem ")) {
	if (strcmp(buf, "system")) {
	 char err[256];
	 sprintf(err, "-E- basetype is $basetype but received obj of type %s", buf);
	 Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
	}
  } else if (!strcmp("$basetype", "IBSysPort ")) {
	if (strcmp(buf, "sysport")) {
	 char err[256];
	 sprintf(err, "-E- basetype is $basetype but received obj of type %s", buf);
	 Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
	}
  } else if (!strcmp("$basetype", "IBNode ")) {
	if (strcmp(buf, "node")) {
	 char err[256];
	 sprintf(err, "-E- basetype is $basetype but received obj of type %s", buf);
	 Tcl_SetStringObj(tcl_result, err, strlen(err));
	 return TCL_ERROR;
	}
  } else if (!strcmp("$basetype", "IBPort ")) {
	if (strcmp(buf, "port")) {
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

// representing vec_pport in TCL
%typemap(tcl8, out) vec_pport * {
  Tcl_Obj *p_tclObj;

  for (unsigned int i = 0; i < $source->size(); i++) {
	 IBPort *p_port = (*$source)[i];
	 if (p_port) {
		p_tclObj = Tcl_NewObj();
		if (ibdmGetObjTclNameByPtr(p_tclObj, p_port, "IBPort *")
			 != TCL_OK) {
		  printf("-E- Fail to map Port Object (a Vector element)\n");
		} else {
		  Tcl_AppendElement(interp, Tcl_GetString(p_tclObj));
		}
		Tcl_DecrRefCount(p_tclObj);
	 }
  }
}

%typemap(tcl8, out) vec_vec_byte * {
  for (unsigned int i = 0; i < $source->size(); i++) {
	 Tcl_AppendResult(interp,"{", NULL);
	 for (unsigned int j = 0; j < (*$source)[i].size(); j++) {
		char buf[32];
		sprintf(buf,"%u ", (*$source)[i][j]);
		Tcl_AppendResult(interp, buf, NULL);
	 }
	 Tcl_AppendResult(interp,"} ", NULL);
  }
}

%typemap(tcl8, out) vec_byte * {
  for (unsigned int i = 0; i < $source->size(); i++) {
	 char buf[32];
	 sprintf(buf,"%u ", (*$source)[i]);
	 Tcl_AppendResult(interp, buf, NULL);
  }
}

// representing map_str_psysport in TCL
%typemap(tcl8, out) map_str_psysport * {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  map_str_psysport::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I).second, "IBSysPort *") != TCL_OK) {
		printf("-E- Fail to map SysPort Object (a Vector element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "%s %s", (*I).first.c_str(), Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// representing map_str_psys in TCL
%typemap(tcl8, out) map_str_psys * {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  map_str_psys::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I).second, "IBSystem *") != TCL_OK) {
		printf("-E- Fail to map System Object (a Vector element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "%s %s", (*I).first.c_str(), Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// representing map_str_pnode in TCL
%typemap(tcl8, out) map_str_pnode * {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  map_str_pnode::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I).second, "IBNode *") != TCL_OK) {
		printf("-E- Fail to map Node Object (a Vector element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "%s %s", (*I).first.c_str(), Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// representing map_guid_pport in TCL
%typemap(tcl8, out) map_guid_pport * {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  map_guid_pport::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I).second, "IBPort *") != TCL_OK) {
		printf("-E- Fail to map Port Object (a guid map element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "0x%016" PRIx64 " %s",
              (*I).first, Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// representing map_guid_pnode in TCL
%typemap(tcl8, out) map_guid_pnode * {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  map_guid_pnode::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I).second, "IBNode *") != TCL_OK) {
		printf("-E- Fail to map Node Object (a guid map element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "0x%016" PRIx64 " %s",
              (*I).first, Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// representing map_guid_psystem in TCL
%typemap(tcl8, out) map_guid_psys * {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  map_guid_psys::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I).second, "IBSystem *") != TCL_OK) {
		printf("-E- Fail to map System Object (a guid map element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "0x%016" PRIx64 " %s",
              (*I).first, Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// cast a Tcl String object to string
%typemap(tcl8,in) string * {
  int len;
  static string $target_tmp;
  $target_tmp = string(Tcl_GetStringFromObj($source,&len));
  $target = &$target_tmp;
}

// NOTE FOR SOME REASON THE RETURN TYPE IS TAKEN NOT AS A POINTER
// BUT PASSED AS ONE ?????

// return a char * from a string:
%typemap(tcl8,out) string, string * {
	char ezTmp[1024];
	strcpy(ezTmp, $source->c_str());
	Tcl_SetStringObj(tcl_result, ezTmp, strlen(ezTmp));
}
%{
#define new_string string
%}
// return a char * from a string that was allocated previously
// so we need to delete it
%typemap(tcl8,out) new_string, new_string * {
	char ezTmp[1024];
	strcpy(ezTmp, $source->c_str());
	Tcl_SetStringObj(tcl_result, ezTmp, strlen(ezTmp));
   delete $source;
}

%typemap (tcl8, ignore) char **p_out_str (char *p_c) {
  $target = &p_c;
}

%typemap (tcl8, argout) char **p_out_str {
  if (*$source) {
     Tcl_SetStringObj($target,*$source,strlen(*$source));
      free(*$source);
  } else {
     Tcl_SetStringObj($target,"",-1);
  }
}

/*
  TCL Type Maps for Standard Integer Types
*/

%typemap(tcl8,in) uint64_t *(uint64_t temp) {
  temp = strtoull(Tcl_GetStringFromObj($source,NULL), NULL,16);
  $target = &temp;
}

%typemap(tcl8,out) uint64_t {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,out) uint64_t * {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%{
#define new_uint64_t uint64_t
%}
%typemap(tcl8,out) new_uint64_t *, new_uint64_t {
  char buff[20];
  /* new_uint64_t tcl8 out */
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
  delete $source;
}

%typemap(tcl8,in) uint32_t * (uint32_t temp){
  temp = strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0);
  $target = &temp;
}

%typemap(tcl8,out) uint32_t * {
  char buff[20];
  sprintf(buff, "%u", *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) uint16_t * (uint16_t temp) {
  temp = strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0);
  $target = &temp;
}

%typemap(tcl8,out) uint16_t * {
  char buff[20];
  sprintf(buff, "%u", *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) uint8_t * (uint8_t temp) {
  temp = strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0);
  $target = &temp;
}

%typemap(tcl8,out) uint8_t * {
  char buff[20];
  sprintf(buff, "%u", *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) ib_net64_t *(uint64_t temp) {
  temp = cl_hton64(strtoull(Tcl_GetStringFromObj($source,NULL), NULL, 16));
  $target = &temp;
}

%typemap(tcl8,out) ib_net64_t * {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, cl_ntoh64(*$source));
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) ib_net32_t *(ib_net32_t temp) {
  temp = cl_hton32(strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0));
  $target = &temp;
}

%typemap(tcl8,out) ib_net32_t * {
  char buff[20];
  sprintf(buff, "%u", cl_ntoh32(*$source));
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) ib_net16_t * (ib_net16_t temp) {
  temp = cl_hton16(strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0));
  $target = &temp;
}

%typemap(tcl8,out) ib_net16_t * {
  char buff[20];
  sprintf(buff, "%u", cl_hton16(*$source));
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,argout) uint64_t *OUTPUT {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,ignore) uint64_t *OUTPUT(uint64_t temp) {
  $target = &temp;
}


%typemap(tcl8,argout) ostringstream & {
  Tcl_SetStringObj($target, (char*)$source->str().c_str(),
                   $source->str().size() + 1);
}

%typemap(tcl8,ignore) ostringstream &(ostringstream tempStream) {
  $target = &tempStream;
}

%typemap(tcl8,out) boolean_t * {
  if (*$source) {
	 Tcl_SetStringObj($target,"TRUE", 4);
  } else {
	 Tcl_SetStringObj($target,"FALSE", 5);
  }
}

%typemap(tcl8,in) boolean_t *(boolean_t temp) {
  if (strcmp(Tcl_GetStringFromObj($source,NULL), "TRUE")) {
	 temp = FALSE;
  } else {
	 temp = TRUE;
  }
  $target = &temp;
}

%typemap(tcl8,out) IBLinkWidth * {
  Tcl_SetStringObj($target,width2char(*$source), -1);
}

%typemap(tcl8,in) IBLinkWidth * (IBLinkWidth temp1) {
  temp1 = char2width(Tcl_GetStringFromObj($source,NULL));
  $target = &temp1;
}

%typemap(tcl8,out) IBLinkSpeed * {
  Tcl_SetStringObj($target,speed2char(*$source), -1);
}

%typemap(tcl8,in) IBLinkSpeed * (IBLinkSpeed temp2) {
  temp2 = char2speed(Tcl_GetStringFromObj($source,NULL));
  $target = &temp2;
}
%include typemaps.i

%typemap(tcl8,out) list_pnode *,  list_pnode {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  list_pnode::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;

  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I), "IBNode *") != TCL_OK) {
		printf("-E- Fail to map Node Object (a guid map element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "%s", Tcl_GetString(p_tclObj));
		Tcl_AppendElement(interp, buf);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}

// given a list of node names - generate the list of nodes
%typemap(tcl8,in) list_pnode * (list_pnode tmpNodeList) {
#if TCL_MINOR_VERSION > 3
  const char **sub_lists;
#else
  char **sub_lists;
#endif
  int num_sub_lists;
  int idx;

  /* we will use the TCL split list to split into elements */
  if (Tcl_SplitList(interp,
                    Tcl_GetStringFromObj($source,0),
                    &num_sub_lists, &sub_lists) != TCL_OK) {
    printf("-E- Bad formatted list :%s\n",
           Tcl_GetStringFromObj($source,0));
    return TCL_ERROR;
  }

  for (idx = 0; (idx < num_sub_lists); idx++)
  {
    /* we need to double copy since TCL 8.4 requires split res to be const */
    Tcl_Obj *p_tclObj;
    void *ptr;
    char buf[128];
    strcpy(buf, sub_lists[idx]);

    if (strncmp("node:", buf, 5)) {
      printf("-E- Bad formatted node (%u) object:%s\n", idx, buf);
      return TCL_ERROR;
    }

	 p_tclObj = Tcl_NewObj();
    Tcl_SetStringObj(p_tclObj, buf, -1);
    if (ibdmGetObjPtrByTclName(p_tclObj, &ptr) != TCL_OK) {
      printf("-E- fail to find ibdm obj by id:%s", buf );
      Tcl_DecrRefCount(p_tclObj);
      return TCL_ERROR;
    }
    Tcl_DecrRefCount(p_tclObj);
    tmpNodeList.push_back((IBNode *)ptr);
  }

  $target = &tmpNodeList;
}

//
// INTERFACE DEFINITION (~copy of h file)
//

%section "IBDM Constants"
/* These constants are provided by IBDM: */

typedef enum {IB_UNKNOWN_NODE_TYPE, IB_CA_NODE, IB_SW_NODE} IBNodeType;
/* Node Types */

%subsection "Log Verbosity Flags",before,pre
/* To be or'ed and used as the value of FabricUtilsVerboseLevel */
%readonly
#define FABU_LOG_NONE    0x0
#define FABU_LOG_ERROR   0x1
#define FABU_LOG_INFO    0x2
#define FABU_LOG_VERBOSE 0x4
%readwrite

%section "IBDM Globals"
int FabricUtilsVerboseLevel;
/* Log level: set to FABU_LOG* values  */

int ibdmUseInternalLog();
/* instruct ibdm to use intrernal buffer for log */
int ibdmUseCoutLog();
/* use stdout for log */
%new char *ibdmGetAndClearInternalLog();
/* obtain log messages from internal log and clear it */

%section "IBDM Objects",pre
/* This section decribes the various object types exposed by IBDM. */
%text %{

  IBDM exposes some of its internal objects. The objects
  identifiers returned by the various function calls are formatted
  according to the following rules:
  Fabric: fabric:<idx>
  System: system:<fab idx>:<sys name>
  SysPort: sysport:<fab idx>:<sys name>:<port name>
  Node: node:<fab idx>:<node name>
  Port: port:<fab idx>:<node name>/<port num>

  IBDM Objects are standard Swig-Tcl objects.
  As such they have two flavors for their usage: Variables, Objects.

  Variables/Pointers:
     For each object attribute a "get" and "set" methods are provided.
	  The format of the methods is: <class>_<attribute>_<get|set>.
     The "set" method is only available for read/write attributes.

	  Example:
     set nodes [ibdm_get_nodes]
     set node  [lindex $nodes 0]
     IBNode_numPorts_get $node

  Objects:
     Given an object pointer one can convert it to a Tcl "Object"
	  using the following command:
     <class> <obj_name> -this <obj pointer>

     Once declared the <obj-name> can be used in conjunction to
     with the standard "configure" and "cget" commands.

	  Example (following the previous one):
     IBFabric VaTech -this $fabric
	  VaTech cget -NodeByName

     To delete an object symbol (and enable its mapping to another
     pointer) use:
     rename <obj name> ""
     for example:
     rename VaTech ""
%}

//
// IB Port class.
// This is not the "End Node" but the physical port of
// a node.
//
class IBPort {
 public:
  IBPort    * p_remotePort; // Port connected on the other side of link
  IBSysPort * p_sysPort;    // The system port (if any) connected to
  IBNode    * p_node;       // The node the port is part of.
  int			      num;            // Physical ports are identified by number.
  unsigned int	   base_lid;       // The base lid assigned to the port.
  IBLinkWidth     width;          // The link width of the port
  IBLinkSpeed     speed;          // The link speed of the port
  unsigned int    counter1;       // a generic value to be used by various algorithms

  IBPort(IBNode *p_nodePtr, int number);
  // constructor

  new_uint64_t guid_get();
  void guid_set(uint64_t guid);

  new_string getName();

  void connect (IBPort *p_otherPort,
                IBLinkWidth w = DefaultLinkWidth,
                IBLinkSpeed s = DefaultLinkSpeed);
  // connect the port to another node port

  int disconnect();
  // disconnect the port. Return 0 if successful

};

//
// IB Node class
//
class IBNode {
 public:
  string			   name;      // Name of the node (instance name of the chip)
  IBNodeType		type;      // Either a CA or SW
  uint32_t        devId;     // The device ID of the node
  uint32_t        revId;     // The device revision Id.
  uint32_t        vendId;    // The device Vendor ID.
  string          attributes;// Comma-sep string of arbitrary attributes k=v
  uint8_t         rank;      // The rank of the node in the tree
%readonly
  IBSystem	     *p_system; // What system we belong to
  IBFabric	     *p_fabric; // What fabric we belong to.
  unsigned int	   numPorts;  // Number of physical ports
  vec_pport		   Ports;     // Vector of all the ports
  vec_vec_byte		MinHopsTable; // Table describing minimal hop count through
                                // each port to each target lid
  vec_byte        LFT;          // The LFT of this node (for switches only)
%readwrite
  // void            *p_appData1;  // Application Private Data #1
  // void            *p_appData2;  // Application Private Data #2

  new_uint64_t guid_get();
  void guid_set(uint64_t guid);

  IBNode(string n,
			IBFabric *p_fab,
			IBSystem *p_sys,
			IBNodeType t, int np);
  // Constractor

  ~IBNode();

  inline IBPort *makePort (unsigned int num);
  // create a new port by name if required to

  inline IBPort *getPort(unsigned int num);
  // get a port by number num = 1..N:

  void setHops (IBPort *p_port, unsigned int lid, int hops);
  // Set the min hop for the given port (* is all) lid pair

  int getHops (IBPort *p_port, unsigned int lid);
  // Get the min number of hops defined for the given port or all

  IBPort *getFirstMinHopPort(unsigned int lid);
  // Scan the node ports and find the first port
  // with min hop to the lid

  void setLFTPortForLid (unsigned int lid, unsigned int portNum);
  // Set the Linear Forwarding Table:

  int getLFTPortForLid (unsigned int lid);
  // Get the LFT for a given lid

  void repHopTable();
  // dump out the min hop table of the node

}; // Class IBNode

//
// System Port Class
// The System Port is a front pannel entity.
//
class IBSysPort {
 public:
  string			   name;              // The front pannel name of the port
  IBSysPort	*p_remoteSysPort;  // If connected the other side sys port
  IBSystem	*p_system;         // System it benongs to
  IBPort	   *p_nodePort;       // The node port it connects to.

  IBSysPort(string n, IBSystem *p_sys);
  // Constructor

  ~IBSysPort();

  void connect (IBSysPort *p_otherSysPort,
                IBLinkWidth width = UnknownLinkWidth,
                IBLinkSpeed speed = UnknownLinkSpeed);
  // connect two SysPorts

  int disconnect();
  // disconnect the SysPort (and ports). Return 0 if successful

};

//
// IB System Class
// This is normally derived into a system specific class
//
class IBSystem {
 public:
  string			   name;       // the "host" name of the system
  string			   type;       // what is the type i.e. Cougar, Buffalo etc
  IBFabric        *p_fabric;  // fabric belongs to
%readonly
  map_str_pnode NodeByName;   // Provide the node pointer by its name
  map_str_psysport PortByName;// A map provising pointer to the SysPort by name
%readwrite

  IBSystem(string n, IBFabric *p_fab, string t);
  // Constractor

  ~IBSystem();

  new_uint64_t guid_get();
  void guid_set(uint64_t guid);

  IBSysPort *makeSysPort (string pName);
  // make sure we got the port defined (so define it if not)

  IBPort *getSysPortNodePortByName (string sysPortName);
  // get the node port for the given sys port by name

  IBSysPort *getSysPort(string name);
  // Get a Sys Port by name
};

//
// IB Fabric Class
// The entire fabric
//
class IBFabric {
 public:
%readonly
  map_str_pnode NodeByName;   // Provide the node pointer by its name
  map_str_psys  SystemByName; // Provide the system pointer by its name
  vec_pport     PortByLid;    // Pointer to the Port by its lid
  map_guid_pnode NodeByGuid;   // Provides the node by guid
  map_guid_psys  SystemByGuid; // Provides the system by guid
  map_guid_pport PortByGuid;   // Provides the port by guid
%readwrite
  unsigned int  minLid;       // Track min lid used.
  unsigned int  maxLid;       // Track max lid used.
  unsigned int  lmc;          // LMC value used

  // IBFabric() {maxLid = 0;};

  // we need to have our own destructor to take care of the
  // fabrics vector cleanup

  // ~IBFabric();

  IBNode *makeNode (string n,
						  IBSystem *p_sys,
						  IBNodeType type,
						  unsigned int numPorts);
  // get the node by its name (create one of does not exist)

  IBNode *getNode (string name);
  // get the node by its name

  list_pnode *getNodesByType (IBNodeType type);
  // return the list of node pointers matching the required type

  IBSystem *makeGenericSystem (string name);
  // crate a new generic system - basically an empty contaner for nodes...

  IBSystem *makeSystem (string name, string type);
  // crate a new system - the type must have a registed factory.

  IBSystem *getSystem(string name);
  // Get system by name

  IBSystem *getSystemByGuid(uint64_t guid);
  // get the system by its guid
  IBNode *getNodeByGuid(uint64_t guid);
  // get the node by its guid
  IBPort *getPortByGuid(uint64_t guid);
  // get the port by its guid

  void addCable (string t1, string n1, string p1,
					  string t2, string n2, string p2,
                 IBLinkWidth width = DefaultLinkWidth,
                 IBLinkSpeed speed = DefaultLinkSpeed
                 );

  // Add a cable connection

  int parseCables (string fn);
  // Parse the cables file and build the fabric

  int parseTopology (string fn);
  // Parse Topology File

  int addLink(string type1, int numPorts1, uint64_t sysGuid1,
				  uint64_t nodeGuid1,  uint64_t portGuid1,
				  int vend1, int devId1, int rev1, string desc1,
				  int hcaIdx1, int lid1, int portNum1,
				  string type2, int numPorts2, uint64_t sysGuid2,
				  uint64_t nodeGuid2,  uint64_t portGuid2,
				  int vend2, int devId2, int rev2, string desc2,
				  int hcaIdx2, int lid2, int portNum2,
              IBLinkWidth width = DefaultLinkWidth,
              IBLinkSpeed speed = DefaultLinkSpeed);
  // Add a link into the fabric - this will create system
  // and nodes as required.

  int parseSubnetLinks (string fn);
  // Parse the OpenSM subnet.lst file and build the fabric from it.

  int parseFdbFile(string fn);
  // Parse OpenSM FDB dump file

  int parseMCFdbFile(string fn);
  // Parse an OpenSM MCFDBs file and set the MFT table accordingly

  int parsePSLFile(string fn);
  // Parse Path to SL mapping file

  int parseSLVLFile(string fn);
  // Parse SLVL tables file

  inline void setLidPort (unsigned int lid, IBPort *p_port);
  // set a lid port

  inline IBPort *getPortByLid (unsigned int lid);
  // get a port by lid

  int dumpTopology(const char *fileName, const char *ibnlDir);
  // write out a topology file and IBNLs into given directory

  int dumpNameMap(const char *fileName);
  // write out node names map file (guid, lid and name for each node)

  int setNodeGuidsByNameMapFile(const char *fileName);
  // parse a name to guid and LID map file and update node GUIDs

};

/* we use our own version of the constructor */
IBFabric*  new_IBFabric();
/* we use our own version of the destructor */
void delete_IBFabric(IBFabric *p_fabric);

%section "IBDM Functions",pre
/* IBDM functions */
%text %{
This section provide the details about the functions IBDM exposes.
The order follows the expected order in a regular IBDM flow.
They all return 0 on succes.
%}

%subsection "Subnet Utilities",before,pre
%text %{

  Subnet Utilities:

  The file holds a set of utilities to be run on the subnet to mimic OpenSM
  initialization and analyze the results:

  Assign Lids: SubnMgtAssignLids
  Init min hop tables: SubnMgtCalcMinHopTables
  Perform Enhanced LMC aware routing: SubnMgtOsmEnhancedRoute
  Perform standard routing: SubnMgtOsmRoute
  Perform Fat Tree specialized routing: SubnMgtFatTreeRoute
  Verify all CA to CA routes: SubnMgtVerifyAllCaToCaRoutes

%}

%name(ibdmAssignLids)
 int SubnMgtAssignLids (IBPort *p_smNodePort, unsigned int lmc = 0);
// Assign lids

%name(ibdmCalcMinHopTables)
 int SubnMgtCalcMinHopTables (IBFabric *p_fabric);
// Calculate the minhop table for the switches

%name(ibdmCalcUpDnMinHopTbls)
 int SubnMgtCalcUpDnMinHopTblsByRootNodesRex(IBFabric *p_fabric, const char *rootNodesNameRex);
// Fill in the FDB tables in a Up Down routing.
// Start the tree from the given nodes by regular expression

%name (ibdmOsmRoute)
 int SubnMgtOsmRoute(IBFabric *p_fabric);
// Fill in the FDB tables in an OpesnSM style routing
// which is switch based, uses number of routes per port
// profiling and treat LMC assigned lids sequentialy
// Rely on running the SubnMgtCalcMinHopTables beforehand

%name(ibdmEnhancedRoute)
 int SubnMgtOsmEnhancedRoute(IBFabric *p_fabric);
// Fill in the FDB tables in an OpesnSM style routing
// which is switch based, uses number of routes per port
// profiling and treat LMC assigned lids sequentialy.
// Also it will favor runing through a new system or node
// on top of the port profile.
// Rely on running the SubnMgtCalcMinHopTables beforehand

int ibdmFatTreeRoute(IBFabric *p_fabric, list_pnode rootNodes);
// Perform Fat Tree specific routing by assigning a single LID to
// each root node port a single LID to route through.

%name(ibdmFatTreeAnalysis) int FatTreeAnalysis(IBFabric *p_fabric);
// Performs FatTree structural analysis

%name(ibdmFatTreeRouteByPermutation) int FatTreeRouteByPermutation(IBFabric *p_fabric, const char* srcs, const char* dsts);
// Performs optimal permutation routing in FatTree

%name(ibdmVerifyCAtoCARoutes)
 int SubnMgtVerifyAllCaToCaRoutes(IBFabric *p_fabric);
// Verify point to point connectivity

%name(ibdmVerifyAllPaths)
 int SubnMgtVerifyAllRoutes(IBFabric *p_fabric);
// Verify all paths

%name(ibdmAnalyzeLoops)
 int CrdLoopAnalyze(IBFabric *p_fabric);
// Analyze the Fabric for Credit Loops

%name(ibdmSetCreditLoopAnalysisMode)
int CredLoopMode(int include_switch_to_switch_paths, int include_multicast);
// Set the analysis mode of ibdmAnalyzeLoops

%name(ibdmFindSymmetricalTreeRoots)
list_pnode SubnMgtFindTreeRootNodes(IBFabric *p_fabric);
// Analyze the fabric to find its root nodes assuming it is
// a pure tree (keeping all levels in place).

%name(ibdmFindRootNodesByMinHop)
list_pnode SubnMgtFindRootNodesByMinHop(IBFabric *p_fabric);
// Analyze the fabric to find its root nodes using statistical methods
// on the profiles of min hops to CAs

int ibdmRankFabricByRoots(IBFabric *p_fabric, list_pnode rootNodes);
// Just rank the fabric according to the given nodes list

int ibdmReportNonUpDownCa2CaPaths(IBFabric *p_fabric, list_pnode rootNodes);
// Find any routes that exist in the FDB's from CA to CA and do not adhare to
// the up/down rules. Report any crossing of the path. Use the given list fo nodes
// as roots of the tree.

%name(ibdmReportCA2CAPathsThroughSWPort)
int SubnReportCA2CAPathsThroughSWPort(IBPort *p_port);
// Report all CA 2 CA Paths giong through the given port

%name(ibdmCheckMulticastGroups)
int SubnMgtCheckFabricMCGrps(IBFabric *p_fabric);
// Check all multicast groups :
// 1. all switches holding it are connected
// 2. No loops (i.e. a single BFS with no returns).

int ibdmCheckFabricMCGrpsForCreditLoopPotential(
  IBFabric *p_fabric, list_pnode rootNodes);
// Check all multicast groups do not have credit loop potential

%name(ibdmLinkCoverageAnalysis)
int LinkCoverageAnalysis(IBFabric *p_fabric, list_pnode rootNodes);
// Provide sets of port pairs to run BW check from in a way that is
// full bandwidth. Reide in LinkCover.cpp

%subsection "Tracing Utilities",before,pre

%name(ibdmTraceDRPathRoute)
int TraceDRPathRoute (IBPort *p_smNodePort, list_int drPathPortNums);
// Trace a direct route from the given SM node port

%name(ibdmTraceRouteByMinHops)
int TraceRouteByMinHops (IBFabric *p_fabric,
  unsigned int slid , unsigned int dlid);
// Trace a route from slid to dlid by Min Hop

%typemap(tcl8,in) list_pnode_arg_name*(list_pnode tmp) {
	$target = &tmp;
}

%typemap(tcl8,argout) list_pnode_arg_name* {
  // build a TCL list out of the Objec ID's of the ibdm objects in it.
  list_pnode::const_iterator I = $source->begin();
  Tcl_Obj *p_tclObj;
  Tcl_SetVar(interp, Tcl_GetString($arg),"",0);
  while (I != $source->end()) {
	 p_tclObj = Tcl_NewObj();
	 if (ibdmGetObjTclNameByPtr(p_tclObj, (*I), "IBNode *") != TCL_OK) {
		printf("-E- Fail to map Node Object (a guid map element)\n");
	 } else {
		char buf[128];
		sprintf(buf, "%s", Tcl_GetString(p_tclObj));
		Tcl_SetVar(interp, Tcl_GetString($arg), buf,
					  TCL_LIST_ELEMENT|TCL_APPEND_VALUE);
	 }
	 Tcl_DecrRefCount(p_tclObj);
	 I++;
  }
}
%{
#define list_pnode_arg_name list_pnode
%}

%typemap(tcl8,in) unsigned_int_arg_name*(unsigned int tmp) {
	$target = &tmp;
}

%typemap(tcl8,argout) unsigned_int_arg_name* {
   char buf[16];
	sprintf(buf, "%u", tmp);
   Tcl_SetVar(interp, Tcl_GetString($arg), buf, 0);
}
%{
#define unsigned_int_arg_name unsigned int
%}

%name(ibdmTraceRouteByLFT)
int TraceRouteByLFT (IBFabric *p_fabric,
							unsigned int slid , unsigned int dlid,
							unsigned_int_arg_name *hops,
							list_pnode_arg_name *p_nodesList);
// Trace a route from slid to dlid by LFT

%subsection "Topology Matching Utilities",before,pre

%apply char **p_out_str {char **p_report_str};

%name(ibdmMatchFabrics)
int TopoMatchFabrics(
  IBFabric *p_spec_fabric,       // The specification fabric
  IBFabric *p_discovered_fabric, // The discovered fabric
  char     *anchorNodeName,      // The system to be the anchor point
  int       anchorPortNum,       // The port number of the anchor port
  uint64_t  anchorPortGuid,      // Discovered Guid of the anchor port
  char **p_report_str            // Diagnostic output.
  );
// Error if fabrics deffer. And provide it as result.

%name(ibdmBuildMergedFabric)
int
TopoMergeDiscAndSpecFabrics(
  IBFabric  *p_spec_fabric,       // The specification fabric
  IBFabric  *p_discovered_fabric, // The discovered fabric
  IBFabric  *p_merged_fabric);    // Output merged fabric (allocated internaly)
// Build a merged fabric from a matched discovered and spec fabrics.
// NOTE: you have to run ibdmMatchFabrics before calling this routine.

%name(ibdmMatchFabricsFromEdge)
int
TopoMatchFabricsFromEdge(
  IBFabric *p_sFabric,            // The specification fabric
  IBFabric *p_dFabric,            // The discovered fabric
  char **p_report_str             // Diagnostic output.
  );
// Match the fabrics going from the hosts inwards. Report mismatches.

%subsection "Congestion Analysis Utilities",before,pre

%name(ibdmCongInit) int CongInit(IBFabric *p_fabric);
// Initialize a fabric for congestion analysis

%name(ibdmCongCleanup) int CongCleanup(IBFabric *p_fabric);
// Cleanup congestion analysis data and free memory

%name(ibdmCongClear) int CongZero(IBFabric *p_fabric);
// Clear the congestion analysis path trace. Does not affect max paths

%name(ibdmCongTrace)
int CongTrackPath(IBFabric *p_fabric, uint16_t srcLid, uint16_t dstLid);
// Trace the path from source to destination tracking the visited links

%name(ibdmCongReport) int CongReport(IBFabric *p_fabric, ostringstream &out);
// Report the max path count and histogram

%name(ibdmCongDump) int CongDump(IBFabric *p_fabric, ostringstream &out);
// provide detailed dump of the link usage

//
// FIX OF SWIG TO SUPPORT NAME ALTERNATE MANGLING
//
%{
#include "swig_alternate_mangling.cpp"
%}

///////////////////////////////////////////////////////////////////////////////
extern char * ibdmSourceVersion;

//
// INIT CODE
//
%init %{

  /* mixing declarations .... */
  {
	 // Register the objects for alternate mangling
    SWIG_AlternateObjMangling["_IBFabric_p"] = &ibdmGetObjTclNameByPtr;
    SWIG_AlternateNameToObj  ["_IBFabric_p"] = &ibdmGetObjPtrByTclName;

    SWIG_AlternateObjMangling["_IBSystem_p"] = &ibdmGetObjTclNameByPtr;
    SWIG_AlternateNameToObj  ["_IBSystem_p"] = &ibdmGetObjPtrByTclName;

    SWIG_AlternateObjMangling["_IBSysPort_p"] = &ibdmGetObjTclNameByPtr;
    SWIG_AlternateNameToObj  ["_IBSysPort_p"] = &ibdmGetObjPtrByTclName;

    SWIG_AlternateObjMangling["_IBNode_p"] = &ibdmGetObjTclNameByPtr;
    SWIG_AlternateNameToObj  ["_IBNode_p"] = &ibdmGetObjPtrByTclName;

    SWIG_AlternateObjMangling["_IBPort_p"] = &ibdmGetObjTclNameByPtr;
    SWIG_AlternateNameToObj  ["_IBPort_p"] = &ibdmGetObjPtrByTclName;
  }

%}
