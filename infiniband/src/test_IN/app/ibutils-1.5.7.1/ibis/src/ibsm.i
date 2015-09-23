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

%text %{

  Subnet Management:
    The SM interface supports sending Subnet Management Packets to
    the fabric devices.

    A set of object types is defined. The user can allocate a new attribute
    object. Then the object can be set using configure or the direct
    manipulation methods.

    Extra methods are provided for each object:
    setByDr <dr> <attrMod>
    getByDr <dr> <attrMod>
    setByLid <lid> <attrMod>
    getByLid <lid> <attrMod>
%}

/*
 * NOTE : The simplest way to implement that interface is to
 * code the Set section as embedded tcl code that will create a new
 * object of the attribute type and use the given set of flags
 * as configuration command. This way we let swig do the coding
 * and parsing of each field.
 */

%{
#include "stdio.h"
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <iba/ib_types.h>
#include <complib/cl_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_map.h>
#include <complib/cl_debug.h>
#include "ibis_api.h"

  /* the global pointer to this SM mads manager */
  static ibsm_t *gp_ibsm;

  /* the data section within a LFT mad */
  typedef struct _ibsm_lft_block {
    uint8_t lft[64];
  } ibsm_lft_block_t;

  /* the data section within a MFT mad */
  typedef struct _ibsm_mft_block {
    uint16_t mft[32];
  } ibsm_mft_block_t;

  /* we probably want to use our own naming for classes */
  typedef ib_node_info_t        smNodeInfo;
  typedef ib_port_info_t        smPortInfo;
  typedef ib_switch_info_t      smSwInfo;
  typedef ibsm_lft_block_t      smLftBlock;
  typedef ibsm_mft_block_t      smMftBlock;
  typedef ib_guid_info_t        smGuidInfo;
  typedef ib_pkey_table_t       smPkeyTable;
  typedef ib_slvl_table_t       smSlVlTable;
  typedef ib_vl_arb_table_t     smVlArbTable;
  typedef ib_node_desc_t        smNodeDesc;
  typedef ib_sm_info_t          smSMInfo;
  typedef ib_mad_notice_attr_t  smNotice;

  /* these are the globals to be used for set/get */
  ib_node_info_t          ibsm_node_info_obj;
  ib_port_info_t          ibsm_port_info_obj;
  ib_switch_info_t        ibsm_switch_info_obj;
  ibsm_lft_block_t        ibsm_lft_block_obj;
  ibsm_mft_block_t        ibsm_mft_block_obj;
  ib_guid_info_t          ibsm_guid_info_obj;
  ib_pkey_table_t         ibsm_pkey_table_obj;
  ib_slvl_table_t         ibsm_slvl_table_obj;
  ib_vl_arb_table_t       ibsm_vl_arb_table_obj;
  ib_node_desc_t          ibsm_node_desc_obj;
  ib_sm_info_t            ibsm_sm_info_obj;
  ib_mad_notice_attr_t    ibsm_notice_obj;

  /* TODO - define a Vendor Specific CR Read/Write attributes to use VL15 */

%}

//
// STANDARD IB TYPE MAPS:
//

%typemap(tcl8,in) ib_gid_t*(ib_gid_t temp) {
  char buf[38];
  char *p_prefix, *p_guid;
  char *str_token = NULL;

  strcpy(buf, Tcl_GetStringFromObj($source,NULL));
  p_prefix = strtok_r(buf,":", &str_token);
  p_guid = strtok_r(NULL, " ", &str_token);
  temp.unicast.prefix = cl_hton64(strtoull(p_prefix, NULL, 16));
  errno = 0;
  if (errno) {
    printf("Wrong format for gid prefix:%s\n", p_prefix);
    return TCL_ERROR;
  }

  temp.unicast.interface_id = cl_hton64(strtoull(p_guid, NULL, 16));
  if (errno) {
    printf("Wrong format for gid guid:%s\n", p_guid);
    return TCL_ERROR;
  }

  $target = &temp;
}


%typemap(tcl8, out) ibsm_lft_block_t * {
  char buf[12];
  int i;
  for (i = 0; i < 64; i++) {
    sprintf(buf, "{%u %u} ", i, $source[i]);
    Tcl_AppendToObj($target, buf, -1);
  }
}

