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

/* Holds ib_types.h MAD structs in and out TypeMaps */
%{

  /* for IB structs we use the format: <type>:<ptr> */

  /* Given the Object Pointer and Type provide it's TCL name */
  int ibmsGetIBStructObjNameByPtr(Tcl_Obj *objPtr, void *ptr, char *type) {
	 char tclName[128];
	 string uiType;
	 char name[128];

    /* check that the string starts with _ib_ and ends with _t_p */
    if (strncmp(type, "_ib_", 4)) {
		sprintf(tclName, "-E- Unrecognized Object Type:%s (should start with _ib_)", type);
		Tcl_SetStringObj(objPtr, tclName, -1);
		return TCL_ERROR;
	 }

    if (strncmp(type+strlen(type) - 4, "_t_p", 4)) {
		sprintf(tclName, "-E- Unrecognized Object Type:%s (should end with _t_p %s)",
              type, type+strlen(type) - 4);
		Tcl_SetStringObj(objPtr, tclName, -1);
		return TCL_ERROR;
	 }

    strncpy(name, type+4, strlen(type) - 8);
    name[strlen(type) - 8] = '\0';
    sprintf(tclName, "%s:%p", name, ptr);
    Tcl_SetStringObj(objPtr, tclName, -1);
    return TCL_OK;
  }

  /* Given the Object TCL Name Get it's pointer */
  int ibmsGetIBStructObjPtrByTclName(Tcl_Obj *objPtr, void **ptr) {
	 /* we need to parse the name and get the type etc. */
	 char *colonIdx;
	 *ptr = NULL;
    char buf[256];

	 strcpy(buf, Tcl_GetStringFromObj(objPtr,0));

	 /* the format is always: <type>:<idx>[:<name>] */

	 /* first separate the type */
	 colonIdx = index(buf,':');
	 if (!colonIdx) {
		printf("-E- Bad formatted (no :) ibdm object:%s\n", buf);
		return TCL_ERROR;
	 }
	 *colonIdx = '\0';
    colonIdx++;

    /* now all we need is to extract the pointer value from the
       rest of the string */
    if (sscanf(colonIdx,"%p", ptr) != 1) {
		printf("-E- Bad formatted pointer value:%s\n", colonIdx);
		return TCL_ERROR;
    }
	 return TCL_OK;
  }
%}

