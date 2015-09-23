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

  Congestion Control:
    The CC interface supports sending Congestion Control Packets to
    the fabric devices.

    A set of object types is defined. The user can allocate a new attribute
    object. Then the object can be set using configure or the direct
    manipulation methods.

    Extra methods are provided for each object:
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

	/* the global pointer to this CC mads manager */
	static ibcc_t *gp_ibcc;

	/* we probably want to use our own naming for classes */
	typedef ib_class_port_info_t       ccClassPortInfo;
	typedef ibcc_notice_attr_t         ccNotice;
	typedef ib_cong_info_t             ccCongestionInfo;
	typedef ib_cong_key_info_t         ccCongestionKeyInfo;
	typedef ibcc_ca_cong_log_t         ccCACongestionLog;
	typedef ibcc_sw_cong_log_t         ccSWCongestionLog;
	typedef ib_sw_cong_setting_t       ccSWCongestionSetting;
	typedef ib_sw_port_cong_setting_t  ccSWPortCongestionSetting;
	typedef ib_ca_cong_setting_t       ccCACongestionSetting;
	typedef ib_cc_tbl_t                ccTable;
	typedef ib_time_stamp_t            ccTimeStamp;

	/* these are the global objects to be used
	   for set/get (one for each attribute) */
	ib_class_port_info_t               ibcc_class_port_info_obj;
	ibcc_notice_attr_t                 ibcc_notice_obj;
	ib_cong_info_t                     ibcc_cong_info_obj;
	ib_cong_key_info_t                 ibcc_cong_key_info_obj;
	ibcc_ca_cong_log_t                 ibcc_ca_cong_log_obj;
	ibcc_sw_cong_log_t                 ibcc_sw_cong_log_obj;
	ib_sw_cong_setting_t               ibcc_sw_cong_setting_obj;
	ib_sw_port_cong_setting_t          ibcc_sw_port_cong_setting_obj;
	ib_ca_cong_setting_t               ibcc_ca_cong_setting_obj;
	ib_cc_tbl_t                        ibcc_table_obj;
	ib_time_stamp_t                    ibcc_time_stamp_obj;

%}

//
// STANDARD IB TYPE MAPS:
//

%typemap(tcl8,in) ib_gid_t*(ib_gid_t temp)
{
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

//
// INTERFACE DEFINITION (~copy of h file)
//

%section "IBCC Constants"
/* These constants are provided by IBCC: */

%section "IBCC Objects",pre
/* This section describes the various object types exposed by IBCC. */
%text %{

%}

/**************************************************
 ***        ClassPortInfo (A10.4.3.1)           ***
 **************************************************/

typedef struct _ibcc_class_port_info {
	uint8_t     base_ver;
	uint8_t     class_ver;
	ib_net16_t  cap_mask;
	ib_net32_t  cap_mask2_resp_time;
	ib_gid_t    redir_gid;
	ib_net32_t  redir_tc_sl_fl;
	ib_net16_t  redir_lid;
	ib_net16_t  redir_pkey;
	ib_net32_t  redir_qp;
	ib_net32_t  redir_qkey;
	ib_gid_t    trap_gid;
	ib_net32_t  trap_tc_sl_fl;
	ib_net16_t  trap_lid;
	ib_net16_t  trap_pkey;
	ib_net32_t  trap_hop_qp;
	ib_net32_t  trap_qkey;
} ccClassPortInfo;

%addmethods ccClassPortInfo {

	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccClassPortInfo), // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CLASS_PORT_INFO),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}

	int setByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid(
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccClassPortInfo), // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CLASS_PORT_INFO),
			0,                       // attribute modifier
			IB_MAD_METHOD_SET);
	}
}

/**************************************************
 ***       Notice - Trap 0 (A10.4.3.2)          ***
 **************************************************/