%typemap(tcl8, out) ibsm_mft_block_t * {
  char buf[12];
  int i;
  for (i = 0; i < 32; i++) {
    sprintf(buf, "{%u %u} ", i, $source[i]);
    Tcl_AppendToObj($target, buf, -1);
  }
}

%typemap(tcl8, out) ibsm_dr_path_t* {
  char buf[12];
  int i;
  for (i = 1; i < $source->count; i++) {
    sprintf(buf, "%u ", $source->path[i]);
    Tcl_AppendToObj($target, buf, -1);
  }
}

%typemap(tcl8,in) ibsm_dr_path_t* (ibsm_dr_path_t dr) {
  char buf[1024];
  char *p_next;
  unsigned int port;
  int i;

  dr.count = 1;
  dr.path[0] = 0;

  strncpy(buf, Tcl_GetStringFromObj($source,NULL), 1023);
  buf[1023] = '\0';
  p_next = strtok(buf," \t");
  while (p_next != NULL)
  {
    if (sscanf(p_next,"%u", &port) != 1)
    {
      printf("Error: bad format in directed route path index:%u : %s\n",
             dr.count, p_next);
      return TCL_ERROR;
    }
    dr.path[dr.count++] = port;
    p_next = strtok(NULL," \t");
  }
  for (i = dr.count; i < 64; i++) dr.path[i] = 0;
  dr.count--;
  $target = &dr;
}

//
// INTERFACE DEFINITION (~copy of h file)
//

%section "IBSM Constants"
/* These constants are provided by IBSM: */

%section "IBSM Objects",pre
/* This section describes the various object types exposed by IBSM. */
%text %{

%}

typedef struct _ibsm_node_info
{
  uint8_t       base_version;
  uint8_t       class_version;
  uint8_t       node_type;
  uint8_t       num_ports;
  ib_net64_t    sys_guid;
  ib_net64_t    node_guid;
  ib_net64_t    port_guid;
  ib_net16_t    partition_cap;
  ib_net16_t    device_id;
  ib_net32_t    revision;
  ib_net32_t    port_num_vendor_id;
} smNodeInfo;

%addmethods smNodeInfo {
  int getByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smNodeInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_NODE_INFO), 0,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smNodeInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_NODE_INFO), 0,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smNodeInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_NODE_INFO), 0,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smNodeInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_NODE_INFO), 0,
                                IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_port_info
{
  ib_net64_t  m_key;
  ib_net64_t  subnet_prefix;
  ib_net16_t  base_lid;
  ib_net16_t  master_sm_base_lid;
  ib_net32_t  capability_mask;
  ib_net16_t  diag_code;
  ib_net16_t  m_key_lease_period;
  uint8_t     local_port_num;
  uint8_t     link_width_enabled;
  uint8_t     link_width_supported;
  uint8_t     link_width_active;
  uint8_t     state_info1;    // LinkSpeedSupported and PortState
  uint8_t     state_info2;  // PortPhysState and LinkDownDefaultState
  uint8_t     mkey_lmc;
  uint8_t     link_speed;   // LinkSpeedEnabled and LinkSpeedActive
  uint8_t     mtu_smsl;
  uint8_t     vl_cap;       // VlCap and InitType
  uint8_t     vl_high_limit;
  uint8_t     vl_arb_high_cap;
  uint8_t     vl_arb_low_cap;
  uint8_t     mtu_cap;
  uint8_t     vl_stall_life;
  uint8_t     vl_enforce;
  ib_net16_t  m_key_violations;
  ib_net16_t  p_key_violations;
  ib_net16_t  q_key_violations;
  uint8_t     guid_cap;
  uint8_t     subnet_timeout;
  uint8_t     resp_time_value;
  uint8_t     error_threshold;
} smPortInfo;

%addmethods smPortInfo {
  int getByDr(ibsm_dr_path_t *dr, uint8_t portNum) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smPortInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_PORT_INFO), portNum, IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint8_t portNum) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smPortInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_PORT_INFO), portNum, IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint8_t portNum) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smPortInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_PORT_INFO), portNum, IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint8_t portNum) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smPortInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_PORT_INFO), portNum, IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_switch_info
{
  ib_net16_t   lin_cap;
  ib_net16_t   rand_cap;
  ib_net16_t   mcast_cap;
  ib_net16_t   lin_top;
  uint8_t      def_port;
  uint8_t      def_mcast_pri_port;
  uint8_t      def_mcast_not_port;
  uint8_t      life_state;
  ib_net16_t   lids_per_port;
  ib_net16_t   enforce_cap;
  uint8_t      flags;
} smSwInfo;

