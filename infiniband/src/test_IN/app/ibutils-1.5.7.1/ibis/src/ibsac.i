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

%{
#include "stdio.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <iba/ib_types.h>
#include <complib/cl_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_map.h>
#include <complib/cl_debug.h>
#include "ibis_api.h"
#ifdef OSM_BUILD_OPENIB
#include <vendor/osm_vendor_sa_api.h>
#else
#include <opensm/osm_vendor_sa_api.h>
#endif
#include "ibsac.h"

  /*
     TODO:  Add the following queries
     ClassPortInfo
     InformInfo
     SLtoVLMappingTableRecord
     VLArbitrationTableRecord
     ServiceRecord
     P_KeyTableRecord

     Not supported by OpenSM:
     Notice
     MulticastForwardingTableRecord
     InformInfoRecord

     Later:
     MultiPathRecord
  */

  /* we probably want to use our own naming for classes */
  typedef ib_node_record_t         sacNodeRec;
  typedef ib_node_info_t           sacNodeInfo;
  typedef ib_portinfo_record_t     sacPortRec;
  typedef ib_port_info_t           sacPortInfo;
  typedef ib_sminfo_record_t       sacSmRec;
  typedef ib_sm_info_t             sacSmInfo;
  typedef ib_switch_info_record_t  sacSwRec;
  typedef ib_switch_info_t         sacSwInfo;
  typedef ib_link_record_t         sacLinkRec;
  typedef ib_path_rec_t            sacPathRec;
  typedef ib_lft_record_t          sacLFTRec;
  typedef ib_member_rec_t          sacMCMRec;
  typedef ib_class_port_info_t     sacClassPortInfo;
  typedef ib_inform_info_t         sacInformInfo;
  typedef ib_service_record_t      sacServiceRec;
  typedef ib_slvl_table_t          sacSlVlTbl;
  typedef ib_slvl_table_record_t   sacSlVlRec;
  typedef ib_vl_arb_table_record_t sacVlArbRec;
  typedef ib_pkey_table_t          sacPKeyTbl;
  typedef ib_pkey_table_record_t   sacPKeyRec;
  typedef ib_guid_info_t           sacGuidInfo;
  typedef ib_guidinfo_record_t     sacGuidRec;
  typedef uint8_t                  ib_lft_t;
%}