typedef struct _ibcc_notice
{
	uint8_t       generic_type;

	uint8_t       generic__prod_type_msb;
	ib_net16_t    generic__prod_type_lsb;
	ib_net16_t    generic__trap_num;

	ib_net16_t    issuer_lid;
	ib_net16_t    toggle_count;

	ib_net16_t    ntc0__source_lid;   // Source LID from offending packet LRH
	uint8_t       ntc0__method;       // Method, from common MAD header
	uint8_t       ntc0__resv0;
	ib_net16_t    ntc0__attr_id;      // Attribute ID, from common MAD header
	ib_net16_t    ntc0__resv1;
	ib_net32_t    ntc0__attr_mod;     // Attribute Modif, from common MAD header
	ib_net32_t    ntc0__qp;           // 8b pad, 24b dest QP from BTH
	ib_net64_t    ntc0__cc_key;       // CC key of the offending packet
	ib_gid_t      ntc0__source_gid;   // GID from GRH of the offending packet
	uint8_array_t ntc0__padding[14];  // Padding - ignored on read

	ib_gid_t      issuer_gid;
} ccNotice;

%addmethods ccNotice {
	int trapByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid(
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccNotice),        // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_NOTICE),
			0,                       // attribute modifier
			IB_MAD_METHOD_TRAP);
	}
}

/**************************************************
 ***       CongestionInfo (A10.4.3.3)           ***
 **************************************************/

typedef struct _ib_cong_info {
	uint8_t cong_info;
	uint8_t resv;
	uint8_t ctrl_table_cap;
} ccCongestionInfo;

%addmethods ccCongestionInfo {
	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                     // log data
			0,                        // log data size
			(uint8_t *)self,          // mgt data
			sizeof(ccCongestionInfo), // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CONG_INFO),
			0,                        // attribute modifier
			IB_MAD_METHOD_GET);
	}
}

/**************************************************
 ***      CongestionKeyInfo (A10.4.3.4)         ***
 **************************************************/

typedef struct _ib_cong_key_info {
	ib_net64_t cc_key;
	ib_net16_t protect_bit;
	ib_net16_t lease_period;
	ib_net16_t violations;
} ccCongestionKeyInfo;

%addmethods ccCongestionKeyInfo {

	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccCongestionKeyInfo), // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CONG_KEY_INFO),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}

	int setByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid(
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccCongestionKeyInfo), // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CONG_KEY_INFO),
			0,                       // attribute modifier
			IB_MAD_METHOD_SET);
	}
}

/**************************************************
 ***        CongestionLog (A10.4.3.5)           ***
 **************************************************/

/*
 * IBA defined CongestionLog attribute (A10.4.3.5)
 * has the following definition:
 *
 *	typedef struct _ib_cong_log {
 *		uint8_t log_type;
 *		union _log_details
 *		{
 *			struct {
 *				uint8_t cong_flags;
 *				ib_net16_t event_counter;
 *				ib_net32_t time_stamp;
 *				uint8_array_t port_map[32];
 *				ib_cong_log_event_sw_t entry_list[15];
 *			} log_sw;
 *
 *			struct {
 *				uint8_t cong_flags;
 *				ib_net16_t event_counter;
 *				ib_net16_t event_map;
 *				ib_net16_t resv;
 *				ib_net32_t time_stamp;
 *				ib_cong_log_event_ca_t log_event[13];
 *			} log_ca;
 *
 *		} log_details;
 *	} ccCongestionLog;
 *
 * This definition has a union that includes Congestion Log for
 * switches and CAs.
 * Instead of dealing with the union in SWIG, we define two separate logs:
 *   ccSWCongestionLog (for dealing only with switches)
 *   ccCACongestionLog (for dealing only with CAs)
 */