%addmethods smSwInfo {
  int getByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smSwInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_SWITCH_INFO), 0,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smSwInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_SWITCH_INFO), 0,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smSwInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_SWITCH_INFO), 0,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smSwInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_SWITCH_INFO), 0,
                                IB_MAD_METHOD_SET));
  }
}

/* the data section within a LFT mad */
typedef struct _ibsm_lft_block {
  uint8_array_t lft[64];
} smLftBlock;

%addmethods smLftBlock {
  int getByDr(ibsm_dr_path_t *dr, uint16_t blockNum) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smLftBlock),
                               dr, CL_NTOH16(IB_MAD_ATTR_LIN_FWD_TBL), blockNum, IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint16_t blockNum) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smLftBlock),
                               dr, CL_NTOH16(IB_MAD_ATTR_LIN_FWD_TBL), blockNum, IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint16_t blockNum) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smLftBlock),
                                lid, CL_NTOH16(IB_MAD_ATTR_LIN_FWD_TBL), blockNum, IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint16_t blockNum) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smLftBlock),
                                lid, CL_NTOH16(IB_MAD_ATTR_LIN_FWD_TBL), blockNum, IB_MAD_METHOD_SET));
  }
}

/* the data section within a MFT mad */
typedef struct _ibsm_mft_block {
  ib_net16_array_t mft[32];
} smMftBlock;

%{
  int smMftGetAttrMod( uint16_t startLid, uint8_t startPort, uint32_t *p_attrMod )
    {
      if (startLid % 32)
      {
        printf("Error: Given startLid must be a multiply of 32: %u\n", startLid);
        return TCL_ERROR;
      }
      if (startPort % 16)
      {
        printf("Error: Given startPort must be a multiply of 16: %u\n", startPort);
        return TCL_ERROR;
      }

      /*
         always true due to the uint8_t
         if (startPort > 255)
         {
         printf("Error: Given startPort is out of range: %u > 255\n", startPort);
         return TCL_ERROR;
         }
      */
      *p_attrMod = ((startLid - 0xc000) / 32) + ((startPort / 16) << 28);
      return TCL_OK;
    }
%}

%addmethods smMftBlock {
  int getByDr(ibsm_dr_path_t *dr, uint16_t startLid, uint8_t startPort) {
    uint32_t attrMod;
    if (smMftGetAttrMod(startLid, startPort, &attrMod) != TCL_OK)
      return TCL_ERROR;
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smMftBlock),
                               dr, CL_NTOH16(IB_MAD_ATTR_MCAST_FWD_TBL), attrMod, IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint16_t startLid, uint8_t startPort) {
    uint32_t attrMod;
    if (smMftGetAttrMod(startLid, startPort, &attrMod) != TCL_OK)
      return TCL_ERROR;
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smMftBlock),
                               dr, CL_NTOH16(IB_MAD_ATTR_MCAST_FWD_TBL), attrMod, IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint16_t startLid, uint8_t startPort) {
    uint32_t attrMod;
    if (smMftGetAttrMod(startLid, startPort, &attrMod) != TCL_OK)
      return TCL_ERROR;
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smMftBlock),
                                lid, CL_NTOH16(IB_MAD_ATTR_MCAST_FWD_TBL), attrMod, IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint16_t startLid, uint8_t startPort) {
    uint32_t attrMod;
    if (smMftGetAttrMod(startLid, startPort, &attrMod) != TCL_OK)
      return TCL_ERROR;
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smMftBlock),
                                lid, CL_NTOH16(IB_MAD_ATTR_MCAST_FWD_TBL), attrMod, IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_guid_info
{
	ib_net64_array_t guid[GUID_TABLE_MAX_ENTRIES];
} smGuidInfo;

%addmethods smGuidInfo {
  int getByDr(ibsm_dr_path_t *dr, uint16_t blockNum) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smGuidInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_GUID_INFO), blockNum,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint16_t blockNum) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smGuidInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_GUID_INFO), blockNum,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint16_t blockNum) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smGuidInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_GUID_INFO), blockNum,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint16_t blockNum) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smGuidInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_GUID_INFO), blockNum,
                                IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_pkey_table
{
  ib_net16_array_t pkey_entry[IB_NUM_PKEY_ELEMENTS_IN_BLOCK];
} smPkeyTable;

