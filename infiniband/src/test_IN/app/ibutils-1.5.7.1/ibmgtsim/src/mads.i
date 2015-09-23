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
 * MADS INTERFACE: Provide means for dispatching mads into the simulator
 */

%{
  /* the following code will place the mad into the dispatcher */
  int
    send_mad(
      IBMSNode *pFromNode,
      uint8_t   fromPort,
      uint16_t  destLid,
      uint8_t   mgmt_class,
      uint8_t   method,
      uint16_t  attr,
      uint32_t  attr_mod,
      uint8_t  *data,
      size_t    size)
    {
      ibms_mad_msg_t msg;
      IBPort *pPort;
      static uint64_t tid = 19927;

      /* initialize the message address vector */
      msg.addr.sl = 0;
      msg.addr.pkey_index = 0;
      msg.addr.dlid = destLid;
      msg.addr.sqpn = 0;
      msg.addr.dqpn = 0;

      pPort = pFromNode->getIBNode()->getPort(fromPort);
      if (! pPort)
      {
        cout << "-E- Given port:" << fromPort << " is down." << endl;
        return 1;
      }
      msg.addr.slid = pPort->base_lid;

      /* initialize the mad header */
      msg.header.base_ver = 1;
      msg.header.mgmt_class = mgmt_class;
      msg.header.class_ver = 1;
      msg.header.method = method;
      msg.header.status = 0;
      msg.header.class_spec = 0;
      msg.header.trans_id = tid++;
      msg.header.attr_id = cl_hton16(attr);
      msg.header.attr_mod = cl_hton32(attr_mod);

      memcpy(msg.payload, data, size);
      IBMSDispatcher *pDispatcher = Simulator.getDispatcher();
      if (! pDispatcher )
        return TCL_ERROR;

      return pDispatcher->dispatchMad(pFromNode, fromPort, msg);
    }

  int send_sa_mad(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint8_t   mgmt_class,
    uint8_t   method,
    uint16_t  attr,
    uint64_t  comp_mask,
    uint8_t  *sa_data,
    size_t    sa_data_size)
    {
      ib_sa_mad_t mad = {0}; /* includes std header and rmpp header */

      mad.attr_offset = ib_get_attr_offset(sa_data_size);
      mad.comp_mask = cl_hton64(comp_mask);
      memcpy(mad.data, sa_data, sa_data_size);

      return send_mad(
        pFromNode,
        fromPort,
        destLid,
        mgmt_class,
        method,
        attr,
        0,
        &mad.rmpp_version,
        MAD_RMPP_DATA_SIZE + 12);
    }

%}

%{
#define madMcMemberRec ib_member_rec_t
%}

struct madMcMemberRec
{
  madMcMemberRec();
  ~madMcMemberRec();

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
  //  uint8_t				proxy_join;  hard to get as it is defined as bit field
}

%addmethods madMcMemberRec {
  int send_set(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_SET,
                cl_ntoh16(IB_MAD_ATTR_MCMEMBER_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madMcMemberRec)
                )
              );
    }

  int send_get(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_GET,
                cl_ntoh16(IB_MAD_ATTR_MCMEMBER_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madMcMemberRec)
                )
              );
    }

  int send_del(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_DELETE,
                cl_ntoh16(IB_MAD_ATTR_MCMEMBER_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madMcMemberRec)
                )
              );
    }
}


%{
#define madPathRec ib_path_rec_t
%}

struct madPathRec
{
  madPathRec();
  ~madPathRec();
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
  uint8_array_t resv2[6];
}

%addmethods madPathRec {
  int send_get(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_GET,
                cl_ntoh16(IB_MAD_ATTR_PATH_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madPathRec)
                )
              );
    }
}


%{
#define madGuidRec ib_guidinfo_record_t
#define guidRecGuidInfoBlock ib_guid_info_t
%}

struct guidRecGuidInfoBlock {
  guidRecGuidInfoBlock();
  ~guidRecGuidInfoBlock();
  ib_net64_array_t guid[GUID_TABLE_MAX_ENTRIES];
}

struct madGuidRec
{
  madGuidRec();
  ~madGuidRec();

  ib_net16_t lid;
  uint8_t block_num;
  uint8_t resv;
  uint32_t reserved;
  guidRecGuidInfoBlock guid_info;
}