%typemap(in) ib_cong_log_event_sw_t[ANY] (ib_cong_log_event_sw_t entrys[$dim0]) {
	long int value;
	long int entry_index = 0;
	int k;
	int countSubLists, numElements;
	int i = 0;
	int option = 0;
	Tcl_Obj ** subListObjArray;
	Tcl_Obj  * tclObj;

	if (Tcl_ListObjGetElements(interp, $source, &countSubLists, &subListObjArray) != TCL_OK)
	{
		printf("Error: wrong format for SW Congestion Log Event: %s\n",
			Tcl_GetStringFromObj($source,NULL));
		return TCL_ERROR;
	}

	/* SW Congestion Log Event List should have up to 15 events */
	if (countSubLists > 15)
	{
		printf("Error: SW Congestion Log Event List should have up to %d events (provided %d)\n",
			15, countSubLists);
		return TCL_ERROR;
	}

	/*
	 * There are two options to configure log_event:
	 *   1. Configure the whole list by providing list of value groups:
	 *        ccSWCongestionLogMad configure -log_event {{1 2 3 4} {5 6 7 8}}
	 *   2. Configure specific items from the list by providing index in addition to the above:
	 *        ccSWCongestionLogMad configure -log_event {{4 1 2 3 4} {8 5 6 7 8}}
	 */

	if (countSubLists > 0) {

		/* check how many members does the first substring have */

		if (Tcl_ListObjLength(interp, subListObjArray[0], &numElements) != TCL_OK) {
			printf("Error: wrong format for SW Congestion Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}

		if (numElements == 4)
			option = 1;
		else if (numElements == 5)
			option = 2;
		else {
			printf("Error: wrong number of elements for SW Congestion Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}
	}
	else {
		/* if the user didn't specify anything, use option 1,
		   wich will effectively clear the whole list values */
		option = 1;
	}

	for (i = 0; i < $dim0; i++) {
		entrys[i].slid = 0;
		entrys[i].dlid = 0;
		entrys[i].sl = 0;
		entrys[i].time_stamp = 0;
	}

	if (option == 1) {
		/*
		 * first option - list of groups of four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for SW Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 4) {
				printf("Error: wrong number of elements for SW Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 4; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of SW Congestion Log Event: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entrys[i].slid = cl_hton16(value); break;
					case 1: entrys[i].dlid = cl_hton16(value); break;
					case 2: entrys[i].sl = cl_hton32(value); break;
					case 3: entrys[i].time_stamp = cl_hton32(value); break;
					default: break;
				}
			}
		}

	}
	else {
		/*
		 * second option - index and four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for SW Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 5) {
				printf("Error: wrong number of elements for SW Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 5; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of SW Congestion Log Event: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entry_index = value; break;
					case 1: entrys[entry_index].slid = cl_hton16(value); break;
					case 2: entrys[entry_index].dlid = cl_hton16(value); break;
					case 3: entrys[entry_index].sl = cl_hton32(value); break;
					case 4: entrys[entry_index].time_stamp = cl_hton32(value); break;
					default: break;
				}
			}

		}
	}

	$target = entrys;
}

%typemap(memberin) ib_cong_log_event_sw_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(tcl8,out) ib_cong_log_event_sw_t[ANY] {
	int i;
	char buff[99];

	sprintf(buff, "-entry_list\n ");
	Tcl_AppendResult(interp, buff, NULL);

	for (i=0; i <$dim0 ; i++) {
		sprintf(buff, " {#%02u:", i);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -slid 0x%04x", cl_ntoh16($source[i].slid));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -dlid 0x%04x", cl_ntoh16($source[i].dlid));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -sl 0x%08x", cl_ntoh32($source[i].sl));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -time_stamp 0x%08x", cl_ntoh32($source[i].time_stamp));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, "}\n ");
		Tcl_AppendResult(interp, buff, NULL);
	}
}

typedef struct _ib_cong_log_event_sw {
	ib_net16_t slid;
	ib_net16_t dlid;
	ib_net32_t sl;
	ib_net32_t time_stamp;
} ib_cong_log_event_sw_t;

/**************************************************/

%typemap(in) ib_cong_log_event_ca_t[ANY] (ib_cong_log_event_ca_t entrys[$dim0]) {
	long int value;
	long int entry_index = 0;
	int k;
	int countSubLists, numElements;
	int i = 0;
	int option = 0;
	Tcl_Obj ** subListObjArray;
	Tcl_Obj  * tclObj;

	if (Tcl_ListObjGetElements(interp, $source, &countSubLists, &subListObjArray) != TCL_OK)
	{
		printf("Error: wrong format for CA Congestion Log Event: %s\n",
			Tcl_GetStringFromObj($source,NULL));
		return TCL_ERROR;
	}

	/* CA Congestion Log Event List should have up to 13 events */
	if (countSubLists > 13)
	{
		printf("Error: CA Congestion Log Event List should have up to %d events (provided %d)\n",
			13, countSubLists);
		return TCL_ERROR;
	}

	/*
	 * There are two options to configure log_event:
	 *   1. Configure the whole list by providing list of value groups:
	 *        ccCACongestionLogMad configure -log_event {{1 2 3 4} {5 6 7 8}}
	 *   2. Configure specific items from the list by providing index in addition to the above:
	 *        ccCACongestionLogMad configure -log_event {{4 1 2 3 4} {8 5 6 7 8}}
	 */

	if (countSubLists > 0) {

		/* check how many members does the first substring have */

		if (Tcl_ListObjLength(interp, subListObjArray[0], &numElements) != TCL_OK) {
			printf("Error: wrong format for CA Congestion Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}

		if (numElements == 4)
			option = 1;
		else if (numElements == 5)
			option = 2;
		else {
			printf("Error: wrong number of elements for CA Congestion Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}
	}
	else {
		/* if the user didn't specify anything, use option 1,
		   wich will effectively clear the whole list values */
		option = 1;
	}

	for (i = 0; i < $dim0; i++) {
		entrys[i].local_qp_resv0 = 0;
		entrys[i].remote_qp_sl_service_type = 0;
		entrys[i].remote_lid = 0;
		entrys[i].time_stamp = 0;
	}

	if (option == 1) {
		/*
		 * first option - list of groups of four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for CA Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 4) {
				printf("Error: wrong number of elements for CA Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 4; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of CA Congestion Log Event: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entrys[i].local_qp_resv0 = cl_hton32(value); break;
					case 1: entrys[i].remote_qp_sl_service_type = cl_hton32(value); break;
					case 2: entrys[i].remote_lid = cl_hton16(value); break;
					case 3: entrys[i].time_stamp = cl_hton32(value); break;
					default: break;
				}
			}
		}

	}
	else {
		/*
		 * second option - index and four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for CA Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 5) {
				printf("Error: wrong number of elements for CA Congestion Log Event: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 5; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of CA Congestion Log Event: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entry_index = value; break;
					case 1: entrys[entry_index].local_qp_resv0 = cl_hton32(value); break;
					case 2: entrys[entry_index].remote_qp_sl_service_type = cl_hton32(value); break;
					case 3: entrys[entry_index].remote_lid = cl_hton16(value); break;
					case 4: entrys[entry_index].time_stamp = cl_hton32(value); break;
					default: break;
				}
			}

		}
	}

	$target = entrys;
}

%typemap(memberin) ib_cong_log_event_ca_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(tcl8,out) ib_cong_log_event_ca_t[ANY] {
	int i;
	char buff[99];

	sprintf(buff, "-log_event\n ");
	Tcl_AppendResult(interp, buff, NULL);

	for (i=0; i <$dim0 ; i++) {
		sprintf(buff, " {#%02u:", i);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -local_qp_resv0 0x%08x", cl_ntoh32($source[i].local_qp_resv0));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -remote_qp_sl_service_type 0x%08x", cl_ntoh32($source[i].remote_qp_sl_service_type));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -remote_lid 0x%04x", cl_ntoh16($source[i].remote_lid));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -time_stamp 0x%08x", cl_ntoh32($source[i].time_stamp));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, "}\n ");
		Tcl_AppendResult(interp, buff, NULL);
	}
}

typedef struct _ib_cong_log_event_ca {
	ib_net32_t local_qp_resv0;
	ib_net32_t remote_qp_sl_service_type;
	ib_net16_t remote_lid;
	ib_net16_t resv1;
	ib_net32_t time_stamp;
} ib_cong_log_event_ca_t;

/**************************************************/

typedef struct _ib_sw_cong_log {
	uint8_t log_type;
	uint8_t cong_flags;
	ib_net16_t event_counter;
	ib_net32_t time_stamp;
	uint8_array_t port_map[32];
	ib_cong_log_event_sw_t entry_list[15];
} ccSWCongestionLog;

%addmethods ccSWCongestionLog {
	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			(uint8_t *)self,           // log data
			sizeof(ccSWCongestionLog), // log data size
			NULL,                      // mgt data
			0,                         // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CONG_LOG),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}
}

typedef struct _ib_ca_cong_log {
	uint8_t log_type;
	uint8_t cong_flags;
	ib_net16_t event_counter;
	ib_net16_t event_map;
	ib_net16_t resv;
	ib_net32_t time_stamp;
	ib_cong_log_event_ca_t log_event[13];
} ccCACongestionLog;

%addmethods ccCACongestionLog {
	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			(uint8_t *)self,           // log data
			sizeof(ccCACongestionLog), // log data size
			NULL,                      // mgt data
			0,                         // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CONG_LOG),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}
}


/**************************************************
 ***    SwitchCongestionSetting (A10.4.3.6)     ***
 **************************************************/

typedef struct _ib_sw_cong_setting {
	ib_net32_t control_map;
	uint8_array_t victim_mask[32];
	uint8_array_t credit_mask[32];
	uint8_t threshold_resv;
	uint8_t packet_size;
	ib_net16_t cs_threshold_resv;
	ib_net16_t cs_return_delay;
	ib_net16_t marking_rate;
} ccSWCongestionSetting;


%addmethods ccSWCongestionSetting {

	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccSWCongestionSetting),// mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_SW_CONG_SETTING),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}

	int setByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccSWCongestionSetting),// mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_SW_CONG_SETTING),
			0,                       // attribute modifier
			IB_MAD_METHOD_SET);
	}

}

/**************************************************
 ***   SwitchPortCongestionSetting (A10.4.3.7)  ***
 **************************************************/

%typemap(in) ib_sw_port_cong_setting_element_t[ANY] (ib_sw_port_cong_setting_element_t entrys[$dim0]) {
	long int value;
	long int entry_index = 0;
	int k;
	int countSubLists, numElements;
	int i = 0;
	int option = 0;
	Tcl_Obj ** subListObjArray;
	Tcl_Obj  * tclObj;

	if (Tcl_ListObjGetElements(interp, $source, &countSubLists, &subListObjArray) != TCL_OK)
	{
		printf("Error: wrong format for SW Port Congestion Setting Element: %s\n",
			Tcl_GetStringFromObj($source,NULL));
		return TCL_ERROR;
	}

	/* SwitchPortCongestionSetting Block list should have up to 32 blocks */
	if (countSubLists > 32)
	{
		printf("Error: SwitchPortCongestionSetting Block list should have up to %d blocks (provided %d)\n",
			32, countSubLists);
		return TCL_ERROR;
	}

	/*
	 * There are two options to configure entry_list:
	 *   1. Configure the whole list by providing list of value groups:
	 *        ccSWPortCongestionSettingMad configure -block {{1 2 3} {4 5 6}}
	 *   2. Configure specific items from the list by providing index in addition to the above:
	 *        ccSWPortCongestionSettingMad configure -block {{4 1 2 3} {8 4 5 6}}
	 */

	if (countSubLists > 0) {

		/* check how many members does the first substring have */

		if (Tcl_ListObjLength(interp, subListObjArray[0], &numElements) != TCL_OK) {
			printf("Error: wrong format for SW Port Congestion Setting Element: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}

		if (numElements == 3)
			option = 1;
		else if (numElements == 4)
			option = 2;
		else {
			printf("Error: wrong number of elements for SW Port Congestion Setting Element: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}
	}
	else {
		/* if the user didn't specify anything, use option 1,
		   wich will effectively clear the whole list values */
		option = 1;
	}

	for (i = 0; i < $dim0; i++) {
		entrys[i].valid_ctrl_type_res_threshold = 0;
		entrys[i].packet_size = 0;
		entrys[i].cong_param = 0;
	}

	if (option == 1) {
		/*
		 * first option - list of groups of four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for SW Port Congestion Setting Element: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 3) {
				printf("Error: wrong number of elements for SW Port Congestion Setting Element: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 3; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of SW Port Congestion Setting Element: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entrys[i].valid_ctrl_type_res_threshold = value; break;
					case 1: entrys[i].packet_size = value; break;
					case 2: entrys[i].cong_param = cl_hton16(value); break;
					default: break;
				}
			}
		}

	}
	else {
		/*
		 * second option - index and four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for SW Port Congestion Setting Element: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 4) {
				printf("Error: wrong number of elements for SW Port Congestion Setting Element: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 4; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of SW Port Congestion Setting Element: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entry_index = value; break;
					case 1: entrys[entry_index].valid_ctrl_type_res_threshold = value; break;
					case 2: entrys[entry_index].packet_size = value; break;
					case 3: entrys[entry_index].cong_param = cl_hton16(value); break;
					default: break;
				}
			}

		}
	}

	$target = entrys;
}

%typemap(memberin) ib_sw_port_cong_setting_element_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(tcl8,out) ib_sw_port_cong_setting_element_t[ANY] {
	int i;
	char buff[99];

	sprintf(buff, "-block\n ");
	Tcl_AppendResult(interp, buff, NULL);

	for (i=0; i <$dim0 ; i++) {
		sprintf(buff, " {#%02u:", i);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -valid_ctrl_type_res_threshold 0x%02x", $source[i].valid_ctrl_type_res_threshold);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -packet_size 0x%02x", $source[i].packet_size);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -cong_param 0x%04x", cl_ntoh16($source[i].cong_param));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, "}\n ");
		Tcl_AppendResult(interp, buff, NULL);
	}
}

typedef struct _ib_sw_port_cong_setting_element {
	uint8_t valid_ctrl_type_res_threshold;
	uint8_t packet_size;
	ib_net16_t cong_param;
} ib_sw_port_cong_setting_element_t;

typedef struct _ib_sw_port_cong_setting {
	ib_sw_port_cong_setting_element_t block[32];
} ccSWPortCongestionSetting;

%addmethods ccSWPortCongestionSetting {

	int getByLid(uint16_t lid, uint8_t block) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccSWPortCongestionSetting),// mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_SW_PORT_CONG_SETTING),
			block,                   // attribute modifier
			IB_MAD_METHOD_GET);
	}

	int setByLid(uint16_t lid, uint8_t block) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccSWPortCongestionSetting),// mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_SW_PORT_CONG_SETTING),
			block,                   // attribute modifier
			IB_MAD_METHOD_SET);
	}

}

/**************************************************
 ***     CACongestionSetting (A10.4.3.8)        ***
 **************************************************/

%typemap(in) ib_ca_cong_entry_t[ANY] (ib_ca_cong_entry_t entrys[$dim0]) {
	long int value;
	long int entry_index = 0;
	int k;
	int countSubLists, numElements;
	int i = 0;
	int option = 0;
	Tcl_Obj ** subListObjArray;
	Tcl_Obj  * tclObj;

	if (Tcl_ListObjGetElements(interp, $source, &countSubLists, &subListObjArray) != TCL_OK)
	{
		printf("Error: wrong format for CA Congestion Setting: %s\n",
			Tcl_GetStringFromObj($source,NULL));
		return TCL_ERROR;
	}

	/* CA Congestion Setting Entry List should have up to 16 entries */
	if (countSubLists > 16)
	{
		printf("Error: CA Congestion Setting Entry List should have up to %d entries (provided %d)\n",
			16, countSubLists);
		return TCL_ERROR;
	}

	/*
	 * There are two options to configure entry_list:
	 *   1. Configure the whole list by providing list of value groups:
	 *        ccCACongestionSetting configure -entry_list {{1 2 3 4} {5 6 7 8}}
	 *   2. Configure specific items from the list by providing index in addition to the above:
	 *        ccCACongestionSetting configure -entry_list {{4 1 2 3 4} {8 5 6 7 8}}
	 */

	if (countSubLists > 0) {

		/* check how many members does the first substring have */

		if (Tcl_ListObjLength(interp, subListObjArray[0], &numElements) != TCL_OK) {
			printf("Error: wrong format for CA Congestion Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}

		if (numElements == 4)
			option = 1;
		else if (numElements == 5)
			option = 2;
		else {
			printf("Error: wrong number of elements for CA Congestion Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}
	}
	else {
		/* if the user didn't specify anything, use option 1,
		   wich will effectively clear the whole list values */
		option = 1;
	}

	for (i = 0; i < $dim0; i++) {
		entrys[i].ccti_timer = 0;
		entrys[i].ccti_increase = 0;
		entrys[i].trigger_threshold = 0;
		entrys[i].ccti_min = 0;
	}

	if (option == 1) {
		/*
		 * first option - list of groups of four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for CA Congestion Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 4) {
				printf("Error: wrong number of elements for CA Congestion Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 4; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of CA Congestion Entry: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entrys[i].ccti_timer = cl_hton16(value); break;
					case 1: entrys[i].ccti_increase = value; break;
					case 2: entrys[i].trigger_threshold = value; break;
					case 3: entrys[i].ccti_min = value; break;
					default: break;
				}
			}
		}

	}
	else {
		/*
		 * second option - index and four values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for CA Congestion Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 5) {
				printf("Error: wrong number of elements for CA Congestion Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			for (k = 0; k < 5; k++) {

				if (Tcl_ListObjIndex(interp, subListObjArray[i], k, &tclObj) != TCL_OK) {
					printf("Error: Fail to obtain the element of CA Congestion Entry: %s\n",
						Tcl_GetStringFromObj(subListObjArray[i],NULL));
					return TCL_ERROR;
				}
				value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);
				switch (k) {
					case 0: entry_index = value; break;
					case 1: entrys[entry_index].ccti_timer = cl_hton16(value); break;
					case 2: entrys[entry_index].ccti_increase = value; break;
					case 3: entrys[entry_index].trigger_threshold = value; break;
					case 4: entrys[entry_index].ccti_min = value; break;
					default: break;
				}
			}

		}
	}

	$target = entrys;
}

%typemap(memberin) ib_ca_cong_entry_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(tcl8,out) ib_ca_cong_entry_t[ANY] {
	int i;
	char buff[99];

	sprintf(buff, "-entry_list\n ");
	Tcl_AppendResult(interp, buff, NULL);

	for (i=0; i <$dim0 ; i++) {
		sprintf(buff, " {SL%02u:", i);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -ccti_timer 0x%04x", cl_ntoh16($source[i].ccti_timer));
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -ccti_increase 0x%02x", $source[i].ccti_increase);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -trigger_threshold 0x%02x", $source[i].trigger_threshold);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, " -ccti_min 0x%02x", $source[i].ccti_min);
		Tcl_AppendResult(interp, buff, NULL);

		sprintf(buff, "}\n ");
		Tcl_AppendResult(interp, buff, NULL);
	}
}

typedef struct _ib_ca_cong_entry {
	ib_net16_t ccti_timer;
	uint8_t ccti_increase;
	uint8_t trigger_threshold;
	uint8_t ccti_min;
	uint8_t resv0;
	ib_net16_t resv1;
} ib_ca_cong_entry_t;

typedef struct _ib_ca_cong_setting {
	ib_net16_t port_control;
	ib_net16_t control_map;
	ib_ca_cong_entry_t entry_list[16];
} ccCACongestionSetting;


%addmethods ccCACongestionSetting {

	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccCACongestionSetting),// mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CA_CONG_SETTING),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}

	int setByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccCACongestionSetting),// mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CA_CONG_SETTING),
			0,                       // attribute modifier
			IB_MAD_METHOD_SET);
	}

}

/**************************************************
 ***     CongestionControlTable (A10.4.3.9)     ***
 **************************************************/

%typemap(in) ib_cc_tbl_entry_t[ANY] (ib_cc_tbl_entry_t entrys[$dim0]) {
	long int value;
	long int entry_index = 0;
	int countSubLists, numElements;
	int i = 0;
	int option = 0;
	Tcl_Obj ** subListObjArray;
	Tcl_Obj  * tclObj;

	if (Tcl_ListObjGetElements(interp, $source, &countSubLists, &subListObjArray) != TCL_OK)
	{
		printf("Error: wrong format for CC Table: %s\n",
			Tcl_GetStringFromObj($source,NULL));
		return TCL_ERROR;
	}

	/* Congestion Control Table Entry List should have up to 64 entries */
	if (countSubLists > 64)
	{
		printf("Error: Congestion Control Table Entry List should have up to %d entries (provided %d)\n",
			64, countSubLists);
		return TCL_ERROR;
	}

	/*
	 * There are two options to configure entry_list:
	 *   1. Configure the whole list by providing list of values:
	 *        ccTableMad configure -entry_list {1 2 3 4 5 6}
	 *   2. Configure specific items from the list by providing index and value:
	 *        ccTableMad configure -entry_list {{1 3} {2 4} {7 16}}
	 */

	if (countSubLists > 0) {

		/* check how many members does the first substring have */

		if (Tcl_ListObjLength(interp, subListObjArray[0], &numElements) != TCL_OK) {
			printf("Error: wrong format for CC Table Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}

		if (numElements == 1)
			option = 1;
		else if (numElements == 2)
			option = 2;
		else {
			printf("Error: wrong number of elements for CC Table Entry: %s\n",
				Tcl_GetStringFromObj(subListObjArray[0],NULL));
			return TCL_ERROR;
		}
	}
	else {
		/* if the user didn't specify anything, use option 1,
		   wich will effectively clear the whole list values */
		option = 1;
	}

	for (i = 0; i < $dim0; i++)
		entrys[i].shift_multiplier = 0;

	if (option == 1) {
		/*
		 * first option - list of values
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 1) {
				printf("Error: wrong number of elements for CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (Tcl_ListObjIndex(interp, subListObjArray[i], 0, &tclObj) != TCL_OK) {
				printf("Error: Fail to obtain the element of CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);

			entrys[i].shift_multiplier = cl_hton16(value);
		}

	}
	else {
		/*
		 * second option - index and value
		 */

		for (i = 0; i < countSubLists; i++) {

			if (Tcl_ListObjLength(interp, subListObjArray[i], &numElements) != TCL_OK) {
				printf("Error: wrong format for CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (numElements != 2) {
				printf("Error: wrong number of elements for CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			if (Tcl_ListObjIndex(interp, subListObjArray[i], 0, &tclObj) != TCL_OK) {
				printf("Error: Fail to obtain the element of CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			entry_index = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);

			if (Tcl_ListObjIndex(interp, subListObjArray[i], 1, &tclObj) != TCL_OK) {
				printf("Error: Fail to obtain the element of CC Table Entry: %s\n",
					Tcl_GetStringFromObj(subListObjArray[i],NULL));
				return TCL_ERROR;
			}

			value = strtol(Tcl_GetStringFromObj(tclObj, NULL), NULL, 0);

			entrys[entry_index].shift_multiplier = cl_hton16(value);
		}
	}

	$target = entrys;
}

%typemap(memberin) ib_cc_tbl_entry_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(tcl8,out) ib_cc_tbl_entry_t[ANY] {
	int i;
	char buff[99];

	sprintf(buff, "-entry_list { ");
	Tcl_AppendResult(interp, buff, NULL);

	for (i=0; i <$dim0 ; i++) {
		sprintf(buff, "{#%02u: 0x%04x} ", i, cl_ntoh16($source[i].shift_multiplier));
		Tcl_AppendResult(interp, buff, NULL);
	}

	sprintf(buff, "} ");
	Tcl_AppendResult(interp, buff, NULL);
}

typedef struct _ibcc_tbl_entry {
	ib_net16_t shift_multiplier;
} ib_cc_tbl_entry_t;

typedef struct _ib_cc_tbl {
	ib_net16_t ccti_limit;
	ib_net16_t resv;
	ib_cc_tbl_entry_t entry_list[64];
} ccTable;

%addmethods ccTable {

	int getByLid(uint16_t lid, uint8_t sn) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccTable),         // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CC_TBL),
			sn,                      // attribute modifier
			IB_MAD_METHOD_GET);
	}

	int setByLid(uint16_t lid, uint8_t sn) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccTable),         // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_CC_TBL),
			sn,                      // attribute modifier
			IB_MAD_METHOD_SET);
	}

}

/**************************************************
 ***           TimeStamp (A10.4.3.10)           ***
 **************************************************/

typedef struct _ib_time_stamp {
	ib_net32_t value;
} ccTimeStamp;

%addmethods ccTimeStamp {

	int getByLid(uint16_t lid) {
		return ibcc_send_mad_by_lid (
			gp_ibcc,
			IBCC_DEAFULT_KEY,
			NULL,                    // log data
			0,                       // log data size
			(uint8_t *)self,         // mgt data
			sizeof(ccTimeStamp),     // mgt data size
			lid,
			CL_NTOH16(IB_MAD_ATTR_TIME_STAMP),
			0,                       // attribute modifier
			IB_MAD_METHOD_GET);
	}

}