%addmethods smPkeyTable {
  int getByDr(ibsm_dr_path_t *dr, uint8_t portNum, uint16_t blockNum) {
    uint32_t attrMod = blockNum | (portNum << 16);
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smPkeyTable),
                               dr, CL_NTOH16(IB_MAD_ATTR_P_KEY_TABLE), attrMod,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint8_t portNum, uint16_t blockNum) {
    uint32_t attrMod = blockNum | (portNum << 16);
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smPkeyTable),
                               dr, CL_NTOH16(IB_MAD_ATTR_P_KEY_TABLE), attrMod,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint8_t portNum, uint16_t blockNum) {
    uint32_t attrMod = blockNum | (portNum << 16);
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smPkeyTable),
                                lid, CL_NTOH16(IB_MAD_ATTR_P_KEY_TABLE), attrMod,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint8_t portNum, uint16_t blockNum) {
    uint32_t attrMod = blockNum | (portNum << 16);
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smPkeyTable),
                                lid, CL_NTOH16(IB_MAD_ATTR_P_KEY_TABLE), attrMod,
                                IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_slvl_table
{
	uint8_array_t raw_vl_by_sl[IB_MAX_NUM_VLS/2];
} smSlVlTable;

%addmethods smSlVlTable {
  int getByDr(ibsm_dr_path_t *dr, uint8_t inPortNum, uint8_t outPortNum) {
    uint32_t attrMod = outPortNum | (inPortNum << 8);
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smSlVlTable),
                               dr, CL_NTOH16(IB_MAD_ATTR_SLVL_TABLE), attrMod,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint8_t inPortNum, uint8_t outPortNum) {
    uint32_t attrMod = outPortNum | (inPortNum << 8);
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smSlVlTable),
                               dr, CL_NTOH16(IB_MAD_ATTR_SLVL_TABLE), attrMod,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint8_t inPortNum, uint8_t outPortNum) {
    uint32_t attrMod = outPortNum | (inPortNum << 8);
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smSlVlTable),
                                lid, CL_NTOH16(IB_MAD_ATTR_SLVL_TABLE), attrMod,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint8_t inPortNum, uint8_t outPortNum) {
    uint32_t attrMod = outPortNum | (inPortNum << 8);
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smSlVlTable),
                                lid, CL_NTOH16(IB_MAD_ATTR_SLVL_TABLE), attrMod,
                                IB_MAD_METHOD_SET));
  }
}

/* ---------------- handling array of ib_vl_arb_element_t ---------------------- */
%typemap(memberin) ib_vl_arb_element_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(in) ib_vl_arb_element_t[ANY] (ib_vl_arb_element_t entrys[$dim0]) {
  char *p_ch;
  char *last;
  long int vl, weight;
  int   countSubLists, idx, numElements;
  int i = 0;
  Tcl_Obj	**subListObjArray;
  Tcl_Obj   *tclObj;

  if (Tcl_ListObjGetElements(interp, $source, &countSubLists, &subListObjArray) != TCL_OK)
  {
    printf("Error: wrong format for VL Arb Table: %s\n",
           Tcl_GetStringFromObj($source,NULL));
    return TCL_ERROR;
  }

  /* go over all sub lists and convert them */
  for (idx = 0; idx < countSubLists; idx++) {
    if (Tcl_ListObjLength(interp, subListObjArray[idx], &numElements) != TCL_OK)
    {
      printf("Error: wrong format for VL Arb Table Entry: %s\n",
             Tcl_GetStringFromObj(subListObjArray[idx],NULL));
      return TCL_ERROR;
    }

    if (numElements != 2)
    {
      printf("Error: wrong number of elements for VL Arb Table Entry: %s\n",
             Tcl_GetStringFromObj(subListObjArray[idx],NULL));
      return TCL_ERROR;
    }

    if (Tcl_ListObjIndex(interp, subListObjArray[idx], 0, &tclObj) != TCL_OK)
    {
      printf("Error: Fail to obtain first element of VL Arb Table Entry: %s\n",
             Tcl_GetStringFromObj(subListObjArray[idx],NULL));
      return TCL_ERROR;
    }

    vl = strtol(Tcl_GetStringFromObj( tclObj, NULL ), NULL, 0);
    if (Tcl_ListObjIndex(interp, subListObjArray[idx], 1, &tclObj) != TCL_OK)
    {
      printf("Error: Fail to obtain second element of VL Arb Table Entry: %s\n",
             Tcl_GetStringFromObj(subListObjArray[idx],NULL));
      return TCL_ERROR;
    }

    weight = strtol(Tcl_GetStringFromObj( tclObj, NULL ), NULL, 0);

    entrys[i].vl = vl;
    entrys[i++].weight = weight;

    p_ch = strtok_r(NULL, " \t", &last);
  }

  for (; i < $dim0; i++)
  {
    entrys[i].vl = 0;
    entrys[i].weight = 0;
  }

  $target = entrys;
}