%addmethods madGuidRec {
  int send_set(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_SET,
                cl_ntoh16(IB_MAD_ATTR_GUIDINFO_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madGuidRec)
                )
              );
    }

  int send_get(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_GET,
                cl_ntoh16(IB_MAD_ATTR_GUIDINFO_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madGuidRec)
                )
              );
    }

  int send_del(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_DELETE,
                cl_ntoh16(IB_MAD_ATTR_GUIDINFO_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madGuidRec)
                )
              );
    }
}


%{
#define madServiceRec ib_service_record_t
%}

struct madServiceRec
{
  madServiceRec();
  ~madServiceRec();

  ib_net64_t		service_id;
  ib_gid_t		service_gid;
  ib_net16_t		service_pkey;
  ib_net16_t		resv;
  ib_net32_t		service_lease;
  uint8_array_t	        service_key[16];
  uint8_array_t	        service_name[64];
  uint8_array_t		service_data8[16];
  uint16_array_t	service_data16[8];
  uint32_array_t	service_data32[4];
  uint64_array_t	service_data64[2];
}

%addmethods madServiceRec {
  int send_set(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_SET,
                cl_ntoh16(IB_MAD_ATTR_SERVICE_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madServiceRec)
                )
              );
    }

  int send_get(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_GET,
                cl_ntoh16(IB_MAD_ATTR_SERVICE_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madServiceRec)
                )
              );
    }

  int send_del(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_DELETE,
                cl_ntoh16(IB_MAD_ATTR_SERVICE_RECORD),
                comp_mask,
                (uint8_t*)self,
                sizeof(madServiceRec)
                )
              );
    }
}


%{
#include <complib/cl_packon.h>
typedef struct _ib_mad_notice_attr128
{
  uint8_t	        generic_type;
  uint8_t		prod_type_msb;
  ib_net16_t	        prod_type_lsb;
  ib_net16_t	        trap_num;
  ib_net16_t		issuer_lid;
  ib_net16_t		toggle_count;
  ib_net16_t            sw_lid; // the sw lid of which link state changed - for 128 only
  ib_gid_t		issuer_gid;
}	PACK_SUFFIX ib_mad_notice_attr128_t;
#include <complib/cl_packoff.h>

#define madNotice128 ib_mad_notice_attr128_t
%}

struct madNotice128
{
  madNotice128();
  ~madNotice128();

  uint8_t	        generic_type;
  uint8_t		prod_type_msb;
  ib_net16_t	        prod_type_lsb;
  ib_net16_t	        trap_num;
  ib_net16_t		issuer_lid;
  ib_net16_t		toggle_count;
  ib_net16_t            sw_lid; // the sw lid of which link state changed - for 128 only
  ib_gid_t		issuer_gid;
}

%addmethods madNotice128 {
  int send_trap(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid)
    {
      return( send_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_LID,
                IB_MAD_METHOD_TRAP,
                cl_ntoh16(IB_MAD_ATTR_NOTICE),
                0,
                (uint8_t*)self,
                sizeof(madNotice128)
                )
              );
    }
}

%{
#include <complib/cl_packon.h>
typedef struct _ib_mad_notice_attr129
{
  uint8_t	        generic_type;
  uint8_t		prod_type_msb;
  ib_net16_t	        prod_type_lsb;
  ib_net16_t	        trap_num;
  ib_net16_t		issuer_lid;
  ib_net16_t		toggle_count;
  ib_net16_t            pad;      //129
  ib_net16_t            lid;	  // 129 lid and port number of the violation
  uint8_t               port_num; //129
  ib_gid_t		issuer_gid;
}	PACK_SUFFIX ib_mad_notice_attr129_t;
#include <complib/cl_packoff.h>

#define madNotice129 ib_mad_notice_attr129_t
%}

struct madNotice129
{
  madNotice129();
  ~madNotice129();
  uint8_t	        generic_type;
  uint8_t		prod_type_msb;
  ib_net16_t	        prod_type_lsb;
  ib_net16_t	        trap_num;
  ib_net16_t		issuer_lid;
  ib_net16_t		toggle_count;
  ib_net16_t            pad;      //129
  ib_net16_t            lid;	  // 129 lid and port number of the violation
  uint8_t               port_num; //129
  ib_gid_t		issuer_gid;
}

%addmethods madNotice129 {
  int send_trap(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid)
    {
      return( send_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_LID,
                IB_MAD_METHOD_TRAP,
                cl_ntoh16(IB_MAD_ATTR_NOTICE),
                0,
                (uint8_t*)self,
                sizeof(madNotice129)
                )
              );
    }
}