%typemap(tcl8,in) ib_gid_t*(ib_gid_t temp) {
  char buf[36];
  char *p_prefix, *p_guid;
  char *str_token = NULL;

  strcpy(buf, Tcl_GetStringFromObj($source,NULL));
  p_prefix = strtok_r(buf,":", &str_token);
  p_guid = strtok_r(NULL, " ", &str_token);
  errno = 0;
  temp.unicast.prefix = cl_hton64(strtoull(p_prefix, NULL, 16));
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

%typemap(tcl8,out) ib_gid_t* {
  char buff[36];
  sprintf(buff, "0x%016" PRIx64 ":0x%016" PRIx64,
          cl_ntoh64($source->unicast.prefix),
          cl_ntoh64($source->unicast.interface_id)
          );
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,out) ib_vl_arb_table_t* {
  char buff[256];
  int i;
  if ($source != NULL)
  {
    for (i = 0; i < 32; i++)
    {
      sprintf(buff, "{0x%02x 0x%02x} ", $source->vl_entry[i].vl, $source->vl_entry[i].weight);
      Tcl_AppendToObj($target,buff,strlen(buff));
    }
  }
  else
  {
    Tcl_SetStringObj($target, "{0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0} {0 0}", -1);
  }
}

/* break the list of sub lists into vl weight ... */
%typemap(tcl8,in) ib_vl_arb_table_t* (ib_vl_arb_table_t tmp) {
  int i;

  int numEntries, numElements, code;
  const char **subListStrings, **elements;

  code = Tcl_SplitList(interp, Tcl_GetStringFromObj($source,NULL),
							  &numEntries, &subListStrings);
  if (code != TCL_OK) {
	 printf("Wrong format for vl_arb_table should be list of lists:%s\n",
			  Tcl_GetStringFromObj($source,NULL));
	 return TCL_ERROR;
  }

  memset(&tmp, 0, sizeof(ib_vl_arb_table_t));
  for (i = 0; i < numEntries; i++) {
	 code = Tcl_SplitList(interp, subListStrings[i], &numElements, &elements);
	 if (code != TCL_OK) {
		printf("Wrong format for vl_arb_table sublist:%s\n", subListStrings[i]);
		Tcl_Free((char *) subListStrings);
		return TCL_ERROR;
	 }
	 if (numElements != 2) {
		printf("Wrong format for vl_arb_table sublist:%s num elements:%d != 2\n",
				 subListStrings[i], numElements);
		Tcl_Free((char *) elements);
		Tcl_Free((char *) subListStrings);
		return TCL_ERROR;
	 }
	 errno = 0;
	 tmp.vl_entry[i].vl = strtoul(elements[0],NULL,0);
	 if (errno) {
		printf("Wrong format for vl_arb_table sublist %d vl:%s\n",
				 i, elements[0]);
		Tcl_Free((char *) elements);
		Tcl_Free((char *) subListStrings);
		return TCL_ERROR;
	 }
	 tmp.vl_entry[i].weight =  strtoul(elements[1],NULL,0);
	 if (errno) {
		 printf("Wrong format for vl_arb_table sublist %d weight:%s\n",
				  i, elements[1]);
		Tcl_Free((char *) elements);
		Tcl_Free((char *) subListStrings);
		return TCL_ERROR;
	 }
	 Tcl_Free((char *) elements);
  }
  Tcl_Free((char *) subListStrings);

  $target = &tmp;
}

%typemap(tcl8,out) ib_slvl_table_t* {
  char buff[64];
  int i;
  int entry;
  if ($source != NULL)
  {
    for (i = 0; i < 8; i++)
	 {
		 entry = $source->raw_vl_by_sl[i];
		 sprintf(buff, "0x%02x 0x%02x ", ((entry & 0xf0) >> 4), (entry & 0xf));
		 Tcl_AppendToObj($target,buff,strlen(buff));
	 }
  }
  else
  {
    Tcl_SetStringObj($target, "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ", -1);
  }
}

%typemap(tcl8,in) ib_slvl_table_t* (ib_slvl_table_t tmp) {
  int i;
  int entry, value;
  int numEntries, code;
  const char **subListStrings;

  code = Tcl_SplitList(interp, Tcl_GetStringFromObj($source,NULL),
							  &numEntries, &subListStrings);
  if (code != TCL_OK) {
	 printf("Wrong format for ib_slvl_table_t should be list:%s\n",
			  Tcl_GetStringFromObj($source,NULL));
	 return TCL_ERROR;
  }
  if (numEntries > 16) {
	 printf("Maximal number of SL2VL entries is 16:%s\n",
			  Tcl_GetStringFromObj($source,NULL));
	 Tcl_Free((char *) subListStrings);
	 return TCL_ERROR;
  }
  memset(&tmp, 0, sizeof(ib_slvl_table_t));
  for (i = 0; i < numEntries; i++) {
	  errno = 0;
	  value = strtoul(subListStrings[i],NULL,0);
	  if (errno) {
		  printf("Wrong format for vl_arb_table sublist %d vl:%s\n",
					i, subListStrings[i]);
		  Tcl_Free((char *) subListStrings);
		  return TCL_ERROR;
	  }
	  if (value > 15) {
		  printf("Given VL at index %d is %d > 15\n", i, value);
		  Tcl_Free((char *) subListStrings);
		  return TCL_ERROR;
	  }
	  entry = tmp.raw_vl_by_sl[i/2];
	  if (i % 2) {
		  entry = (value & 0xf) | (entry & 0xf0) ;
	  } else {
		  entry = ((value & 0xf) << 4) | (entry & 0xf);
	  }
	  tmp.raw_vl_by_sl[i/2] = entry;
  }
  Tcl_Free((char *) subListStrings);

  $target = &tmp;
}

%typemap(tcl8,out) ib_guid_info_t* {
  char buff[36];
  int i;
  if ($source != NULL)
  {
    for (i = 0; i < 8; i++)
    {
      sprintf(buff, "0x%016" PRIx64, cl_ntoh64($source->guid[i]));
      Tcl_AppendToObj($target,buff,strlen(buff));
    }
  }
  else
  {
    Tcl_SetStringObj($target, "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", 64);
  }
}

%typemap(tcl8,in) ib_guid_info_t* (ib_guid_info_t tmp) {
  char buf[256];
  char *p_guid;
  char *str_token = NULL;
  int i = 0;
  memset(&tmp, 0, sizeof(ib_guid_info_t));

  strncpy(buf, Tcl_GetStringFromObj($source,NULL), 255);
  buf[255] = '\0';
  p_guid = strtok_r(buf," ", &str_token);
  while (p_guid && (i < 8))
  {
    errno = 0;
    tmp.guid[i++] = cl_hton64(strtoul(p_guid, NULL, 0));
    if (errno) {
      printf("Wrong format for guid:%s\n", p_guid);
      return TCL_ERROR;
    }

    p_guid = strtok_r(NULL," ", &str_token);
  }
  $target = &tmp;
}

%typemap(tcl8,out) ib_pkey_table_t* {
  char buff[36];
  int i;
  if ($source != NULL)
  {
    for (i = 0; i < 32; i++)
    {
      sprintf(buff, "0x%04x ", cl_ntoh16($source->pkey_entry[i]));
      Tcl_AppendToObj($target,buff,strlen(buff));
    }
  }
  else
  {
    Tcl_SetStringObj($target, "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", 64);
  }
}

%typemap(tcl8,in) ib_pkey_table_t* (ib_pkey_table_t tmp) {
  char buf[256];
  char *p_pkey;
  char *str_token = NULL;
  int i = 0;
  memset(&tmp, 0, sizeof(ib_pkey_table_t));

  strncpy(buf, Tcl_GetStringFromObj($source,NULL), 255);
  buf[255] = '\0';
  p_pkey = strtok_r(buf," ", &str_token);
  while (p_pkey && (i < 32))
  {
    errno = 0;
    tmp.pkey_entry[i++] = cl_hton16(strtoul(p_pkey, NULL, 0));
    if (errno) {
      printf("Wrong format for pkey:%s\n", p_pkey);
      return TCL_ERROR;
    }

    p_pkey = strtok_r(NULL," ", &str_token);
  }
  $target = &tmp;
}

%typemap(tcl8,out) ib_mft_table_t* {
  char buff[36];
  int i;
  for (i = 0; i < IB_MCAST_BLOCK_SIZE; i++)
  {
    sprintf(buff, "0x%04x ", cl_ntoh16($source->mft_entry[i]));
    Tcl_AppendToObj($target,buff,strlen(buff));
  }
}

%typemap(tcl8,ignore) ib_mft_table_t *OUTPUT(ib_mft_table_t temp) {
  $target = &temp;
}

%typemap(tcl8,argout) ib_mft_table_t *OUTPUT {
  /* Argout ib_mft_table_t */
  char buff[36];
  int i;
  /* HACK if we did not have the result show an error ... */
  if (!_result)
  {
    /* we need to cleanup the result 0 ... */
    Tcl_ResetResult(interp);
    for (i = 0; i < IB_MCAST_BLOCK_SIZE; i++)
    {
      sprintf(buff, "0x%04x ", cl_ntoh16($source->mft_entry[i]));
      Tcl_AppendToObj($target,buff,strlen(buff));
    }
  }
}

%typemap(tcl8,in) ib_mft_table_t* (ib_mft_table_t tmp) {
  char buf[256];
  char *p_mftEntry;
  char *str_token = NULL;
  int i = 0;

  strncpy(buf, Tcl_GetStringFromObj($source,NULL), 255);
  buf[255] = '\0';
  p_mftEntry = strtok_r(buf," ", &str_token);
  while (p_mftEntry && (i < IB_MCAST_BLOCK_SIZE))
  {
    errno = 0;
    tmp.mft_entry[i++] = cl_hton16(strtoul(p_mftEntry, NULL, 0));
    if (errno) {
      printf("Wrong format for MFT Entry:%s\n", p_mftEntry);
      return TCL_ERROR;
    }

    p_mftEntry = strtok_r(NULL," ", &str_token);
  }
  while (i < IB_MCAST_BLOCK_SIZE)
  {
    tmp.mft_entry[i++] = 0;
  }
  $target = &tmp;
}

%{
#define uint8_array_t uint8_t
%}
%typemap(memberin)  uint8_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%{
#define uint32_array_t uint32_t
%}
%typemap(memberin)  uint32_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

typedef struct _ib_node_info
{
	uint8_t				base_version;
	uint8_t				class_version;
	uint8_t				node_type;
	uint8_t				num_ports;
	ib_net64_t			sys_guid;
	ib_net64_t			node_guid;
	ib_net64_t			port_guid;
	ib_net16_t			partition_cap;
	ib_net16_t			device_id;
	ib_net32_t			revision;
	ib_net32_t			port_num_vendor_id;
} ib_node_info_t;

typedef struct _ib_switch_info
{
	ib_net16_t			lin_cap;
	ib_net16_t			rand_cap;
	ib_net16_t			mcast_cap;
	ib_net16_t			lin_top;
	uint8_t				def_port;
	uint8_t				def_mcast_pri_port;
	uint8_t				def_mcast_not_port;
	uint8_t				life_state;
	ib_net16_t			lids_per_port;
	ib_net16_t			enforce_cap;
	uint8_t				flags;

} ib_switch_info_t;

typedef struct _ib_port_info
{
	ib_net64_t			m_key;
	ib_net64_t			subnet_prefix;
	ib_net16_t			base_lid;
	ib_net16_t			master_sm_base_lid;
	ib_net32_t			capability_mask;
	ib_net16_t			diag_code;
	ib_net16_t			m_key_lease_period;
	uint8_t				local_port_num;
	uint8_t				link_width_enabled;
	uint8_t				link_width_supported;
	uint8_t				link_width_active;
	uint8_t				state_info1; // LinkSpeedSupported and PortState
	uint8_t				state_info2; // PortPhysState and LinkDownDefaultState
	uint8_t				mkey_lmc;
	uint8_t				link_speed;	 // LinkSpeedEnabled and LinkSpeedActive
	uint8_t				mtu_smsl;
	uint8_t				vl_cap;		 // VlCap and InitType
	uint8_t				vl_high_limit;
	uint8_t				vl_arb_high_cap;
	uint8_t				vl_arb_low_cap;
	uint8_t				mtu_cap;
	uint8_t				vl_stall_life;
	uint8_t				vl_enforce;
	ib_net16_t			m_key_violations;
	ib_net16_t			p_key_violations;
	ib_net16_t			q_key_violations;
	uint8_t				guid_cap;
	uint8_t				subnet_timeout;
	uint8_t				resp_time_value;
	uint8_t				error_threshold;
} ib_port_info_t;

typedef struct _ib_node_desc
{
	// Node String is an array of UTF-8 character that
	// describes the node in text format
	// Note that this string is NOT NULL TERMINATED!
	uint8_array_t description[IB_NODE_DESCRIPTION_SIZE];
} ib_node_desc_t;

typedef struct _ib_lft_record
{
	ib_net16_t		lid;
	ib_net16_t		block_num;
	uint32_t		   resv0;
	uint8_array_t  lft[64];
} ib_lft_record_t;

typedef struct _ib_pm_counters {
  ib_mad_t mad_header;
  uint32_array_t reserved0[10];
  uint8_t reserved1;
  uint8_t port_select;
  ib_net16_t counter_select;
  ib_net16_t symbol_error_counter;
  uint8_t link_error_recovery_counter;
  uint8_t link_down_counter;
  ib_net16_t port_rcv_errors;
  ib_net16_t port_rcv_remote_physical_errors;
  ib_net16_t port_rcv_switch_relay_errors;
  ib_net16_t port_xmit_discard;
  uint8_t port_xmit_constraint_errors;
  uint8_t port_rcv_constraint_errors;
  uint8_t reserved2;
  uint8_t lli_errors_exc_buf_errors;
  ib_net16_t reserved3;
  ib_net16_t vl15_dropped;
  ib_net32_t port_xmit_data;
  ib_net32_t port_rcv_data;
  ib_net32_t port_xmit_pkts;
  ib_net32_t port_rcv_pkts;
  uint32_array_t reserved5[38];
} ib_pm_counters_t;