%typemap(tcl8, out) ib_vl_arb_element_t[ANY] {
  int i;
  char buff[16];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "{0x%x 0x%02x} ", $source[i].vl, $source[i].weight);
    Tcl_AppendResult(interp, buff, NULL);
  }
}

typedef struct _ibsm_vl_arb_table
{
	ib_vl_arb_element_t vl_entry[IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK];
} smVlArbTable;

%addmethods smVlArbTable {
  int getByDr(ibsm_dr_path_t *dr, uint8_t portNum, uint8_t block) {
    uint32_t attrMod = (block <<16) | portNum;
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smVlArbTable),
                               dr, CL_NTOH16(IB_MAD_ATTR_VL_ARBITRATION), attrMod,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr, uint8_t portNum, uint8_t block) {
    uint32_t attrMod = (block <<16) | portNum;
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smVlArbTable),
                               dr, CL_NTOH16(IB_MAD_ATTR_VL_ARBITRATION), attrMod,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid, uint8_t portNum, uint8_t block) {
    uint32_t attrMod = (block <<16) | portNum;
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smVlArbTable),
                                lid, CL_NTOH16(IB_MAD_ATTR_VL_ARBITRATION), attrMod,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid, uint8_t portNum, uint8_t block) {
    uint32_t attrMod = (block <<16) | portNum;
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smVlArbTable),
                                lid, CL_NTOH16(IB_MAD_ATTR_VL_ARBITRATION), attrMod,
                                IB_MAD_METHOD_SET));
  }
}

%{
  typedef uint8_t ibsm_node_desc_str_t;
%}

%typemap(tcl8,in) ibsm_node_desc_str_t *(uint8_t temp[IB_NODE_DESCRIPTION_SIZE]) {
  strcpy((char *)temp, Tcl_GetStringFromObj($source,NULL));
  $target = temp;
}
%typemap(tcl8,memberin) ibsm_node_desc_str_t[IB_NODE_DESCRIPTION_SIZE] {
  strncpy((char *)$target,(char *)$source,IB_NODE_DESCRIPTION_SIZE - 1);
  $target[IB_NODE_DESCRIPTION_SIZE - 1] = '\0';
}

%typemap(tcl8,out) ibsm_node_desc_str_t[ANY] {
  /* we must make sure we do not overflow the node desc length */
  char buff[IB_NODE_DESCRIPTION_SIZE];
  strncpy(buff,(char *)$source,IB_NODE_DESCRIPTION_SIZE - 1);
  buff[IB_NODE_DESCRIPTION_SIZE - 1] = '\0';
  Tcl_SetStringObj($target, buff, strlen(buff));
}

typedef struct _ibsm_node_desc
{
	ibsm_node_desc_str_t	description[IB_NODE_DESCRIPTION_SIZE];
} smNodeDesc;

%addmethods smNodeDesc {
  int getByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smNodeDesc),
                               dr, CL_NTOH16(IB_MAD_ATTR_NODE_DESC), 0,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smNodeDesc),
                               dr, CL_NTOH16(IB_MAD_ATTR_NODE_DESC), 0,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smNodeDesc),
                                lid, CL_NTOH16(IB_MAD_ATTR_NODE_DESC), 0,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smNodeDesc),
                                lid, CL_NTOH16(IB_MAD_ATTR_NODE_DESC), 0,
                                IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_sm_info
{
  ib_net64_t			guid;
  ib_net64_t			sm_key;
  ib_net32_t			act_count;
  uint8_t				pri_state;
}	smSMInfo;