%{

#include "swig_extended_obj.c"

  /* Pre allocated Query Objects */
  ib_node_record_t          ibsac_node_rec;             // ib_node_info_t
  ib_portinfo_record_t      ibsac_portinfo_rec;         // ib_port_info_t
  ib_sminfo_record_t        ibsac_sminfo_rec;           // ib_sm_info_t
  ib_switch_info_record_t   ibsac_swinfo_rec;           // ib_switch_info_t
  ib_link_record_t          ibsac_link_rec;             // no sub type
  ib_path_rec_t             ibsac_path_rec;             // no sub type
  ib_lft_record_t           ibsac_lft_rec;              // no sub type
  ib_member_rec_t           ibsac_mcm_rec;              // no sub type
  ib_class_port_info_t      ibsac_class_port_info;      // no sub type
  ib_inform_info_t          ibsac_inform_info;          // no sub type
  ib_service_record_t       ibsac_svc_rec;              // no sub type
  ib_slvl_table_record_t    ibsac_slvl_rec;             // ib_slvl_table_t
  ib_vl_arb_table_record_t  ibsac_vlarb_rec;            // ib_vl_arb_table_t
  ib_pkey_table_record_t    ibsac_pkey_rec;             // ib_pkey_table_t
  ib_guidinfo_record_t      ibsac_guidinfo_rec;         // ib_guid_info_t

  /* Query Functions for each record */
  /* These are TCL specific thus are here */

  char *ibsacNodeRecordQuery(
	 ib_node_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_node_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_NODE_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_node_record_t *)malloc(sizeof(ib_node_record_t));

		/* copy into it */
		*p_rec = *(osmv_get_query_node_rec( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("nr",p_rec);
		SWIG_AltMnglRegObj("ni",&(p_rec->node_info));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "nr", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacPortInfoRecordQuery(
	 ib_portinfo_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_portinfo_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_PORTINFO_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_portinfo_record_t *)malloc(sizeof(ib_portinfo_record_t));

		/* copy into it */
		*p_rec = *(osmv_get_query_portinfo_rec( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("pir",p_rec);
		SWIG_AltMnglRegObj("pi",&(p_rec->port_info));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "pir", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacSMInfoRecordQuery(
	 ib_sminfo_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_sminfo_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_SMINFO_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_sminfo_record_t *)malloc(sizeof(ib_sminfo_record_t));

		/* copy into it */
		*p_rec = *((ib_sminfo_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("smir",p_rec);
		SWIG_AltMnglRegObj("smi",&(p_rec->sm_info));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "smir", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacSwitchInfoRecordQuery(
	 ib_switch_info_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_switch_info_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, CL_NTOH16(0x0014), self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_switch_info_record_t *)malloc(sizeof(ib_switch_info_record_t));

		/* copy into it */
		*p_rec = *((ib_switch_info_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("swir",p_rec);
		SWIG_AltMnglRegObj("swi",&(p_rec->switch_info));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "swir", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacPathRecordQuery(
	 ib_path_rec_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_path_rec_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_PATH_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_path_rec_t *)malloc(sizeof(ib_path_rec_t));

		/* copy into it */
		*p_rec = *((ib_path_rec_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("path",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "path", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacLinkRecordQuery(
	 ib_link_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_link_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_LINK_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_link_record_t *)malloc(sizeof(ib_link_record_t));

		/* copy into it */
		*p_rec = *((ib_link_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("link",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "link", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacLFTRecordQuery(
	 ib_lft_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_lft_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_LFT_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_lft_record_t *)malloc(sizeof(ib_lft_record_t));

		/* copy into it */
		*p_rec = *((ib_lft_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("lft",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "lft", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacMCMemberRecordQuery(
	 ib_member_rec_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_member_rec_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_MCMEMBER_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_member_rec_t *)malloc(sizeof(ib_member_rec_t));

		/* copy into it */
		*p_rec = *((ib_member_rec_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("mcm",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "mcm", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacClassPortInfoQuery(
	 ib_class_port_info_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_class_port_info_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_CLASS_PORT_INFO, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_class_port_info_t *)malloc(sizeof(ib_class_port_info_t));

		/* copy into it */
		*p_rec = *((ib_class_port_info_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("cpi",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "cpi", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacInformInfoQuery(
	 ib_inform_info_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_inform_info_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj,  IB_MAD_ATTR_INFORM_INFO, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_inform_info_t *)malloc(sizeof(ib_inform_info_t));

		/* copy into it */
		*p_rec = *((ib_inform_info_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("info",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "info", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }


  char *ibsacServiceRecordQuery(
	 ib_service_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_service_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj,  IB_MAD_ATTR_SERVICE_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_service_record_t *)malloc(sizeof(ib_service_record_t));

		/* copy into it */
		*p_rec = *((ib_service_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("svc",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "svc", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacSl2VlRecordQuery(
	 ib_slvl_table_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_slvl_table_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_SLVL_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_slvl_table_record_t *)malloc(sizeof(ib_slvl_table_record_t));

		/* copy into it */
		*p_rec = *((ib_slvl_table_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("slvr",p_rec);
		SWIG_AltMnglRegObj("slvt",&(p_rec->slvl_tbl));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "slvr", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacVlArbRecordQuery(
	 ib_vl_arb_table_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_vl_arb_table_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_VLARB_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_vl_arb_table_record_t *)malloc(sizeof(ib_vl_arb_table_record_t));

		/* copy into it */
		*p_rec = *((ib_vl_arb_table_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("vlarb",p_rec);

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "vlarb", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

  char *ibsacPKeyRecordQuery(
	 ib_pkey_table_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_pkey_table_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_PKEY_TBL_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_pkey_table_record_t *)malloc(sizeof(ib_pkey_table_record_t));

		/* copy into it */
		*p_rec = *((ib_pkey_table_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("pkr",p_rec);
		SWIG_AltMnglRegObj("pkt",&(p_rec->pkey_tbl));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "pkr", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }

char *ibsacGuidRecordQuery(
	 ib_guidinfo_record_t *self,
	 uint64_t comp_mask,
	 uint8_t method) {
	 ib_guidinfo_record_t *p_rec;
	 uint32_t i;
	 ib_api_status_t status;
	 uint32_t num_recs = 0;
	 osm_madw_t *p_result_madw;
	 char *p_res_str = NULL, *tmp;
	 Tcl_Obj *p_tclObj;
	 int nameLength;

	 status = ibsac_query(
		&IbisObj, IB_MAD_ATTR_GUIDINFO_RECORD, self, comp_mask, method,
		&num_recs, &p_result_madw
		);

	 for( i = 0; i < num_recs; i++ )
	 {
		/* we need to create a new node info and copy */
		p_rec = (ib_guidinfo_record_t *)malloc(sizeof(ib_guidinfo_record_t));

		/* copy into it */
		*p_rec = *((ib_guidinfo_record_t*)osmv_get_query_result( p_result_madw, i ));

		/* register it as a new object */
		SWIG_AltMnglRegObj("gr",p_rec);
		SWIG_AltMnglRegObj("gi",&(p_rec->guid_info));

		p_tclObj = Tcl_NewObj();

		/* get the assigned name */
		if (SWIG_AltMnglGetObjNameByPtr(p_tclObj, "gr", p_rec)) {
		  printf("-E- Fail to get name of object %p\n", p_rec);
		} else {
		  tmp = Tcl_GetStringFromObj(p_tclObj, &nameLength);

		  /* enlarge the result string length */
		  if (p_res_str)
			 p_res_str = (char *)realloc(p_res_str, strlen(p_res_str) + nameLength + 2);
		  else {
			 p_res_str = (char *)malloc(nameLength + 2);
			 p_res_str[0] = '\0';
		  }

		  strcat(p_res_str, tmp);
		  strcat(p_res_str, " ");
		}
		Tcl_DecrRefCount(p_tclObj);
	 }

	 if( p_result_madw != NULL )
		osm_mad_pool_put( &IbisObj.mad_pool, p_result_madw );

	 return(p_res_str);
  }


%}

//
// STANDARD IB TYPE MAPS:
//

%typemap(tcl8,in) ib_gid_t*(ib_gid_t temp) {
  char buf[40];
  char *p_prefix, *p_guid;
  char *str_token = NULL;

  strcpy(buf, Tcl_GetStringFromObj($source,NULL));
  p_prefix = strtok_r(buf,":", &str_token);
  if (! p_prefix)
  {
    printf("Wrong format for gid prefix:%s\n", Tcl_GetStringFromObj($source,NULL));
    return TCL_ERROR;
  }
  p_guid = strtok_r(NULL, " ", &str_token);
  if (! p_guid)
  {
    printf("Wrong format for gid prefix:%s\n", Tcl_GetStringFromObj($source,NULL));
    return TCL_ERROR;
  }

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

%typemap(tcl8,out) ib_node_desc_t * {
  /* we must make sure we do not overflow the node desc length */
  char buff[IB_NODE_DESCRIPTION_SIZE];
  strncpy(buff,(char *)$source,IB_NODE_DESCRIPTION_SIZE - 1);
  buff[IB_NODE_DESCRIPTION_SIZE - 1] = '\0';
  Tcl_SetStringObj($target, buff, strlen(buff));
}

%typemap(tcl8,out) ib_lft_t * {
  char buf[12];
  int i;
  for (i = 0; i < 64; i++) {
	 sprintf(buf, "{%u %u} ", i, $source[i]);
	 Tcl_AppendToObj($target, buf, -1);
  }
}

%typemap(tcl8,memberin) ib_lft_t[ANY] {
  int i;
  int m = $dim0;
  if (m > 64) m = 64;
  for (i=0; i <$dim0 ; i++) {
    $target[i] = *($source+i);
  }
}

%typemap(tcl8,in) ib_node_desc_t *(ib_node_desc_t temp) {
  strcpy((char *)temp.description, Tcl_GetStringFromObj($source,NULL));
  $target = &temp;
}


//
// INTERFACE DEFINITION (~copy of h file)
//

%section "IBSAC Constants"
/* These constants are provided by IBIS: */

%section "IBSAC Objects",pre
/* This section describes the various object types exposed by IBSAC. */
%text %{

  IBSAC exposes some of its internal subnet objects. The objects
  identifiers returned by the various function calls are formatted
  acording to the following rules:
  ni:<idx> = node info objects

  IBIS Objects are standard Swig-Tcl objects.
  As such they have two flavors for their usage: Variables, Objects.

  Variables:
     For each object attribute a "get" and "set" methods are provided.
	  The format of the methods is: <class>_<attribute>_<get|set>.
     The "set" method is only available for read/write attributes.

  Example:
  ib_node_info_t_port_guid_get ni:1

  Objects:
     Given an object identifier one can convert it to a Tcl "Object"
	  using the following command:
     <class> <obj_name> -this <obj identifier>

     Once declared the <obj-name> can be used in conjunction to
     with the standard "configure" and "cget" commands.

  Example: (following the previous one):
		 ib_node_info_t nodeRecNI -this ni:1

%}

#define IB_NR_COMPMASK_LID					0x1
#define IB_NR_COMPMASK_RESERVED1			0x2
#define IB_NR_COMPMASK_BASEVERSION		0x4
#define IB_NR_COMPMASK_CLASSVERSION		0x8
#define IB_NR_COMPMASK_NODETYPE			0x10
#define IB_NR_COMPMASK_NUMPORTS			0x20
#define IB_NR_COMPMASK_SYSIMAGEGUID		0x40
#define IB_NR_COMPMASK_NODEGUID			0x80
#define IB_NR_COMPMASK_PORTGUID			0x100
#define IB_NR_COMPMASK_PARTCAP			0x200
#define IB_NR_COMPMASK_DEVID				0x400
#define IB_NR_COMPMASK_REV					0x800
#define IB_NR_COMPMASK_PORTNUM			0x1000
#define IB_NR_COMPMASK_VENDID				0x2000
#define IB_NR_COMPMASK_NODEDESC			0x4000

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

} sacNodeInfo;

%addmethods sacNodeInfo {
  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

typedef struct _ib_node_record_t
{
	ib_net16_t		lid;
	sacNodeInfo 	node_info;
	ib_node_desc_t node_desc;
} sacNodeRec;

%addmethods sacNodeRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacNodeRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacNodeRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
	 /* we need to de-register both the node info and node record */
	 SWIG_AltMnglUnregObj(&(self->node_info));
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	PORT INFO
	-----------------------------------------------------*/

#define IB_PIR_COMPMASK_LID				  "0x1"
#define IB_PIR_COMPMASK_PORTNUM			  "0x2"
#define IB_PIR_COMPMASK_RESV1				  "0x4"
#define IB_PIR_COMPMASK_MKEY				  "0x8"
#define IB_PIR_COMPMASK_GIDPRE			  0x10
#define IB_PIR_COMPMASK_BASELID			  0x20
#define IB_PIR_COMPMASK_SMLID				  0x40
#define IB_PIR_COMPMASK_CAPMASK			  0x80
#define IB_PIR_COMPMASK_DIAGCODE			  0x100
#define IB_PIR_COMPMASK_MKEYLEASEPRD	  0x200
#define IB_PIR_COMPMASK_LOCALPORTNUM	  0x400
#define IB_PIR_COMPMASK_LNKWIDTHSUPPORT  0x800
#define IB_PIR_COMPMASK_LNKWIDTHACTIVE	  0x1000
#define IB_PIR_COMPMASK_LINKWIDTHENABLED 0x2000
#define IB_PIR_COMPMASK_LNKSPEEDSUPPORT  0x4000
#define IB_PIR_COMPMASK_PORTSTATE		  0x10000
#define IB_PIR_COMPMASK_PORTPHYSTATE	  0x20000
#define IB_PIR_COMPMASK_LINKDWNDFLTSTATE 0x40000
#define IB_PIR_COMPMASK_MKEYPROTBITS	  0x80000
#define IB_PIR_COMPMASK_LMC				  0x100000
#define IB_PIR_COMPMASK_LINKSPEEDACTIVE  0x200000
#define IB_PIR_COMPMASK_LINKSPEEDENABLE  0x400000
#define IB_PIR_COMPMASK_NEIGHBORMTU		  0x800000
#define IB_PIR_COMPMASK_MASTERSMSL		  0x1000000
#define IB_PIR_COMPMASK_VLCAP				  0x2000000
#define IB_PIR_COMPMASK_INITTYPE			  0x4000000
#define IB_PIR_COMPMASK_VLHIGHLIMIT		  0x8000000
#define IB_PIR_COMPMASK_VLARBHIGHCAP	  0x10000000
#define IB_PIR_COMPMASK_VLARBLOWCAP		  0x20000000
#define IB_PIR_COMPMASK_INITTYPEREPLY	  0x40000000
#define IB_PIR_COMPMASK_MTUCAP			  0x80000000
#define IB_PIR_COMPMASK_VLSTALLCNT		  "0x100000000"
#define IB_PIR_COMPMASK_HOQLIFE			  "0x200000000"
#define IB_PIR_COMPMASK_OPVLS		    	  "0x400000000"
#define IB_PIR_COMPMASK_PARENFIN		     "0x800000000"
#define IB_PIR_COMPMASK_PARENFOUT		  "0x1000000000"
#define IB_PIR_COMPMASK_FILTERRAWIN		  "0x2000000000"
#define IB_PIR_COMPMASK_FILTERRAWOUT	  "0x4000000000"
#define IB_PIR_COMPMASK_MKEYVIO			  "0x8000000000"
#define IB_PIR_COMPMASK_PKEYVIO			  "0x10000000000"
#define IB_PIR_COMPMASK_QKEYVIO			  "0x20000000000"
#define IB_PIR_COMPMASK_GUIDCAP			  "0x40000000000"
#define IB_PIR_COMPMASK_RESV2			     "0x80000000000"
#define IB_PIR_COMPMASK_SUBNTO			  "0x100000000000"
#define IB_PIR_COMPMASK_RESV3			     "0x200000000000"
#define IB_PIR_COMPMASK_RESPTIME		     "0x400000000000"
#define IB_PIR_COMPMASK_LOCALPHYERR		  "0x800000000000"
#define IB_PIR_COMPMASK_OVERRUNERR		  "0x1000000000000"

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
	uint8_t				state_info1;	// LinkSpeedSupported and PortState
	uint8_t				state_info2;	// PortPhysState and LinkDownDefaultState
	uint8_t				mkey_lmc;
	uint8_t				link_speed;		// LinkSpeedEnabled and LinkSpeedActive
	uint8_t				mtu_smsl;
   uint8_t				vl_cap;        // VlCap and InitType
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

} sacPortInfo;

%addmethods sacPortInfo {
  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

typedef struct _ib_portinfo_record
{
	ib_net16_t		lid;
	uint8_t			port_num;
	sacPortInfo   	port_info;
} sacPortRec;

%addmethods sacPortRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacPortInfoRecordQuery(self, cl_hton64(comp_mask),
                                    IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacPortInfoRecordQuery(self, cl_hton64(comp_mask),
                                    IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
	 /* we need to de-register both the node info and node record */
	 SWIG_AltMnglUnregObj(&(self->port_info));
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	SM INFO
	-----------------------------------------------------*/

#define IB_SMR_COMPMASK_LID				  "0x1"
#define IB_SMR_COMPMASK_GUID             "0x2"
#define IB_SMR_COMPMASK_SM_KEY           "0x4"
#define IB_SMR_COMPMASK_ACT_COUNT 	     "0x8"
#define IB_SMR_COMPMASK_STATE  		     0x10
#define IB_SMR_COMPMASK_PRI       	     0x20

typedef struct _ib_sm_info
{
  ib_net64_t			guid;
  ib_net64_t			sm_key;
  ib_net32_t			act_count;
  uint8_t				pri_state;
} sacSmInfo;

%addmethods sacSmInfo {
  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

typedef struct _ib_sminfo_record
{
  ib_net16_t		lid;
  sacSmInfo	      sm_info;
} sacSmRec;

%addmethods sacSmRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacSMInfoRecordQuery(self, cl_hton64(comp_mask),
                                  IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacSMInfoRecordQuery(self, cl_hton64(comp_mask),
                                  IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
	 /* we need to de-register both the node info and node record */
	 SWIG_AltMnglUnregObj(&(self->sm_info));
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	SWITCH INFO
	-----------------------------------------------------*/

#define IB_SWR_COMPMASK_LID			    "0x1"
#define IB_SWR_COMPMASK_LIN_CAP         "0x2"
#define IB_SWR_COMPMASK_RAND_CAP	       "0x4"
#define IB_SWR_COMPMASK_MCAST_CAP	    "0x8"
#define IB_SWR_COMPMASK_LIN_TOP		    "0x10"
#define IB_SWR_COMPMASK_DEF_PORT  	    "0x20"
#define IB_SWR_COMPMASK_DEF_MCAST_PRI   "0x40"
#define IB_SWR_COMPMASK_DEF_MCAST_NOT   "0x80"
#define IB_SWR_COMPMASK_STATE 		    "0x100"
#define IB_SWR_COMPMASK_LIFE    	       "0x200"
#define IB_SWR_COMPMASK_LMC 			    "0x400"
#define IB_SWR_COMPMASK_ENFORCE_CAP     "0x800"
#define IB_SWR_COMPMASK_FLAGS		       "0x1000"

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

} sacSwInfo;

%addmethods sacSwInfo {
  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

typedef struct _ib_switch_info_record
{
  ib_net16_t			lid;
  sacSwInfo       	switch_info;
} sacSwRec;

%addmethods sacSwRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacSwitchInfoRecordQuery(self, cl_hton64(comp_mask),
                                      IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacSwitchInfoRecordQuery(self, cl_hton64(comp_mask),
                                      IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
	 /* we need to de-register both the node info and node record */
	 SWIG_AltMnglUnregObj(&(self->switch_info));
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	LINK
	-----------------------------------------------------*/

#define IB_LR_COMPMASK_FROM_LID   0x1
#define IB_LR_COMPMASK_FROM_PORT  0x2
#define IB_LR_COMPMASK_TO_PORT    0x4
#define IB_LR_COMPMASK_TO_LID     0x8

typedef struct _ib_link_record
{
	ib_net16_t		from_lid;
	uint8_t			from_port_num;
	uint8_t			to_port_num;
	ib_net16_t		to_lid;
} sacLinkRec;

%addmethods sacLinkRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacLinkRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacLinkRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	PATH
	-----------------------------------------------------*/

#define  IB_PR_COMPMASK_DGID              0x4
#define  IB_PR_COMPMASK_SGID              0x8
#define  IB_PR_COMPMASK_DLID              0x10
#define  IB_PR_COMPMASK_SLID              0x20
#define  IB_PR_COMPMASK_RAWTRAFIC         0x40
#define  IB_PR_COMPMASK_RESV0             0x80
#define  IB_PR_COMPMASK_FLOWLABEL         0x100
#define  IB_PR_COMPMASK_HOPLIMIT          0x200
#define  IB_PR_COMPMASK_TCLASS            0x400
#define  IB_PR_COMPMASK_REVERSIBLE        0x800
#define  IB_PR_COMPMASK_NUMBPATH          0x1000
#define  IB_PR_COMPMASK_PKEY              0x2000
#define  IB_PR_COMPMASK_RESV1             0x4000
#define  IB_PR_COMPMASK_SL                0x8000
#define  IB_PR_COMPMASK_MTUSELEC          0x10000
#define  IB_PR_COMPMASK_MTU               0x20000
#define  IB_PR_COMPMASK_RATESELEC         0x40000
#define  IB_PR_COMPMASK_RATE              0x80000
#define  IB_PR_COMPMASK_PKTLIFETIMESELEC  0x100000
#define  IB_PR_COMPMASK_PFTLIFETIME       0x200000

typedef struct _ib_path_rec
{
        ib_net64_t service_id;
        ib_gid_t dgid;
        ib_gid_t sgid;
        ib_net16_t dlid;
        ib_net16_t slid;
        ib_net32_t hop_flow_raw;
        uint8_t tclass;
        uint8_t num_path;
        ib_net16_t pkey;
        ib_net16_t qos_class_sl;
        uint8_t mtu;
        uint8_t rate;
        uint8_t pkt_life;
        uint8_t preference;
} sacPathRec;

%addmethods sacPathRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacPathRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacPathRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
    free(self);
	 SWIG_AltMnglUnregObj(self);
  }
}

/* -----------------------------------------------------
	LFT
	-----------------------------------------------------*/

#define IB_LFT_COMPMASK_LID    "0x1"
#define IB_LFT_COMPMASK_BLOCK  "0x2"

typedef struct _ib_lft_record
{
	ib_net16_t		lid;
	ib_net16_t		block_num;
	ib_lft_t	      lft[64];
} sacLFTRec;

%addmethods sacLFTRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacLFTRecordQuery(self, cl_hton64(comp_mask),
                               IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacLFTRecordQuery(self, cl_hton64(comp_mask),
                               IB_MAD_METHOD_GETTABLE));
  };
  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	MCM
	-----------------------------------------------------*/

#define IB_MCR_COMPMASK_GID         "0x1"
#define IB_MCR_COMPMASK_MGID        "0x1"
#define IB_MCR_COMPMASK_PORT_GID    "0x2"
#define IB_MCR_COMPMASK_QKEY        "0x4"
#define IB_MCR_COMPMASK_MLID        "0x8"
#define IB_MCR_COMPMASK_MTU_SEL     "0x10"
#define IB_MCR_COMPMASK_MTU         "0x20"
#define IB_MCR_COMPMASK_TCLASS      "0x40"
#define IB_MCR_COMPMASK_PKEY        "0x80"
#define IB_MCR_COMPMASK_RATE_SEL    "0x100"
#define IB_MCR_COMPMASK_RATE        "0x200"
#define IB_MCR_COMPMASK_LIFE_SEL    "0x400"
#define IB_MCR_COMPMASK_LIFE        "0x800"
#define IB_MCR_COMPMASK_SL          "0x1000"
#define IB_MCR_COMPMASK_FLOW        "0x2000"
#define IB_MCR_COMPMASK_HOP         "0x4000"
#define IB_MCR_COMPMASK_SCOPE       "0x8000"
#define IB_MCR_COMPMASK_JOIN_STATE  "0x10000"
#define IB_MCR_COMPMASK_PROXY       "0x20000"

typedef struct _ib_member_rec
{
	ib_gid_t				mgid;
	ib_gid_t				port_gid;
	ib_net32_t			qkey;
	ib_net16_t			mlid;
	uint8_t				mtu;
	uint8_t				tclass;
	ib_net16_t			pkey;
	uint8_t				rate;
	uint8_t				pkt_life;
	ib_net32_t			sl_flow_hop;
	uint8_t				scope_state;
  // uint8_t				proxy_join:1; bit fields are not supported.
} sacMCMRec;

%addmethods sacMCMRec {
%new char *get(uint64_t comp_mask) {
	 return(ibsacMCMemberRecordQuery(self, cl_hton64(comp_mask),
                                    IB_MAD_METHOD_GET));
  };
%new char *getTable(uint64_t comp_mask) {
	 return(ibsacMCMemberRecordQuery(self, cl_hton64(comp_mask),
                                    IB_MAD_METHOD_GETTABLE));
  };
 %new char *set(uint64_t comp_mask) {
   return(ibsacMCMemberRecordQuery(self, cl_hton64(comp_mask),
                                   IB_MAD_METHOD_SET));
 };
 %new char *del(uint64_t comp_mask) {
   return(ibsacMCMemberRecordQuery(self, cl_hton64(comp_mask),
                                   IB_MAD_METHOD_DELETE));
 };

 int proxy_join_get() {
   return(self->proxy_join);
 }

  void proxy_join_set(uint8_t proxy_join) {
    self->proxy_join = proxy_join;
  }

  void obj_delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }

}

/* -----------------------------------------------------
	Class Port Info
	-----------------------------------------------------*/
typedef struct _ibsac_class_port_info
{
	uint8_t					base_ver;
	uint8_t					class_ver;
	ib_net16_t				cap_mask;
	ib_net32_t				cap_mask2_resp_time;
	ib_gid_t				redir_gid;
	ib_net32_t				redir_tc_sl_fl;
	ib_net16_t				redir_lid;
	ib_net16_t				redir_pkey;
	ib_net32_t				redir_qp;
	ib_net32_t				redir_qkey;
	ib_gid_t				trap_gid;
	ib_net32_t				trap_tc_sl_fl;
	ib_net16_t				trap_lid;
	ib_net16_t				trap_pkey;
	ib_net32_t				trap_hop_qp;
	ib_net32_t				trap_qkey;
} sacClassPortInfo;

%addmethods sacClassPortInfo {
  %new char *get() {
    return(ibsacClassPortInfoQuery(self, 0, IB_MAD_METHOD_GET));
  };

  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	Inform Info
	-----------------------------------------------------*/
typedef struct _ibsac_inform_info
{
  ib_gid_t				   gid;
  ib_net16_t				lid_range_begin;
  ib_net16_t				lid_range_end;
  ib_net16_t				reserved1;
  uint8_t					is_generic;
  uint8_t					subscribe;
  ib_net16_t				trap_type;
  union _sac_inform_g_or_v
  {
	 struct _sac_inform_generic
	 {
		ib_net16_t		trap_num;
		ib_net32_t		qpn_resp_time_val;
      uint8_t        reserved2;
		uint8_t			node_type_msb;
		ib_net16_t		node_type_lsb;
	 } generic;

	 struct _sac_inform_vend
	 {
		ib_net16_t		dev_id;
		ib_net32_t		qpn_resp_time_val;
      uint8_t        reserved2;
		uint8_t			vendor_id_msb;
		ib_net16_t		vendor_id_lsb;
	 } vend;

  } g_or_v;

} sacInformInfo;

%addmethods sacInformInfo {
  %new char *set() {
    return(ibsacInformInfoQuery(self, 0, IB_MAD_METHOD_SET));
  };

  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	Service Record
	-----------------------------------------------------*/

#define IB_SR_COMPMASK_SID           "0x1"
#define IB_SR_COMPMASK_SGID          "0x1"
#define IB_SR_COMPMASK_SPKEY         "0x2"
#define IB_SR_COMPMASK_RES1          "0x4"
#define IB_SR_COMPMASK_SLEASE        "0x8"
#define IB_SR_COMPMASK_SKEY          "0x10"
#define IB_SR_COMPMASK_SNAME         "0x20"
#define IB_SR_COMPMASK_SDATA8_0      "0x40"
#define IB_SR_COMPMASK_SDATA8_1      "0x80"
#define IB_SR_COMPMASK_SDATA8_2      "0x100"
#define IB_SR_COMPMASK_SDATA8_3      "0x200"
#define IB_SR_COMPMASK_SDATA8_4      "0x400"
#define IB_SR_COMPMASK_SDATA8_5      "0x800"
#define IB_SR_COMPMASK_SDATA8_6      "0x1000"
#define IB_SR_COMPMASK_SDATA8_7      "0x2000"
#define IB_SR_COMPMASK_SDATA8_8      "0x4000"
#define IB_SR_COMPMASK_SDATA8_9      "0x8000"
#define IB_SR_COMPMASK_SDATA8_10     "0x10000"
#define IB_SR_COMPMASK_SDATA8_11     "0x20000"
#define IB_SR_COMPMASK_SDATA8_12     "0x40000"
#define IB_SR_COMPMASK_SDATA8_13     "0x80000"
#define IB_SR_COMPMASK_SDATA8_14     "0x100000"
#define IB_SR_COMPMASK_SDATA8_15     "0x200000"
#define IB_SR_COMPMASK_SDATA16_0     "0x400000"
#define IB_SR_COMPMASK_SDATA16_1     "0x800000"
#define IB_SR_COMPMASK_SDATA16_2     "0x1000000"
#define IB_SR_COMPMASK_SDATA16_3     "0x2000000"
#define IB_SR_COMPMASK_SDATA16_4     "0x4000000"
#define IB_SR_COMPMASK_SDATA16_5     "0x8000000"
#define IB_SR_COMPMASK_SDATA16_6     "0x10000000"
#define IB_SR_COMPMASK_SDATA16_7     "0x20000000"
#define IB_SR_COMPMASK_SDATA32_0     "0x40000000"
#define IB_SR_COMPMASK_SDATA32_1     "0x80000000"
#define IB_SR_COMPMASK_SDATA32_2     "0x100000000"
#define IB_SR_COMPMASK_SDATA32_3     "0x200000000"
#define IB_SR_COMPMASK_SDATA64_0     "0x400000000"
#define IB_SR_COMPMASK_SDATA64_1     "0x800000000"

%typemap(tcl8,in) ib_svc_name_t*(ib_svc_name_t n) {
  char *p_name;
  int l;
  p_name = Tcl_GetStringFromObj($source, &l);
  if (l > 63) l = 63;
  memcpy(n, p_name, l);
  $target = &n;
}

%typemap(tcl8,memberin) ib_svc_name_t {
  memcpy(&($target), $source, sizeof(ib_svc_name_t));
}

%typemap(tcl8,out) ib_svc_name_t* {
  char buff[64];
  strncpy(buff, (char *)(*$source), 63);
  buff[63] = '\0';
  Tcl_SetStringObj($target,buff, strlen(buff));
}


typedef struct _ib_service_record
{
	ib_net64_t		service_id;
	ib_gid_t		   service_gid;
	ib_net16_t		service_pkey;
	ib_net16_t		resv;
	ib_net32_t		service_lease;
	uint8_array_t	service_key[16];
	ib_svc_name_t	service_name;
	uint8_array_t	service_data8[16];
	ib_net16_array_t service_data16[8];
	ib_net32_array_t service_data32[4];
	ib_net64_array_t service_data64[2];
} sacServiceRec;

%addmethods sacServiceRec {
  %new char *get(uint64_t comp_mask) {
    return(ibsacServiceRecordQuery(self, cl_hton64(comp_mask),
                                   IB_MAD_METHOD_GET));
  };
  %new char *set(uint64_t comp_mask) {
    return(ibsacServiceRecordQuery(self, cl_hton64(comp_mask),
                                   IB_MAD_METHOD_SET));
  };
  %new char *getTable(uint64_t comp_mask) {
    return(ibsacServiceRecordQuery(self, cl_hton64(comp_mask),
                                   IB_MAD_METHOD_GETTABLE));
  };
  %new char *delete(uint64_t comp_mask) {
    return(ibsacServiceRecordQuery(self, cl_hton64(comp_mask),
                                   IB_MAD_METHOD_DELETE));
  };

  void obj_delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	SL to VL Table Record
	-----------------------------------------------------*/
#define IB_SLVL_COMPMASK_LID             "0x1"
#define IB_SLVL_COMPMASK_IN_PORT         "0x2"
#define IB_SLVL_COMPMASK_OUT_PORT        "0x4"
typedef struct _ib_slvl_table
{
	uint8_array_t raw_vl_by_sl[IB_MAX_NUM_VLS/2];
} sacSlVlTbl;

typedef struct _ib_slvl_table_record
{
	ib_net16_t		 lid; // for CA: lid of port, for switch lid of port 0
	uint8_t			 in_port_num;	// reserved for CA's
	uint8_t			 out_port_num;	// reserved for CA's
	uint32_t	  	    resv;
	sacSlVlTbl      slvl_tbl;
} sacSlVlRec;

%addmethods sacSlVlRec {
  %new char *get(uint64_t comp_mask) {
    return(ibsacSl2VlRecordQuery(self, cl_hton64(comp_mask),
                                 IB_MAD_METHOD_GET));
  };
  %new char *getTable(uint64_t comp_mask) {
    return(ibsacSl2VlRecordQuery(self, cl_hton64(comp_mask),
                                 IB_MAD_METHOD_GETTABLE));
  };

  void delete() {
	 SWIG_AltMnglUnregObj(&(self->slvl_tbl));
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
   VL Arb Table Record
	-----------------------------------------------------*/
#define IB_VLA_COMPMASK_LID              "0x1"
#define IB_VLA_COMPMASK_OUT_PORT         "0x2"
#define IB_VLA_COMPMASK_BLOCK            "0x4"

%typemap(tcl8,in) sac_vl_arb_tbl_t* (ib_vl_arb_table_t n) {
  uint8_t idx;
  char *p_vl_str, *p_wt_str;
  unsigned long int vl, weight;
#if TCL_MINOR_VERSION > 3
  const char **sub_lists;
#else
  char **sub_lists;
#endif
  int num_sub_lists;
  memset( &n, 0, sizeof(ib_vl_arb_table_t) );

  /* we will use the TCL split list to split into elements */
  if (Tcl_SplitList(interp,
                    Tcl_GetStringFromObj($source,0),
                    &num_sub_lists, &sub_lists) != TCL_OK) {
    printf("-E- Bad formatted list :%s\n",
           Tcl_GetStringFromObj($source,0));
    return TCL_ERROR;
  }

  for (idx = 0;
       (idx < num_sub_lists) && (idx < IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK);
       idx++)
  {
    /* we need to double copy since TCL 8.4 requires split res to be const */
    char buf[16];
    char *p_last;
    strcpy(buf, sub_lists[idx]);
    p_vl_str = strtok_r(buf," \t", &p_last);
    p_wt_str = strtok_r(NULL," \t", &p_last);
    if (! (p_vl_str && p_wt_str)) {
      printf("-E- Bad formatted number pair:%s\n", sub_lists[idx]);
      return TCL_ERROR;
    } else {
      errno = 0;
      vl = strtoul(p_vl_str, NULL, 0);
      if (errno || (vl > 15)) {
        printf("-E- Bad formatted VL:%s\n", p_vl_str);
        return TCL_ERROR;
      }

      weight = strtoul(p_wt_str, NULL, 0);
      if (errno || (weight > 255)) {
        printf("-E- Bad formatted Weight:%s\n", p_wt_str);
        return TCL_ERROR;
      }

      n.vl_entry[idx].vl = vl;
      n.vl_entry[idx].weight = weight;
    }
  }
  Tcl_Free((char *) sub_lists);

  $target = &n;
}

%typemap(tcl8,out) sac_vl_arb_tbl_t* {
  uint32_t i;
  ib_vl_arb_table_t *p_tbl;
  char buf[256];
  if ($source) {
    strcpy(buf, "");
    p_tbl = $source;
    /*  go over all elements */
    for (i = 0; i < IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK; i++) {
      sprintf(buf, "{0x%X 0x%02X} ",
              p_tbl->vl_entry[i].vl, p_tbl->vl_entry[i].weight);
      Tcl_AppendResult(interp, buf, NULL);
    }
  } else {
	 Tcl_SetResult(interp, "", NULL);
  }
}


%{
#define sac_vl_arb_tbl_t ib_vl_arb_table_t
%}
typedef struct _ib_vl_arb_table_record
{
	ib_net16_t			lid; // for CA: lid of port, for switch lid of port 0
	uint8_t				port_num;
	uint8_t				block_num;
	uint32_t			   reserved;
	sac_vl_arb_tbl_t  vl_arb_tbl;
} sacVlArbRec;

%addmethods sacVlArbRec {
  %new char *get(uint64_t comp_mask) {
    return(ibsacVlArbRecordQuery(self, cl_hton64(comp_mask),
                                 IB_MAD_METHOD_GET));
  };
  %new char *getTable(uint64_t comp_mask) {
    return(ibsacVlArbRecordQuery(self, cl_hton64(comp_mask),
                                 IB_MAD_METHOD_GETTABLE));
  };

  void delete() {
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
   PKey Table Record
	-----------------------------------------------------*/
#define IB_PKEY_COMPMASK_LID        "0x1"
#define IB_PKEY_COMPMASK_BLOCK      "0x2"
#define IB_PKEY_COMPMASK_PORT       "0x4"

typedef struct _ib_pkey_table
{
  ib_net16_array_t pkey_entry[IB_NUM_PKEY_ELEMENTS_IN_BLOCK];
} sacPKeyTbl;

typedef struct _ib_pkey_table_record
{
	ib_net16_t			lid; // for CA: lid of port, for switch lid of port 0
	uint16_t				block_num;
   uint8_t				port_num; // for switch: port number, for CA: reserved
	uint8_t   			reserved1;
	uint16_t 			reserved2;
	sacPKeyTbl     	pkey_tbl;
} sacPKeyRec;

%addmethods sacPKeyRec {
  %new char *get(uint64_t comp_mask) {
    return(ibsacPKeyRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GET));
  };
  %new char *getTable(uint64_t comp_mask) {
    return(ibsacPKeyRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GETTABLE));
  };

  void delete() {
	 SWIG_AltMnglUnregObj(&(self->pkey_tbl));
	 SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

/* -----------------------------------------------------
	GuidInfo INFO
	-----------------------------------------------------*/
#define IB_GIR_COMPMASK_LID             "0x1"
#define IB_GIR_COMPMASK_BLOCKNUM        "0x2"
#define IB_GIR_COMPMASK_GID0            "0x10"
#define IB_GIR_COMPMASK_GID1            "0x20"
#define IB_GIR_COMPMASK_GID2            "0x40"
#define IB_GIR_COMPMASK_GID3            "0x80"
#define IB_GIR_COMPMASK_GID4            "0x100"
#define IB_GIR_COMPMASK_GID5            "0x200"
#define IB_GIR_COMPMASK_GID6            "0x400"
#define IB_GIR_COMPMASK_GID7            "0x800"

typedef struct _ib_guid_info {
        ib_net64_array_t guid[GUID_TABLE_MAX_ENTRIES];
} sacGuidInfo;

%addmethods sacGuidInfo {
  void delete() {
    SWIG_AltMnglUnregObj(self);
    free(self);
  }
}

typedef struct _ib_guidinfo_record {
        ib_net16_t lid;
        uint8_t block_num;
        uint8_t resv;
        uint32_t reserved;
        sacGuidInfo guid_info;
} sacGuidRec;

%addmethods sacGuidRec {
  %new char *get(uint64_t comp_mask) {
    return(ibsacGuidRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GET));
  };
  %new char *set(uint64_t comp_mask) {
    return(ibsacGuidRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_SET));
  };
  %new char *getTable(uint64_t comp_mask) {
    return(ibsacGuidRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_GETTABLE));
  };
  %new char *delete(uint64_t comp_mask) {
    return(ibsacGuidRecordQuery(self, cl_hton64(comp_mask),
                                IB_MAD_METHOD_DELETE));
  };

  void obj_delete() {
    /* we need to de-register both the guid info and guid record */
    SWIG_AltMnglUnregObj(&(self->guid_info));
    SWIG_AltMnglUnregObj(self);
    free(self);
  }
}