%{
#include <complib/cl_packon.h>
typedef struct _ib_mad_notice_attr144
{
  uint8_t	        generic_type;
  uint8_t		prod_type_msb;
  ib_net16_t	        prod_type_lsb;
  ib_net16_t	        trap_num;
  ib_net16_t		issuer_lid;
  ib_net16_t		toggle_count;
  ib_net16_t            pad1;         // 144
  ib_net16_t            lid;	      // 144 lid where capability mask changed
  ib_net16_t            pad2;         // 144
  ib_net32_t            new_cap_mask; // 144 new capability mask
  ib_gid_t		issuer_gid;
}	PACK_SUFFIX ib_mad_notice_attr144_t;
#include <complib/cl_packoff.h>

#define madNotice144 ib_mad_notice_attr144_t
%}

struct madNotice144
{
  madNotice144();
  ~madNotice144();

  uint8_t	        generic_type;
  uint8_t		prod_type_msb;
  ib_net16_t	        prod_type_lsb;
  ib_net16_t	        trap_num;
  ib_net16_t		issuer_lid;
  ib_net16_t		toggle_count;
  ib_net16_t            pad1;         // 144
  ib_net16_t            lid;	      // 144 lid where capability mask changed
  ib_net16_t            pad2;         // 144
  ib_net32_t            new_cap_mask; // 144 new capability mask
  ib_gid_t		issuer_gid;
}

%addmethods madNotice144 {
  int send_trap(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid)
    {
      return( send_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_LID,
                IB_MAD_METHOD_TRAP,
                cl_ntoh16(IB_MAD_ATTR_NOTICE),
                0,
                (uint8_t*)self,
                sizeof(madNotice144)
                )
              );
    }
}

%{
#include <complib/cl_packon.h>
typedef struct _ib_generic_inform_info
{
	ib_gid_t			gid;
	ib_net16_t		lid_range_begin;
	ib_net16_t		lid_range_end;
	ib_net16_t		reserved1;
	uint8_t			is_generic;
	uint8_t			subscribe;
	ib_net16_t		trap_type;
	ib_net16_t		trap_num;
	ib_net32_t		qpn_resp_time_val;
	uint8_t        reserved2;
	uint8_t			node_type_msb;
	ib_net16_t		node_type_lsb;
}	PACK_SUFFIX ib_generic_inform_info_t;
#include <complib/cl_packoff.h>

#define madGenericInform ib_generic_inform_info_t
%}

struct madGenericInform
{
  madGenericInform();
  ~madGenericInform();

	ib_gid_t			gid;
	ib_net16_t		lid_range_begin;
	ib_net16_t		lid_range_end;
	ib_net16_t		reserved1;
	uint8_t			is_generic;
	uint8_t			subscribe;
	ib_net16_t		trap_type;
	ib_net16_t		trap_num;
	ib_net32_t		qpn_resp_time_val;
	uint8_t        reserved2;
	uint8_t			node_type_msb;
	ib_net16_t		node_type_lsb;
}

#define IB_INFORM_INFO_COMP_GID 0x1
#define IB_INFORM_INFO_COMP_LID_BEGIN 0x2
#define IB_INFORM_INFO_COMP_LID_END 0x4
#define IB_INFORM_INFO_COMP_IS_GENERIC 0x10
#define IB_INFORM_INFO_COMP_TRAP_TYPE 0x40
#define IB_INFORM_INFO_COMP_TRAP_NUM 0x80
#define IB_INFORM_INFO_COMP_QPN 0x100
#define IB_INFORM_INFO_COMP_RESP_TIME 0x200
#define IB_INFORM_INFO_COMP_NODE_TYPE 0x800

%addmethods madGenericInform {
  int send_set(
    IBMSNode *pFromNode,
    uint8_t   fromPort,
    uint16_t  destLid,
    uint64_t  comp_mask)
    {
      return( send_sa_mad(
                pFromNode,
                fromPort,
                destLid,
                IB_MCLASS_SUBN_ADM,
                IB_MAD_METHOD_SET,
                cl_ntoh16(IB_MAD_ATTR_INFORM_INFO),
                comp_mask,
                (uint8_t*)self,
                sizeof(madGenericInform)
                )
              );
    }
}