%addmethods smSMInfo {
  int getByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smSMInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_SM_INFO), 0,
                               IB_MAD_METHOD_GET));
  }
  int setByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self, sizeof(smSMInfo),
                               dr, CL_NTOH16(IB_MAD_ATTR_SM_INFO), 0,
                               IB_MAD_METHOD_SET));
  }
  int getByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smSMInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_SM_INFO), 0,
                                IB_MAD_METHOD_GET));
  }
  int setByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self, sizeof(smSMInfo),
                                lid, CL_NTOH16(IB_MAD_ATTR_SM_INFO), 0,
                                IB_MAD_METHOD_SET));
  }
}

typedef struct _ibsm_notice
{
  uint8_t		   generic_type;
  union _sm_notice_g_or_v
  {
	 struct _sm_notice_generic
	 {
		uint8_t		prod_type_msb;
		ib_net16_t	prod_type_lsb;
		ib_net16_t	trap_num;
	 }	generic;

	 struct _sm_notice_vend
	 {
		uint8_t		vend_id_msb;
		ib_net16_t	vend_id_lsb;
		ib_net16_t	dev_id;
	 } vend;
  } g_or_v;

  ib_net16_t			issuer_lid;
  ib_net16_t			toggle_count;

  union _data_details
  {
    struct _sm_raw_data
    {
      uint8_array_t	details[54];
    } raw_data;

    struct _sm_ntc_64_67
    {
      uint8_array_t  res[6];
      ib_gid_t   gid;	// the Node or Multicast Group that came in/out
    } ntc_64_67;

    struct _sm_ntc_128 {
      ib_net16_t sw_lid; // the sw lid of which link state changed
    } ntc_128;

    struct _sm_ntc_129_131 {
      ib_net16_t    pad;
      ib_net16_t    lid;		// lid and port number of the violation
      uint8_t       port_num;
    } ntc_129_131;

    struct _sm_ntc_144 {
      ib_net16_t    pad1;
      ib_net16_t    lid;		// lid where capability mask changed
      ib_net16_t    pad2;
      ib_net32_t    new_cap_mask; // new capability mask
    } ntc_144;

    struct _sm_ntc_145 {
      ib_net16_t    pad1;
      ib_net16_t    lid;		// lid where sys guid changed
      ib_net16_t    pad2;
      ib_net64_t    new_sys_guid; // new system image guid
    } ntc_145;

    struct _sm_ntc_256 {
      ib_net16_t    pad1;
      ib_net16_t    lid;
      ib_net16_t    pad2;
      uint8_t       method;
      uint8_t       pad3;
      ib_net16_t    attr_id;
      ib_net32_t    attr_mod;
      ib_net64_t    mkey;
      uint8_t       dr_slid;
      uint8_t       dr_trunc_hop;
      uint8_array_t dr_rtn_path[30];
    } ntc_256;

    struct _sm_ntc_257_258 // violation of p/q_key // 49
    {
      ib_net16_t    pad1;
      ib_net16_t    lid1;
      ib_net16_t    lid2;
      ib_net32_t    key;
      uint8_t       sl;
      ib_net32_t    qp1;
      ib_net32_t    qp2;
      ib_gid_t      gid1;
      ib_gid_t      gid2;
    } ntc_257_258;

    struct _sm_ntc_259 // p/q_key violation with sw info 53
    {
      ib_net16_t    data_valid;
      ib_net16_t    lid1;
      ib_net16_t    lid2;
      ib_net32_t    key;
      uint8_t       sl;
      ib_net32_t    qp1;
      uint8_t       qp2_msb;
      ib_net16_t    qp2_lsb;
      ib_gid_t      gid1;
      ib_gid_t      gid2;
      ib_net16_t    sw_lid;
      uint8_t       port_no;
    } ntc_259;

  } data_details;

  ib_gid_t			issuer_gid;
} smNotice;

%addmethods smNotice {
  int trapByDr(ibsm_dr_path_t *dr) {
    return(ibsm_send_mad_by_dr(gp_ibsm, (uint8_t *)self,
                               sizeof(smNotice),
                               dr, CL_NTOH16(IB_MAD_ATTR_NOTICE), 0,
                               IB_MAD_METHOD_TRAP));
  }
  int trapByLid(uint16_t lid) {
    return(ibsm_send_mad_by_lid(gp_ibsm, (uint8_t *)self,
                                sizeof(smNotice),
                                lid, CL_NTOH16(IB_MAD_ATTR_NOTICE), 0,
                                IB_MAD_METHOD_TRAP));
  }
}
