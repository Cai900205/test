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

/****h* IBMS/Sma
 * NAME
 * IB Subnet Manager Agent Simulator object
 *
 * DESCRIPTION
 * The simulator routes mad messages to the target node. This node
 *  MadProcessor SMA class is provided to handle and respond to these mads.
 *
 * AUTHOR
 * Nimrod Gindi, Mellanox
 *
 *********/

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>

#include "msgmgr.h"
#include "simmsg.h"
#include "sim.h"
#include "sma.h"
#include "helper.h"

#define TAVOR_DEVID 23108
#define ARBEL_DEVID 25208
#define ANAFA_DEVID 43132
#define ANAFA2_DEVID 47396

void
ibms_dump_mad(
  IN const  ibms_mad_msg_t  &madMsg,
  IN const  uint8_t         dir)
{
  if (dir == RCV)
  {
    MSGREG(inf100, 'M', "\n    Received Mad Dump\n$", "ibms_dump_mad");
    MSGSND(inf100, ibms_get_mad_header_str(madMsg.header));
  }
  else
  {
    MSGREG(inf101, 'M', "\n    Send Mad Dump\n$", "ibms_dump_mad");
    MSGSND(inf101, ibms_get_mad_header_str(madMsg.header));
  }

  switch (madMsg.header.attr_id) {
  case IB_MAD_ATTR_PORT_INFO:
    MSGREG(inf102, 'M', "\n   Port Info Dump\n$", "ibms_dump_mad");
    MSGSND(inf102, ibms_get_port_info_str((ib_port_info_t*)((ib_smp_t*) &madMsg.header)->data));
    break;
  case IB_MAD_ATTR_NODE_INFO:
    MSGREG(inf103, 'M', "\n   Node Info Dump\n$", "ibms_dump_mad");
    MSGSND(inf103, ibms_get_node_info_str((ib_node_info_t*)((ib_smp_t*) &madMsg.header)->data));
    break;
  default:
    MSGREG(warn1, 'W', "No handler written yet for this attribute:$", "ibms_dump_mad");

    MSGSND(warn1, cl_ntoh16(madMsg.header.attr_id));
    break;
  }
}

// CLASS SMATimer

SMATimer::SMATimer(int t):time(t)
{
  pthread_mutex_init(&timerMutex, NULL);
  tid = pthread_create(&th, NULL, SMATimer::timerRun, this);
  if (tid) {
    MSGREG(err0, 'E', "Couldn't create timer thread $ !", "processMad");
    MSGSND(err0, tid);
    }
  MSGREG(inf1, 'V', "SMA timer on!", "processMad");
  MSGSND(inf1);
}

void SMATimer::terminate()
{
  pthread_cancel(th);
}

void* SMATimer::timerRun(void* p)
{
  SMATimer* p_timer = (SMATimer*) p;
  while(1) {
    MSGREG(inf1, 'V', "Sleeping for $ secs !", "processMad");
    MSGSND(inf1,p_timer->time);

    sleep(p_timer->time);
    pthread_mutex_lock(&p_timer->timerMutex);
    unsigned i=0;
    while (i<p_timer->L.size()) {
      int res = p_timer->L[i].f(p_timer->L[i].data);
      if (!res)
	p_timer->L.erase(p_timer->L.begin()+i);
      else
	i++;
    }
    pthread_mutex_unlock(&p_timer->timerMutex);
  }
  return NULL;
}

void SMATimer::reg (reg_t r)
{
  pthread_mutex_lock(&timerMutex);
  L.push_back(r);
  pthread_mutex_unlock(&timerMutex);
}

void SMATimer::unreg (void* data)
{
  pthread_mutex_lock(&timerMutex);
  for (unsigned i=0;i<L.size();i++)
    if (L[i].data == data) {
      L.erase(L.begin()+i);
      return;
    }
  pthread_mutex_unlock(&timerMutex);
}

// CLASS IBMSSma

SMATimer IBMSSma::mkeyTimer = SMATimer(T_FREQ);

int IBMSSma::cbMkey(void* data)
{
  portTiming* pT = (portTiming*) data;
  pthread_mutex_lock(&pT->mut);
  pT->counter--;
  if (pT->counter > 0) {
    pthread_mutex_unlock(&pT->mut);
    return 1;
  }
  // Need to zero m_key
  pT->pInfo->m_key = 0;
  pT->timerOn = 0;
  pthread_mutex_unlock(&pT->mut);

  return 0;
}

void IBMSSma::initSwitchInfo()
{
  IBNode              *pNodeData;

  pNodeData = pSimNode->getIBNode();

  pSimNode->switchInfo.lin_cap = CL_HTON16(0xbfff);
  pSimNode->switchInfo.rand_cap = 0;
  pSimNode->switchInfo.mcast_cap = CL_HTON16(0x1ff);

  pSimNode->switchInfo.lin_top = 0;
  pSimNode->switchInfo.def_port = 0;
  pSimNode->switchInfo.def_mcast_pri_port = 0;
  pSimNode->switchInfo.def_mcast_not_port = 0;
  pSimNode->switchInfo.life_state = 0;
  pSimNode->switchInfo.lids_per_port = 0;
  pSimNode->switchInfo.enforce_cap = CL_HTON16(32);
  pSimNode->switchInfo.flags = 0xA0; // capable partition enforce in/out

  MSGREG(inf1, 'V', "Initialization of node's SwitchInfo is Done !", "initSwitchInfo");
  MSGSND(inf1);
  pSimNode->sl2VlInPortEntry.resize((pSimNode->nodeInfo.num_ports) + 1);
  initSwitchSl2VlTable();
}

void IBMSSma::initNodeInfo()
{
  IBNode              *pNodeData;

  pNodeData = pSimNode->getIBNode();
  pSimNode->nodeInfo.base_version = 1 ;
  pSimNode->nodeInfo.class_version = 1 ;
  pSimNode->nodeInfo.num_ports = pNodeData->numPorts;

  // HACK: as the devId is not really meaningful due to IBDM limitation
  // we only rely on the type of device. HCAs get 64 PKeys switches: 24.
  if (pNodeData->type == IB_NODE_TYPE_SWITCH)
  {
    //Switch
    pSimNode->nodeInfo.node_type = IB_NODE_TYPE_SWITCH;
    pSimNode->nodeInfo.partition_cap = CL_HTON16(24);
    initSwitchInfo();
  }
  else if (pNodeData->type == IB_NODE_TYPE_CA)
  {
    //HCA
    pSimNode->nodeInfo.node_type = IB_NODE_TYPE_CA;
    pSimNode->sl2VlInPortEntry.resize((pSimNode->nodeInfo.num_ports) + 1);
    pSimNode->nodeInfo.partition_cap = CL_HTON16(0x40);
    initCaSl2VlTable();
  }
  else
  {
    MSGREG(err0, 'E', "Node Type is un-known $ !", "initNodeInfo");
    MSGSND(err0, pNodeData->type);
  }
  pSimNode->nodeInfo.sys_guid = cl_hton64(pNodeData->guid_get());
  pSimNode->nodeInfo.node_guid = pSimNode->nodeInfo.sys_guid;
  pSimNode->nodeInfo.port_guid = pSimNode->nodeInfo.sys_guid;

  pSimNode->nodeInfo.device_id = cl_hton16(pNodeData->devId);
  pSimNode->nodeInfo.revision = cl_hton32(pNodeData->revId);
  {
    uint32_t tmpVendorLocalPort;
    tmpVendorLocalPort = pNodeData->vendId | (1 << 24);
    pSimNode->nodeInfo.port_num_vendor_id = cl_hton32(tmpVendorLocalPort);
  }
  MSGREG(inf1, 'V', "Initialization of node's NodeInfo is Done !", "initNodeInfo");
  MSGSND(inf1);
}

void IBMSSma::initGuidInfo()
{
  /* initialize guids */
  ib_guid_info_t zeroGuidInfo;
  ib_guid_info_t firstBlockGuidInfo;
  memset(&zeroGuidInfo, 0, sizeof(ib_guid_info_t));
  memset(&firstBlockGuidInfo, 0, sizeof(ib_guid_info_t));

  unsigned int numBlocks;
  vector< ib_guid_info_t > emptyGuidInfoVector;
  IBPort *pNodePortData = NULL;

  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH) {
    //guid info only for the port 0
    pSimNode->nodeGuidsInfo.push_back(emptyGuidInfoVector);
    firstBlockGuidInfo.guid[0] = cl_hton64(pSimNode->nodeInfo.port_guid);
    numBlocks = (pSimNode->nodePortsInfo[0].guid_cap + 7) / 8;
    for (unsigned int block = 0; block < numBlocks; ++block) {
      if (block) {
        pSimNode->nodeGuidsInfo[0].push_back(zeroGuidInfo);
      } else {
        pSimNode->nodeGuidsInfo[0].push_back(firstBlockGuidInfo);
      }
    }
  } else {  //hca
    //dummy port 0 for hca
    pSimNode->nodeGuidsInfo.push_back(emptyGuidInfoVector);
    pSimNode->nodeGuidsInfo[0].push_back(zeroGuidInfo);

    //other ports for hca
    for (unsigned int pn = 1; pn <= pSimNode->nodeInfo.num_ports; ++pn) {
      pNodePortData = pSimNode->getIBNode()->getPort(pn);
      if ((pNodePortData == NULL) || (pNodePortData->p_remotePort == NULL)) {
        MSGREG(wrn0, 'W', "Attempt to reach port $ $ failed - no such port OR it's not connected!", "initGuidInfo");
        MSGSND(wrn0, pn, pSimNode->getIBNode()->name);
        MSGREG(wrn1, 'W', " Entering dummy entry !", "initGuidInfo");
        MSGSND(wrn1);
        //TODO handle not connected port generic - remove all assignments from below
        firstBlockGuidInfo.guid[0] = cl_hton64(pSimNode->nodeInfo.port_guid);
      } else {
        firstBlockGuidInfo.guid[0] = cl_hton64(pNodePortData->guid_get());
      }

      //enter blocks for each port
      numBlocks = (pSimNode->nodePortsInfo[pn].guid_cap + 7) / 8;
      for (unsigned int block = 0; block < numBlocks; block++) {
        if (block) {
          pSimNode->nodeGuidsInfo[pn].push_back(zeroGuidInfo);
        } else {
          pSimNode->nodeGuidsInfo[pn].push_back(firstBlockGuidInfo);
        }
      }
    }
  }
}

void IBMSSma::initPKeyTables()
{

   /* initialize pkeys */
   ib_pkey_table_t zeroPKeys;
   ib_pkey_table_t firstBlockPkeys;
   memset(&zeroPKeys, 0, sizeof(ib_pkey_table_t));
   memset(&firstBlockPkeys, 0, sizeof(ib_pkey_table_t));
   firstBlockPkeys.pkey_entry[0] = 0xffff;

   unsigned int numBlocks;
   vector< ib_pkey_table_t > emptyPkeyVector;

   for (unsigned int pn = 0; pn <= pSimNode->nodeInfo.num_ports; pn++)
   {
      pSimNode->nodePortPKeyTable.push_back(emptyPkeyVector);
      if ( (pSimNode->nodeInfo.node_type != IB_NODE_TYPE_SWITCH) || (pn == 0) )
         numBlocks = (cl_ntoh16(pSimNode->nodeInfo.partition_cap) + 31) / 32;
      else
        numBlocks = (cl_ntoh16(pSimNode->switchInfo.enforce_cap) + 31) / 32;

      for (unsigned int block = 0; block < numBlocks; block++)
      {
         if (block)
         {
            pSimNode->nodePortPKeyTable[pn].push_back(zeroPKeys);
         }
         else
         {
            pSimNode->nodePortPKeyTable[pn].push_back(firstBlockPkeys);
         }
      }
   }
}

void IBMSSma::initVlArbitTable()
{
  uint8_t                     i,j,k;
  ib_vl_arb_table_t           vlArbitEntry;

  for (i=0; i <= (pSimNode->nodeInfo.num_ports) ; i++)
  {
    //TODO when supporting enhanced port0 - will need to support 0 here as well
    // Zero is dummy for the moment
    for (j=0; j <= 4 ; j++)
    {
      for (k=0; k < IB_NUM_VL_ARB_ELEMENTS_IN_BLOCK ; k++)
      {
        vlArbitEntry.vl_entry[k].vl = k % IB_MAX_NUM_VLS;
        vlArbitEntry.vl_entry[k].weight = 1;
      }
      (pSimNode->vlArbPortEntry[i]).push_back( vlArbitEntry );
    }
  }

  MSGREG(inf1, 'V', "Initialization of VL arbitration table is Done !", "initVlArbitTable");
  MSGSND(inf1);
}

void IBMSSma::initSwitchSl2VlTable()
{
  uint8_t                     i,j,k;
  ib_slvl_table_t             slToVlEntry;

  for (i=0; i <= (pSimNode->nodeInfo.num_ports) ; i++)
  {
    //TODO when supporting enhanced port0 - will need to support 0 here as well
    // Zero is dummy for the moment
    for (j=0; j <= (pSimNode->nodeInfo.num_ports) ; j++)
    {
      for (k=0; k < (IB_MAX_NUM_VLS/2) ; k++)
      {
        slToVlEntry.raw_vl_by_sl[k] = 0xf;
      }
      (pSimNode->sl2VlInPortEntry[i]).push_back( slToVlEntry );
    }
  }

  MSGREG(inf1, 'V', "Initialization of switch SL to VL table is Done !", "initSwitchSl2VlTable");
  MSGSND(inf1);
}

void IBMSSma::initCaSl2VlTable()
{
  uint8_t                     i,k;
  ib_slvl_table_t             slToVlEntry;

  for (i=0; i <= (pSimNode->nodeInfo.num_ports) ; i++)
  {
    for (k=0; k < (IB_MAX_NUM_VLS/2) ; k++)
    {
      slToVlEntry.raw_vl_by_sl[k] = 0xf;
    }
    (pSimNode->sl2VlInPortEntry[i]).push_back( slToVlEntry );
  }

  MSGREG(inf1, 'V', "Initialization of CA SL to VL table is Done !", "initCaSl2VlTable");
  MSGSND(inf1);
}

void IBMSSma::initPortInfo()
{
  MSG_ENTER_FUNC;

  uint8_t             i=0;
  ib_port_info_t      tmpPortInfo;
  IBPort              *pNodePortData;
  IBNode              *pNodeData;

  pNodeData = pSimNode->getIBNode();

  memset((void*)&tmpPortInfo, 0, sizeof(ib_port_info_t));
  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_CA)
  {
    MSGREG(inf1, 'V', "Creating dummy port number $ for xCA !", "initPortInfo");
    MSGSND(inf1, i);
  }
  else if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH)
  {
    // TODO add Enhanced Port Zero support
    MSGREG(inf2, 'V', "Creating Base port Zero for Switch !", "initPortInfo");
    MSGSND(inf2);

    //Init the port 0 of the switch according to it's capability
    tmpPortInfo.capability_mask = CL_HTON32(0x808);
    ib_port_info_set_lmc(&tmpPortInfo, 0);
    ib_port_info_set_mpb(&tmpPortInfo, 0);
    //we do not have to support this ! (HACK: Done to avoid OSM bugs)
    //  Base Port 0 case we need mtu_cap too:
    tmpPortInfo.mtu_cap = 1;
    ib_port_info_set_neighbor_mtu(&tmpPortInfo, 1);
    ib_port_info_set_vl_cap(&tmpPortInfo, 4);
    ib_port_info_set_port_state( &tmpPortInfo, IB_LINK_ACTIVE);
    ib_port_info_set_port_phys_state( 5, &tmpPortInfo);

    tmpPortInfo.subnet_timeout = 0;
    tmpPortInfo.guid_cap = 1;   //switch is supported only one guid
    tmpPortInfo.resp_time_value = 0x6;

  }
  else
  {
    MSGREG(err0, 'E', "Attempt to initialize unknown device port !", "initPortInfo");
    MSGSND(err0);
    MSG_EXIT_FUNC;
    return;
  }
  pSimNode->nodePortsInfo.push_back( tmpPortInfo );
  i++;

  while (i <= pSimNode->nodeInfo.num_ports)
  {
    MSGREG(inf1, 'V', "Initializing port number $ !", "initPortInfo");
    MSGSND(inf1, i);

    memset((void*)&tmpPortInfo, 0, sizeof(ib_port_info_t));
    pNodePortData = pNodeData->getPort(i);
    if ((pNodePortData == NULL) || (pNodePortData->p_remotePort == NULL))
    {
      MSGREG(wrn0, 'W', "Attempt to reach port $ failed - no such port OR it's not connected!", "initPortInfo");
      MSGSND(wrn0, i);
      MSGREG(wrn1, 'W', " Entering dummy entry !", "initPortInfo");
      MSGSND(wrn1);
      //TODO handle not connected port generic - remove all assignments from below
      ib_port_info_set_port_state( &tmpPortInfo, IB_LINK_DOWN);
      ib_port_info_set_port_phys_state( 2, &tmpPortInfo);
      pSimNode->nodePortsInfo.push_back( tmpPortInfo );
      i++;
      continue;
    }

    tmpPortInfo.base_lid = cl_hton16(pNodePortData->base_lid);
    tmpPortInfo.capability_mask = CL_HTON32(0x808);
    tmpPortInfo.local_port_num = i;
    tmpPortInfo.link_width_enabled = pNodePortData->width;
    tmpPortInfo.link_width_supported = pNodePortData->width;
    tmpPortInfo.link_width_active = pNodePortData->width;
    {
      int linkSpeed = pNodePortData->speed;

      if (pNodePortData->speed == 2)
      {
        linkSpeed = 3;
      }
      else if (pNodePortData->speed == 4)
      {
        linkSpeed = 7;
      }
      // LinkSpeedSupported and PortState
      ib_port_info_set_port_state( &tmpPortInfo, IB_LINK_INIT);
      tmpPortInfo.state_info1 |= (tmpPortInfo.state_info1 & 0x0f) | (linkSpeed << 4);
      // LinkSpeedEnabled and LinkSpeedActive
      tmpPortInfo.link_speed = linkSpeed | (pNodePortData->speed << 4);

    }

    // PortPhysState and LinkDownDefaultState
    ib_port_info_set_port_phys_state( 5, &tmpPortInfo);
    ib_port_info_set_link_down_def_state( &tmpPortInfo, 2);
    tmpPortInfo.mtu_smsl = 1;
    ib_port_info_set_neighbor_mtu( &tmpPortInfo, 4);
    ib_port_info_set_vl_cap(&tmpPortInfo, 4);
    tmpPortInfo.vl_high_limit = 1;
    tmpPortInfo.vl_arb_high_cap = 8;
    tmpPortInfo.vl_arb_low_cap = 8;
    tmpPortInfo.mtu_cap = 4;
    if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_CA)    //In switch there is not alias guids in
                                                            //phisical ports
    {
      tmpPortInfo.guid_cap = 128;
    }
    tmpPortInfo.resp_time_value = 20;

    pSimNode->nodePortsInfo.push_back( tmpPortInfo );
    i++;
  }
  MSG_EXIT_FUNC;
  return;
}


IBMSSma::IBMSSma(IBMSNode *pSNode, list_uint16 mgtClasses) :
  IBMSMadProcessor(pSNode, mgtClasses)
{
  MSG_ENTER_FUNC;

  IBNode*     pNodeData;

  pNodeData = pSNode->getIBNode();

  //Init NodeInfo structure of the node
  initNodeInfo();

  //Init PortInfo structure of the node
  initPortInfo();

  //Init GuidInfo ports vector size and table
  if (pSimNode->nodeInfo.node_type != IB_NODE_TYPE_SWITCH)  //hca
    pSimNode->nodeGuidsInfo.resize(pSimNode->nodeInfo.num_ports + 1);
  initGuidInfo();

  //Init VL Arbitration ports vector size and table
  pSimNode->vlArbPortEntry.resize(pSimNode->nodeInfo.num_ports + 1);
  initVlArbitTable();

  //Init P_Key ports vector size + and table
  pSimNode->nodePortPKeyTable.resize(pSimNode->nodeInfo.num_ports + 1);
  initPKeyTables();

  //Init ports' timing vector
  vPT.resize(pSimNode->nodeInfo.num_ports + 1);
  for (unsigned i=0;i<vPT.size();i++)
    if ((pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH) || i!=0) {
      vPT[i].pInfo = &(pSimNode->nodePortsInfo[i]);
      pthread_mutex_init(&vPT[i].mut, NULL);
      vPT[i].timerOn = 0;
    }

  MSG_EXIT_FUNC;
};

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

int IBMSSma::nodeDescMad(ibms_mad_msg_t &respMadMsg)
{
  MSG_ENTER_FUNC;

  ib_smp_t* pRespMad;

  MSGREG(inf1, 'I', "NodeDescription Get mad handling !", "nodeDescMad");
  MSGSND(inf1);
  pRespMad = (ib_smp_t*) &respMadMsg.header;

  string desc;
  if (pSimNode->nodeInfo.node_type != IB_NODE_TYPE_SWITCH)
  {
    // parse node name of format sysname/U[0-9]
    const char *p_sep = strstr(pSimNode->getIBNode()->name.c_str(), "/U");
    if (!p_sep) {
	desc = (pSimNode->getIBNode())->p_system->name + string(" HCA-1 (Mellanox HCA)");
    } else {
	// HACK: assume nodes are named [^U]+/U[0-9]$...
	desc = (pSimNode->getIBNode())->p_system->name + string(" HCA-") +
	  string(p_sep+2) + string("  (Mellanox HCA)");
    }
  }
  else
  {
    desc = (pSimNode->getIBNode())->p_system->name + string(" Infiniscale-III Mellanox Technologies");
  }

  int descLen = desc.size();
  if (descLen > 64) descLen = 64;
  memcpy(pRespMad->data, desc.c_str(), descLen);
  if (descLen < 64) pRespMad->data[descLen] = 0;
  MSG_EXIT_FUNC;
  return 0;
}

int IBMSSma::nodeInfoMad(ibms_mad_msg_t &respMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;

  ib_smp_t* pRespMad;

  MSGREG(inf1, 'I', "NodeInfo Get mad handling !", "nodeInfoMad");
  MSGSND(inf1);
  pRespMad = (ib_smp_t*) &respMadMsg.header;
  ib_node_info_set_local_port_num( &(pSimNode->nodeInfo), inPort);
  memcpy (pRespMad->data, &(pSimNode->nodeInfo), sizeof(pSimNode->nodeInfo));

  /* provide the port guid for CA ports */
  if (pSimNode->nodeInfo.node_type != IB_NODE_TYPE_SWITCH)
  {
    ib_node_info_t *p_node_info = (ib_node_info_t *)pRespMad->data;
    IBPort *pPort = pSimNode->getIBNode()->getPort(inPort);
    if (pPort)
      p_node_info->port_guid = cl_hton64(pPort->guid_get());
  }

  MSG_EXIT_FUNC;
  return 0;
}

int IBMSSma::lftMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  ib_smp_t*           pRespMad;
  uint8_t               lftBlock[64];
  uint32_t            lftIndex = 0;
  uint32_t            iter = 0;

  pRespMad = (ib_smp_t*) &respMadMsg.header;

  if ((uint16_t)(cl_ntoh16(pSimNode->switchInfo.lin_cap)/64) <
      cl_ntoh32(reqMadMsg.header.attr_mod))
  {
    MSGREG(err0, 'E',
           "Req. lft block is $ while SwitchInfo LFT Cap is $ !",
           "lftMad");
    MSGSND(err0, cl_ntoh32(reqMadMsg.header.attr_mod),
           cl_ntoh16(pSimNode->switchInfo.lin_cap));
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  lftIndex = cl_ntoh32(reqMadMsg.header.attr_mod) * 64;
  iter = cl_ntoh32(reqMadMsg.header.attr_mod);
  MSGREG(inf3, 'V', "Linear Forwarding entry handled is $ and block number is $ ", "lftMad");
  MSGSND(inf3, lftIndex, iter);

  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf1, 'V', "LFT SubnGet !", "lftMad");
    MSGSND(inf1);

    //copy entries from lft according to index
    for (iter=0;iter<64;iter++) lftBlock[iter] =
                                  (pSimNode->getIBNode())->getLFTPortForLid(lftIndex + iter);
  }
  else
  {
    MSGREG(inf2, 'V', "LFT SubnSet !", "lftMad");
    MSGSND(inf2);

    memcpy ( &lftBlock[0], pRespMad->data, 64 * sizeof(uint8_t));
    //copy entries from lft according to index
    for (iter=0;iter<64;iter++)
      (pSimNode->getIBNode())->setLFTPortForLid(lftIndex + iter, lftBlock[iter]);
  }

  memcpy (pRespMad->data, &lftBlock[0], 64 * sizeof(uint8_t));

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::vlArbMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  ib_smp_t*           pRespMad;
  ib_smp_t*           pReqMad;
  uint8_t             portIndex, priorityIndex;
  ib_vl_arb_table_t   vlArbEntryElm;
  ib_vl_arb_table_t*  pVlArbEntryElm;

  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH)
  {
    portIndex = (cl_ntoh32(reqMadMsg.header.attr_mod) & 0xff);
  }
  else
  {
    portIndex = inPort;
  }
  priorityIndex = (cl_ntoh32(reqMadMsg.header.attr_mod) >> 16);
  if (priorityIndex == 0) {
    MSGREG(err0, 'E',
           "Req.  blockIndex is $ legal values are 1..4 !",
           "vlArbMad");
    MSGSND(err0, portIndex, pSimNode->nodeInfo.num_ports);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  if (portIndex > pSimNode->nodeInfo.num_ports)
  {
    MSGREG(err1, 'E',
           "Req. port is $ while Node number of ports is $ !",
           "vlArbMad");
    MSGSND(err1, portIndex, pSimNode->nodeInfo.num_ports);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  pRespMad = (ib_smp_t*) &respMadMsg.header;
  pReqMad = (ib_smp_t*) &reqMadMsg.header;

  MSGREG(inf0, 'V', "VL Arbitration entry handled for port $ and priority $ ", "vlArbMad");
  MSGSND(inf0, portIndex, priorityIndex);

  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf1, 'V', "VL Arbitration SubnGet !", "vlArbMad");
    MSGSND(inf1);

    vlArbEntryElm = (pSimNode->vlArbPortEntry[portIndex])[priorityIndex];
    memcpy ((void*)(pRespMad->data), (void*)(&vlArbEntryElm), sizeof(ib_vl_arb_table_t));
  }
  else
  {
    MSGREG(inf2, 'V', "VL Arbitration SubnSet !", "vlArbMad");
    MSGSND(inf2);

    pVlArbEntryElm = &(pSimNode->vlArbPortEntry[portIndex])[priorityIndex];
    memcpy ((void*)(pVlArbEntryElm), (void*)(pReqMad->data), sizeof(ib_vl_arb_table_t));
    memcpy ((void*)(pRespMad->data), (void*)(pReqMad->data), sizeof(ib_vl_arb_table_t));
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::sl2VlMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  ib_smp_t*           pRespMad;
  ib_smp_t*           pReqMad;
  uint8_t             inputPortIndex, outputPortIndex;
  ib_slvl_table_t     sl2VlEntryElm;
  ib_slvl_table_t*    pSl2VlEntryElm;

  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH)
  {
    inputPortIndex = ((cl_ntoh32(reqMadMsg.header.attr_mod) >> 8) & 0xff);
    outputPortIndex = (cl_ntoh32(reqMadMsg.header.attr_mod) & 0xff);
  }
  else
  {
    inputPortIndex = 0;
    outputPortIndex = inPort;
  }

  if ((inputPortIndex > pSimNode->nodeInfo.num_ports) ||
      (outputPortIndex > pSimNode->nodeInfo.num_ports))
  {
    MSGREG(err0, 'E',
           "Req. mft input port is $ and output port is $ while Node number of ports is $ !",
           "sl2VlMad");
    MSGSND(err0, inputPortIndex, outputPortIndex, pSimNode->nodeInfo.num_ports);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  pRespMad = (ib_smp_t*) &respMadMsg.header;
  pReqMad = (ib_smp_t*) &reqMadMsg.header;

  MSGREG(inf0, 'V', "SL2VL entry handled for input port $ and output port $ ", "sl2VlMad");
  MSGSND(inf0, inputPortIndex, outputPortIndex);

  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf1, 'V', "SL2VL SubnGet !", "sl2VlMad");
    MSGSND(inf1);

    sl2VlEntryElm = (pSimNode->sl2VlInPortEntry[inputPortIndex])[outputPortIndex];
    memcpy ((void*)(pRespMad->data), (void*)(&sl2VlEntryElm), sizeof(ib_slvl_table_t));
  }
  else
  {
    MSGREG(inf2, 'V', "SL2VL SubnSet !", "sl2VlMad");
    MSGSND(inf2);

    pSl2VlEntryElm = &(pSimNode->sl2VlInPortEntry[inputPortIndex])[outputPortIndex];
    memcpy ((void*)(pSl2VlEntryElm), (void*)(pReqMad->data), sizeof(ib_slvl_table_t));
    memcpy ((void*)(pRespMad->data), (void*)(pReqMad->data), sizeof(ib_slvl_table_t));
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::mftMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  ib_smp_t*           pRespMad;
  ib_smp_t*           pReqMad;
  uint16_t            mftTableEntryIndex = (cl_ntoh32(reqMadMsg.header.attr_mod) & IB_MCAST_BLOCK_ID_MASK_HO);
  uint8_t             mftPortEntryIndex = (cl_ntoh32(reqMadMsg.header.attr_mod) >> IB_MCAST_POSITION_SHIFT);
  ib_mft_table_t*     pMftEntryElm;

  pRespMad = (ib_smp_t*) &respMadMsg.header;
  pReqMad = (ib_smp_t*) &reqMadMsg.header;

  // checking for:
  // 1. AM bits 0-8 =< SwitchInfo:MftCap
  // 2. ((((AM bits 28-31) + 1)*16)-1) <= NodeInfo:NumberOfPort
  if ((cl_ntoh16(pSimNode->switchInfo.mcast_cap)/IB_MCAST_BLOCK_SIZE) < mftTableEntryIndex)
  {
    MSGREG(err0, 'E',
           "Req. mft entry block is $ while SwitchInfo MFT Cap is $ !",
           "mftMad");
    MSGSND(err0, mftTableEntryIndex,
           cl_ntoh16(pSimNode->switchInfo.mcast_cap));
    status = IB_MAD_STATUS_INVALID_FIELD;
    MSG_EXIT_FUNC;
    return status;
  }

  if (mftPortEntryIndex * 16 > pSimNode->nodeInfo.num_ports)
  {
    MSGREG(err1, 'E',
           "Req. mft port block is $ while NodeInfo number of ports is $ !",
           "mftMad");
    MSGSND(err1, mftPortEntryIndex, pSimNode->nodeInfo.num_ports);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  MSGREG(inf3, 'E', "Multicast Forwarding entry handled portIdx $ blockIdx $ ", "mftMad");
  MSGSND(inf3, mftTableEntryIndex, mftPortEntryIndex);

  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf1, 'V', "MFT SubnGet !", "mftMad");
    MSGSND(inf1);

	 if ( (mftPortEntryIndex >= pSimNode->switchMftPortsEntry.size()) ||
			(mftTableEntryIndex >= pSimNode->switchMftPortsEntry[mftPortEntryIndex].size()))
	 {
		 MSGREG(warn1, 'W', "MFT SubnGet with uninitialized values at portIdx:$ blockIdx:$  !", "mftMad");
		 MSGSND(warn1,mftPortEntryIndex, mftTableEntryIndex);
		 memset ((void*)(pRespMad->data), 0, sizeof(ib_mft_table_t));
		 MSG_EXIT_FUNC;
		 return status;
	 }

    pMftEntryElm = &(pSimNode->switchMftPortsEntry[mftPortEntryIndex][mftTableEntryIndex]);
    memcpy ((void*)(pRespMad->data), (void*)(pMftEntryElm), sizeof(ib_mft_table_t));
  }
  else
  {
	  unsigned i;
    MSGREG(inf2, 'V', "MFT SubnSet !", "mftMad");
    MSGSND(inf2);

	 MSGREG(inf9, 'E', "MFT SubnSet $ base_port:$ block:$ entry 0 value:$", "mftMad");

	 for (i = pSimNode->switchMftPortsEntry.size(); i <= mftPortEntryIndex; i++)
	 {
		 vector < ib_mft_table_t > tmpVec;
		 pSimNode->switchMftPortsEntry.push_back(tmpVec);
	 }

	 for (i = pSimNode->switchMftPortsEntry[mftPortEntryIndex].size(); i <= mftTableEntryIndex; i++)
	 {
		 ib_mft_table_t tmp;
		 memset(&tmp, 0, sizeof(ib_mft_table_t));
		 pSimNode->switchMftPortsEntry[mftPortEntryIndex].push_back(tmp);
	 }

    pMftEntryElm = &(pSimNode->switchMftPortsEntry[mftPortEntryIndex][mftTableEntryIndex]);
    memcpy ((void*)(pMftEntryElm), (void*)(pReqMad->data), sizeof(ib_mft_table_t));
	 char buff[16];
	sprintf(buff,"0x%04x", cl_ntoh16(pMftEntryElm->mft_entry[0]));
	MSGSND(inf9, pSimNode->getIBNode()->name,
			mftPortEntryIndex, mftTableEntryIndex, buff);
    memcpy ((void*)(pRespMad->data), (void*)(pReqMad->data), sizeof(ib_mft_table_t));
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::switchInfoMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  ib_smp_t*           pRespMad;

  pRespMad = (ib_smp_t*) &respMadMsg.header;

  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf2, 'V', "SwitchInfo SubnGet !", "switchInfoMad");
    MSGSND(inf2);
  }
  else
  {
    MSGREG(inf3, 'V', "SwitchInfo SubnSet !", "switchInfoMad");
    MSGSND(inf3);
    {
      ib_smp_t*           pReqMad;
      ib_switch_info_t*   pReqSwitchInfo;

      pReqMad = (ib_smp_t*) &reqMadMsg.header;
      pReqSwitchInfo = (ib_switch_info_t*)(pReqMad->data);

      if (cl_ntoh16(pReqSwitchInfo->lin_top) >= cl_ntoh16(pSimNode->switchInfo.lin_cap))
      {
        MSGREG(err0, 'E',
               "SwitchInfo LFT Cap is $ and lower from req. LFT Top $ !",
               "switchInfoMad");
        MSGSND(err0, cl_ntoh16(pSimNode->switchInfo.lin_cap),
               cl_ntoh16(pReqSwitchInfo->lin_top));
        status = IB_MAD_STATUS_INVALID_FIELD;

        MSG_EXIT_FUNC;
        return status;
      }

      pSimNode->switchInfo.lin_top = pReqSwitchInfo->lin_top;
      pSimNode->switchInfo.def_port = pReqSwitchInfo->def_port;
      pSimNode->switchInfo.def_mcast_pri_port = pReqSwitchInfo->def_mcast_pri_port;
      pSimNode->switchInfo.def_mcast_not_port = pReqSwitchInfo->def_mcast_not_port;

      if (ib_switch_info_get_state_change(pReqSwitchInfo))
      {
        ib_switch_info_clear_state_change(pReqSwitchInfo);
      }
      else
      {
        if (ib_switch_info_get_state_change(&(pSimNode->switchInfo)))
          ib_switch_info_set_state_change(pReqSwitchInfo);
      }
      pSimNode->switchInfo.life_state = pReqSwitchInfo->life_state;
    }
  }

  memcpy (pRespMad->data, &(pSimNode->switchInfo), sizeof(ib_switch_info_t));

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::setPortInfoGeneral(ibms_mad_msg_t &respMadMsg,
                                ibms_mad_msg_t &reqMadMsg,
                                uint8_t        inPort,
                                ib_port_info_t portInfoElm,
                                int            portNum)
{
  MSG_ENTER_FUNC;
  ib_net16_t          status = 0;
  ib_smp_t*           pReqMad;
  ib_port_info_t*     pReqPortInfo;
  ib_port_info_t*     pNodePortInfo;

  pReqMad = (ib_smp_t*) &reqMadMsg.header;
  pReqPortInfo = (ib_port_info_t*)(pReqMad->data);
  pNodePortInfo = &(pSimNode->nodePortsInfo[portNum]);

  if (ib_port_info_get_port_state(&portInfoElm) == IB_LINK_DOWN)
  {
    MSGREG(err0, 'W', "PortInfo PortState is Down - Port Not connected !", "setPortInfoGeneral");
    MSGSND(err0);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  pNodePortInfo->local_port_num = inPort;
  pNodePortInfo->m_key = pReqPortInfo->m_key;
  pNodePortInfo->subnet_prefix = pReqPortInfo->subnet_prefix;

  pNodePortInfo->m_key_lease_period = pReqPortInfo->m_key_lease_period;
  // TODO: check LinkWidthEnabled parameter from the SM (check very complexed)
  if (pReqPortInfo->link_width_enabled)
  {
    pNodePortInfo->link_width_enabled = pReqPortInfo->link_width_enabled;
  }

  {
    // state_info1 == LinkSpeedSupported and PortState
    uint8_t newPortState;
    uint8_t oldPortState;

    newPortState = ib_port_info_get_port_state(pReqPortInfo);
    oldPortState = ib_port_info_get_port_state(pNodePortInfo);
    if (((newPortState == IB_LINK_INIT) || (newPortState >= IB_LINK_ACT_DEFER)) ||
        ((oldPortState == IB_LINK_DOWN) &&
         ((newPortState == IB_LINK_ARMED) || (newPortState == IB_LINK_ACTIVE))) ||
        ((oldPortState == IB_LINK_ARMED) && (newPortState == IB_LINK_ARMED)) ||
        ((newPortState == IB_LINK_ACTIVE) &&
         ((oldPortState == IB_LINK_ACTIVE) && (newPortState == IB_LINK_INIT))))
    {
      MSGREG(err0, 'E', "PortInfo PortState set for $ from $ is invalid !", "setPortInfoGeneral");
      MSGSND(err0, newPortState, oldPortState);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }

    if (newPortState == IB_LINK_DOWN) newPortState = IB_LINK_INIT;
    // if all checks passed then set and change required
    if (newPortState)
    {
      ib_port_info_set_port_state(pNodePortInfo, newPortState);
    }
  }

  {   //state_info2 == PortPhysState and LinkDownDefaultState
    uint8_t newPortPhyState;
    uint8_t newDefDownPortState;

    newPortPhyState = ib_port_info_get_port_phys_state(pReqPortInfo);
    if (newPortPhyState >= 4)
    {
      MSGREG(err1, 'E', "PortInfo PortPhyState set for $ (higher or equal to 4) !", "setPortInfoGeneral");
      MSGSND(err1, newPortPhyState);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }

    newDefDownPortState = ib_port_info_get_link_down_def_state(pReqPortInfo);
    if (newDefDownPortState >= 3)
    {
      MSGREG(err2, 'E', "PortInfo LinkDownDefualt State set for $ (higher or equal to 3) !", "setPortInfoGeneral");
      MSGSND(err2, newDefDownPortState);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }

    if (newPortPhyState)
    {
      ib_port_info_set_port_phys_state(newPortPhyState, pNodePortInfo);
    }
    if (newDefDownPortState)
    {
      ib_port_info_set_link_down_def_state(pNodePortInfo, newDefDownPortState);
    }
  }   // END state_info2

  {   // LinkSpeedEnabled and LinkSpeedActive
    uint8_t     linkSpeedEn;

    linkSpeedEn = ib_port_info_get_link_speed_enabled(pReqPortInfo);
    if ((linkSpeedEn >= 8) && (linkSpeedEn <= 0xe))
    {
      MSGREG(err3, 'E', "PortInfo Link speed enabled set for $ (between 0x8 and 0xe) !", "setPortInfoGeneral");
      MSGSND(err3, linkSpeedEn);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }
    if (linkSpeedEn)
    {
      ib_port_info_set_link_speed_enabled(pNodePortInfo, linkSpeedEn);
    }
  }   // END LinkSpeedEnabled and LinkSpeedActive

  pNodePortInfo->vl_high_limit = pReqPortInfo->vl_high_limit;
  pNodePortInfo->m_key_violations = pReqPortInfo->m_key_violations;
  pNodePortInfo->p_key_violations = pReqPortInfo->p_key_violations;
  pNodePortInfo->q_key_violations = pReqPortInfo->q_key_violations;
  pNodePortInfo->subnet_timeout = pReqPortInfo->subnet_timeout;
  pNodePortInfo->error_threshold = pReqPortInfo->error_threshold;

  MSG_EXIT_FUNC;
  return status;
}

/* set the IBPort base lid and keep the map of port by lid consistent */
int IBMSSma::setIBPortBaseLid(
  IBMSNode *pSimNode,
  uint8_t   inPortNum,
  uint16_t base_lid)
{
  IBNode*             pNode;
  IBPort*             pPort = NULL;
  unsigned int        portLidIndex;
  unsigned int        minPortNum;
  unsigned int        maxPortNum;
  MSG_ENTER_FUNC;

  MSGREG(inf0, 'I', "Setting base_lid for node:$ port:$ to $",
         "setIBPortBaseLid");
  pNode = pSimNode->getIBNode();

  /*
     We need a special case for switch port 0 changing
     the reason is that there is no IBPort since they are all physical
     what we do is to search for port 1 - N and set them properly.
     Then we find and change the port by lid table.
  */
  if (pNode->type == IB_SW_NODE)
  {
    if (inPortNum == 0)
    {
      minPortNum = 1;
      maxPortNum = pNode->numPorts;
    }
    else
    {
      MSGREG(w0, 'W', "Ignoring switch port $ != 0", "setIBPortBaseLid");
      MSGSND(w0, inPortNum);
      return 0;
    }
  }
  else
  {
    minPortNum = maxPortNum = inPortNum;
  }

  for (unsigned int portNum = minPortNum; portNum <= maxPortNum; portNum++)
  {
    pPort = pNode->getPort(portNum);
    if (! pPort)
    {
      MSGREG(err9, 'E', "No Port $ on node:$!", "setIBPortBaseLid");
      MSGSND(err9, portNum, pNode->name);
      return 1;
    }

    MSGSND(inf0, pNode->name, portNum, base_lid);

    /* keep track of the previous lid */
    uint16_t prevLid = pPort->base_lid;

    /* assign the lid */
    pPort->base_lid = base_lid;

    /* make sure the vector of port by lid has enough entries */
    if (pNode->p_fabric->PortByLid.size() <= base_lid)
    {
      /* we add 20 entries each time */
      pNode->p_fabric->PortByLid.resize(base_lid+20);
      for ( portLidIndex = pNode->p_fabric->PortByLid.size();
            portLidIndex < (unsigned int)(base_lid + 20);
            portLidIndex++)
      {
        pNode->p_fabric->PortByLid[portLidIndex] = NULL;
      }
    }

    /* keep track of the max lid */
    if (  pNode->p_fabric->maxLid  < base_lid )
      pNode->p_fabric->maxLid = base_lid;

    /* need to cleanup the previous entry */
    if (prevLid < pNode->p_fabric->PortByLid.size())
      pNode->p_fabric->PortByLid[prevLid] = NULL;

    /* cleanup old base_lid for ports that used to have that lid ... */
    IBPort *pPrevPort = pNode->p_fabric->PortByLid[base_lid];
    if (pPrevPort && (pPrevPort != pPort))
    {
      /* for HCAs we can not have two ports pointing at the same lid */
      /* for switches - it must be the same switch ... */
      if ((pNode->type != IB_SW_NODE) || (pPrevPort->p_node != pNode))
        pPrevPort->base_lid = 0;
    }

  }

  /* now set the new port by lid */
  pNode->p_fabric->PortByLid[base_lid] = pPort;

  MSG_EXIT_FUNC;
  return 0;
}

int IBMSSma::setPortInfoSwBasePort(ibms_mad_msg_t &respMadMsg,
                                   ibms_mad_msg_t &reqMadMsg,
                                   uint8_t        inPort,
                                   ib_port_info_t portInfoElm,
                                   int            portNum)
{
  MSG_ENTER_FUNC;
  ib_net16_t          status = 0;
  ib_smp_t*           pReqMad;
  ib_port_info_t*     pReqPortInfo;
  ib_port_info_t*     pNodePortInfo;


  pReqMad = (ib_smp_t*) &reqMadMsg.header;
  pReqPortInfo = (ib_port_info_t*)(pReqMad->data);
  pNodePortInfo = &(pSimNode->nodePortsInfo[portNum]);

  if ((pReqPortInfo->base_lid == 0) || (cl_ntoh16(pReqPortInfo->base_lid) >= 0xbfff))
  {
    MSGREG(err6, 'E', "PortInfo Invalid Lid set to $ on SW Base port!", "setPortInfoSwBasePort");
    MSGSND(err6, pReqPortInfo->base_lid);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  else
  {
    /* need to update the IBPort base lid on a change */
    if (pNodePortInfo->base_lid != pReqPortInfo->base_lid)
    {
      setIBPortBaseLid(pSimNode, portNum, cl_ntoh16(pReqPortInfo->base_lid));
      pNodePortInfo->base_lid = pReqPortInfo->base_lid;
    }
    else
    {
        MSGREG(inf1, 'V', "Lid does not require change by the SubnSet !", "setPortInfoSwBasePort");
        MSGSND(inf1);
    }

  }

  if ((pReqPortInfo->master_sm_base_lid == 0) || (cl_ntoh16(pReqPortInfo->master_sm_base_lid) >= 0xbfff))
  {
    MSGREG(err5, 'E', "PortInfo Invalid master SM Lid set for $ !", "setPortInfoSwBasePort");
    MSGSND(err5, pReqPortInfo->master_sm_base_lid);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  else pNodePortInfo->master_sm_base_lid = pReqPortInfo->master_sm_base_lid;

  {
    uint8_t  reqLmc;
    uint8_t  mKeyProtectBits;

    reqLmc = ib_port_info_get_lmc(pReqPortInfo);
    if (reqLmc != 0)
    {
      MSGREG(err3, 'E', "Base Port0 PortInfo LMC set for $ (must be 0) !", "setPortInfoSwBasePort");
      MSGSND(err3, reqLmc);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }
    ib_port_info_set_lmc(pNodePortInfo, 0);

    mKeyProtectBits = ib_port_info_get_mpb(pReqPortInfo);
    ib_port_info_set_mpb(pNodePortInfo, mKeyProtectBits);
  }

  {
    uint8_t     masterSmSl;

    masterSmSl = ib_port_info_get_smsl(pReqPortInfo);
    ib_port_info_set_smsl(pNodePortInfo, masterSmSl);
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::setPortInfoSwExtPort(ibms_mad_msg_t &respMadMsg,
                                  ibms_mad_msg_t &reqMadMsg,
                                  uint8_t        inPort,
                                  ib_port_info_t portInfoElm,
                                  int            portNum)
{
  MSG_ENTER_FUNC;
  ib_net16_t          status = 0;
  ib_smp_t*           pReqMad;
  ib_port_info_t*     pReqPortInfo;
  ib_port_info_t*     pNodePortInfo;

  pReqMad = (ib_smp_t*) &reqMadMsg.header;
  pReqPortInfo = (ib_port_info_t*)(pReqMad->data);
  pNodePortInfo = &(pSimNode->nodePortsInfo[portNum]);

  {
    uint8_t     nMtuReq;
    uint8_t     mtuCap;

    nMtuReq = ib_port_info_get_neighbor_mtu(pReqPortInfo);
    mtuCap = ib_port_info_get_mtu_cap(pNodePortInfo);
    if ((nMtuReq == 0) || (nMtuReq > 5) || (nMtuReq > mtuCap))
    {
      MSGREG(err4, 'E', "PortInfo N - MTU set for $ with MTU Cap $ !", "setPortInfoSwExtPort");
      MSGSND(err4, nMtuReq, mtuCap);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }
    ib_port_info_set_neighbor_mtu(pNodePortInfo, nMtuReq);
  }

  pNodePortInfo->vl_stall_life = pReqPortInfo->vl_stall_life;

  {
    uint8_t     reqOpVl;
    uint8_t     vlCap;

    reqOpVl = ib_port_info_get_op_vls(pReqPortInfo);
    vlCap = ib_port_info_get_vl_cap(pNodePortInfo);

    if ((reqOpVl > vlCap) || (reqOpVl > 5))
    {
      MSGREG(err5, 'E', "PortInfo Operational VL set for $ with VL Cap $ !", "setPortInfoSwExtPort");
      MSGSND(err5, reqOpVl, vlCap);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }
    if (reqOpVl) ib_port_info_set_op_vls(pNodePortInfo, reqOpVl);
  }

  // TODO check if legal
  pNodePortInfo->vl_enforce =
	 (pNodePortInfo->vl_enforce & 0xf0) | (pReqPortInfo->vl_enforce & 0xf);

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::setPortInfoHca(ibms_mad_msg_t &respMadMsg,
                            ibms_mad_msg_t &reqMadMsg,
                            uint8_t        inPort,
                            ib_port_info_t portInfoElm,
                            int            portNum)
{
  MSG_ENTER_FUNC;
  ib_net16_t          status = 0;
  ib_smp_t*           pReqMad;
  ib_port_info_t*     pReqPortInfo;
  ib_port_info_t*     pNodePortInfo;

  pReqMad = (ib_smp_t*) &reqMadMsg.header;
  pReqPortInfo = (ib_port_info_t*)(pReqMad->data);
  pNodePortInfo = &(pSimNode->nodePortsInfo[portNum]);

  if ((pReqPortInfo->base_lid == 0) || (cl_ntoh16(pReqPortInfo->base_lid) >= 0xbfff))
  {
    MSGREG(err6, 'E', "PortInfo Invalid Lid set to $ on HCA!", "setPortInfoHca");
    MSGSND(err6, pReqPortInfo->base_lid);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  else
  {
    if (pNodePortInfo->base_lid != pReqPortInfo->base_lid)
    {
      setIBPortBaseLid(pSimNode, portNum, cl_ntoh16(pReqPortInfo->base_lid));
      pNodePortInfo->base_lid = pReqPortInfo->base_lid;
    }
    else
    {
      MSGREG(inf1, 'V', "Lid does not require change by the SubnSet !", "setPortInfoHca");
        MSGSND(inf1);
    }
  }

  if ((pReqPortInfo->master_sm_base_lid == 0) || (cl_ntoh16(pReqPortInfo->master_sm_base_lid) >= 0xbfff))
  {
    MSGREG(err5, 'E', "PortInfo Invalid master SM Lid set for $ !", "setPortInfoHca");
    MSGSND(err5, pReqPortInfo->master_sm_base_lid);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  else pNodePortInfo->master_sm_base_lid = pReqPortInfo->master_sm_base_lid;

  {
    uint8_t  reqLmc;
    uint8_t  mKeyProtectBits;

    reqLmc = ib_port_info_get_lmc(pReqPortInfo);
    ib_port_info_set_lmc(pNodePortInfo, reqLmc);
    mKeyProtectBits = ib_port_info_get_mpb(pReqPortInfo);
    ib_port_info_set_mpb(pNodePortInfo, mKeyProtectBits);
  }

  {
    uint8_t     nMtuReq;
    uint8_t     mtuCap;
    uint8_t     masterSmSl;

    nMtuReq = ib_port_info_get_neighbor_mtu(pReqPortInfo);
    mtuCap = ib_port_info_get_mtu_cap(pNodePortInfo);
    if ((nMtuReq == 0) || (nMtuReq > 5) || (nMtuReq > mtuCap))
    {
      MSGREG(err4, 'E', "PortInfo N - MTU set for $ with MTU Cap $ !", "setPortInfoHca");
      MSGSND(err4, nMtuReq, mtuCap);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }
    ib_port_info_set_neighbor_mtu(pNodePortInfo, nMtuReq);
    masterSmSl = ib_port_info_get_smsl(pReqPortInfo);
    ib_port_info_set_smsl(pNodePortInfo, masterSmSl);
  }

  {
    uint8_t     reqOpVl;
    uint8_t     vlCap;

    reqOpVl = ib_port_info_get_op_vls(pReqPortInfo);
    vlCap = ib_port_info_get_vl_cap(pNodePortInfo);

    if ((reqOpVl > vlCap) || (reqOpVl > 5))
    {
      MSGREG(err5, 'E', "PortInfo Operational VL set for $ with VL Cap $ !", "setPortInfoHca");
      MSGSND(err5, reqOpVl, vlCap);
      status = IB_MAD_STATUS_INVALID_FIELD;

      MSG_EXIT_FUNC;
      return status;
    }
    if (reqOpVl) ib_port_info_set_op_vls(pNodePortInfo, reqOpVl);
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::setPortInfo(ibms_mad_msg_t &respMadMsg,
                         ibms_mad_msg_t &reqMadMsg,
                         uint8_t        inPort,
                         ib_port_info_t portInfoElm,
                         int            portNum)
{
  MSG_ENTER_FUNC;
  ib_net16_t          status = 0;

  status = setPortInfoGeneral(respMadMsg, reqMadMsg, inPort, portInfoElm, portNum);
  if (status)
  {
    MSGREG(err0, 'E', "PortInfo failed in setPortInfoGeneral !", "setPortInfo");
    MSGSND(err0);
    MSG_EXIT_FUNC;
    return status;
  }

  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH)
  {
    //Handling a switch port
    //TODO add different handling for: Enhanced Port0
    //MSGREG(inf4, 'V', "PortInfo SubnSet of Switch Enhanced Port0 !", "setPortInfo");
    //MSGSND(inf4);

    //  2) Base Port0
    if (portNum == 0)
    {
      status = setPortInfoSwBasePort(respMadMsg, reqMadMsg, inPort, portInfoElm, portNum);
    }
    else
    { //  3) External ports
      status = setPortInfoSwExtPort(respMadMsg, reqMadMsg, inPort, portInfoElm, portNum);
    }
  }
  else
  {   //Handling an HCA port
    status = setPortInfoHca(respMadMsg, reqMadMsg, inPort, portInfoElm, portNum);
  }
  if (status)
  {
    MSGREG(err1, 'E', "PortInfo failed in Node specific area !", "setPortInfo");
    MSGSND(err1);
    MSG_EXIT_FUNC;
    return status;
  }
  {
    ib_smp_t*           pRespMad;
    ib_port_info_t*     pNodePortInfo;

    pRespMad = (ib_smp_t*) &respMadMsg.header;
    pNodePortInfo = &(pSimNode->nodePortsInfo[portNum]);
    memcpy (pRespMad->data, pNodePortInfo, sizeof(portInfoElm));
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::portInfoMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;

  int                 portNum;
  ib_net16_t          status = 0;
  ib_port_info_t      portInfoElm;

  portNum = cl_ntoh32(reqMadMsg.header.attr_mod);
  //non existing port of the device
  if (portNum > pSimNode->nodeInfo.num_ports)
  {
    MSGREG(err0, 'E', "PortInfo request for non-existing port", "portInfoMad");
    MSGSND(err0);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  //for HCAs if AM is 0 then handling the port that received the mad
  if ((pSimNode->nodeInfo.node_type == IB_NODE_TYPE_CA) && (portNum == 0))
  {
    portNum = inPort;
  }

  portInfoElm = pSimNode->nodePortsInfo[portNum];
  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf2, 'V', "PortInfo SubnGet !", "portInfoMad");
    MSGSND(inf2);
    {
      ib_smp_t*           pRespMad;
      pRespMad = (ib_smp_t*) &respMadMsg.header;

      portInfoElm.local_port_num = inPort;
      memcpy (pRespMad->data, &(portInfoElm), sizeof(portInfoElm));
    }
  }
  else
  {
    status = setPortInfo(respMadMsg, reqMadMsg, inPort, portInfoElm, portNum);
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::guidInfoMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;
  //return 0;

  ib_smp_t*           pRespMad;
  ib_smp_t*           pReqMad;
  ib_net16_t          status = 0;
  ib_guid_info_t      GuidInfoBlockElem;
  ib_guid_info_t*     pGuidInfoBlockElem;
  int                 portNum=0;
  int                 GuidInfoBlockNum=0;

  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_CA) {
    //HCA
    portNum = inPort;
    if (portNum == 0)
      portNum = 1;
  } else {
    //Switch
    portNum = 0;
  }

  GuidInfoBlockNum = cl_ntoh32(reqMadMsg.header.attr_mod) & 0xffff;
  MSGREG(inf2, 'V', "Guid Info block number $ on port $ node $ !", "guidInfoMad");
  MSGSND(inf2, GuidInfoBlockNum, portNum, pSimNode->getIBNode()->name);

  if (GuidInfoBlockNum > (((pSimNode->nodePortsInfo[portNum].guid_cap + 7) / 8) - 1)) {
    MSGREG(err0, 'E', "Guid Info block number $ is un-supported limited to $ blocks !", "guidInfoMad");
    MSGSND(err0, GuidInfoBlockNum, (pSimNode->nodePortsInfo[portNum].guid_cap + 7) / 8);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  pRespMad = (ib_smp_t*) &respMadMsg.header;
  pReqMad = (ib_smp_t*) &reqMadMsg.header;
  if (reqMadMsg.header.method == IB_MAD_METHOD_GET) {
    MSGREG(inf3, 'V', "GuidInfo SubnGet !", "guidInfoMad");
    MSGSND(inf3);

    GuidInfoBlockElem = (pSimNode->nodeGuidsInfo[portNum])[GuidInfoBlockNum];
    memcpy ((void*)(pRespMad->data), (void*)(&GuidInfoBlockElem), sizeof(ib_guid_info_t));
  } else {
    MSGREG(inf4, 'V', "GuidInfo SubnSet !", "guidInfoMad");
    MSGSND(inf4);

    pGuidInfoBlockElem = &(pSimNode->nodeGuidsInfo[portNum])[GuidInfoBlockNum];
    memcpy ((void*)(pGuidInfoBlockElem), (void*)(pReqMad->data), sizeof(ib_guid_info_t));
    memcpy ((void*)(pRespMad->data), (void*)(pReqMad->data), sizeof(ib_guid_info_t));
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::pKeyMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;

  ib_smp_t*           pRespMad;
  ib_smp_t*           pReqMad;
  ib_net16_t          status = 0;
  ib_pkey_table_t     PKeyBlockElem;
  ib_pkey_table_t*    pPkeyBlockElem;
  int                 portNum=0;
  int                 PKeyBlockNum=0;

  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_CA)
  {
    //HCA
    portNum = inPort;
    if (portNum == 0) portNum = 1;

  }
  else if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH)
  {   //Switch
    portNum = cl_ntoh32(reqMadMsg.header.attr_mod) >> 16;
  }
  PKeyBlockNum = cl_ntoh32(reqMadMsg.header.attr_mod) & 0xffff;
  MSGREG(inf2, 'V', "Partition Key block number $ on port $ !", "pKeyMad");
  MSGSND(inf2, PKeyBlockNum, portNum);

  if (PKeyBlockNum > (cl_ntoh16(pSimNode->nodeInfo.partition_cap) / 32))
  {
    MSGREG(err0, 'E', "Partition Key block number $ is un-supported limited to $ blocks !", "pKeyMad");
    MSGSND(err0, PKeyBlockNum, (cl_ntoh16(pSimNode->nodeInfo.partition_cap) / 32));
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }
  //TODO add P_Key size check for external ports of a switch (in SwitchInfo structure)

  pRespMad = (ib_smp_t*) &respMadMsg.header;
  pReqMad = (ib_smp_t*) &reqMadMsg.header;
  if (reqMadMsg.header.method == IB_MAD_METHOD_GET)
  {
    MSGREG(inf3, 'V', "Partition Key SubnGet !", "pKeyMad");
    MSGSND(inf3);

    PKeyBlockElem = (pSimNode->nodePortPKeyTable[portNum])[PKeyBlockNum];
    memcpy ((void*)(pRespMad->data), (void*)(&PKeyBlockElem), sizeof(ib_pkey_table_t));
  }
  else
  {
    MSGREG(inf4, 'V', "Partition Key SubnSet !", "pKeyMad");
    MSGSND(inf4);

    pPkeyBlockElem = &(pSimNode->nodePortPKeyTable[portNum])[PKeyBlockNum];
    memcpy ((void*)(pPkeyBlockElem), (void*)(pReqMad->data), sizeof(ib_pkey_table_t));
    memcpy ((void*)(pRespMad->data), (void*)(pReqMad->data), sizeof(ib_pkey_table_t));
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::madValidation(ibms_mad_msg_t &madMsg)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;

  // we handle Get or Set or trap repress
  if ((madMsg.header.method != IB_MAD_METHOD_GET) &&
      (madMsg.header.method != IB_MAD_METHOD_SET) &&
      (madMsg.header.method != IB_MAD_METHOD_TRAP_REPRESS))
  {
    MSGREG(wrn0, 'W', "We are not handling getResp Method.", "madValidation");
    MSGSND(wrn0);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  ibms_dump_mad( madMsg, RCV);

  MSG_EXIT_FUNC;
  return status;
}

int IBMSSma::processMad(uint8_t inPort, ibms_mad_msg_t &madMsg)
{

  MSG_ENTER_FUNC;

  ibms_mad_msg_t      respMadMsg;
  ib_net16_t          status = 0;
  uint16_t            attributeId = 0;

  //1. Verify rcv MAD validity.
  status = madValidation(madMsg);
  if (status != 0)
  {
    //TODO need to do it more cleanly
    MSG_EXIT_FUNC;
    return status;
  }

  if (madMsg.header.method == IB_MAD_METHOD_TRAP_REPRESS)
  {
    MSGREG(inf0, 'I', "--- Received Trap Repress ---", "processMad");
    MSGSND(inf0);

    MSG_EXIT_FUNC;
    return status;
  }

  //2. Switch according to the attribute to the mad handle
  //      and call appropriate function
  MSGREG(inf1, 'I', "Process Mad got the following attribute: $ !", "processMad");
  MSGSND(inf1, cl_ntoh16(madMsg.header.attr_id));
  attributeId = madMsg.header.attr_id;

  // copy header from request to the response
  {
    ib_smp_t*   pRespSmp = (ib_smp_t*)(&respMadMsg.header);
    ib_smp_t*   pReqSmp = (ib_smp_t*)(&madMsg.header);
    memcpy(pRespSmp, pReqSmp, sizeof(ib_smp_t));
  }

  // perform m_key check
  unsigned mPort = inPort;
  if (pSimNode->nodeInfo.node_type == IB_NODE_TYPE_SWITCH)
    mPort = 0;

  ib_net64_t m_key1 = ((ib_smp_t*)(&madMsg.header))->m_key;

  pthread_mutex_lock(&vPT[mPort].mut);
  ib_net64_t m_key2 = vPT[mPort].pInfo->m_key;

  MSGREG(inf21, 'I', "Mkeys current: $, received: $!", "processMad");
  MSGSND(inf21, cl_ntoh64(m_key2), cl_ntoh64(m_key1));

  if (m_key2 && (m_key1 != m_key2) && madMsg.header.method == IB_MAD_METHOD_SET) {
    MSGREG(inf22, 'I', "Mkeys mismatch!", "processMad");
    MSGSND(inf22);

    // Timer already running
    if (vPT[mPort].timerOn) {
      MSGREG(inf2, 'I', "Timer already on, counter: $!", "processMad");
      MSGSND(inf2,vPT[mPort].counter);

      MSG_EXIT_FUNC;
      pthread_mutex_unlock(&vPT[mPort].mut);
      return 0;
    }
    // Start the timer
    else {
      vPT[mPort].counter = cl_ntoh16(vPT[mPort].pInfo->m_key_lease_period);
      vPT[mPort].timerOn = 1;
      reg_t tmp;
      tmp.f = cbMkey;
      tmp.data = &vPT[mPort];

      MSGREG(inf2, 'I', "Starting timer with counter $!", "processMad");
      MSGSND(inf2,vPT[mPort].counter);

      pthread_mutex_unlock(&vPT[mPort].mut);
      mkeyTimer.reg(tmp);


      MSG_EXIT_FUNC;

      return 0;
    }
  }

  if ((m_key1 == m_key2) && (vPT[mPort].timerOn)) {
    MSGREG(inf2, 'I', "Stopping timer!", "processMad");
    MSGSND(inf2);

    vPT[mPort].timerOn = 0;
    pthread_mutex_unlock(&vPT[mPort].mut);
    mkeyTimer.unreg(&vPT[mPort]);
  }

  pthread_mutex_unlock(&vPT[mPort].mut);

  switch (attributeId) {
  case IB_MAD_ATTR_NODE_DESC:
    MSGREG(inf5, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf5, CL_NTOH16(IB_MAD_ATTR_NODE_DESC));
    if (madMsg.header.method != IB_MAD_METHOD_GET)
    {
      MSGREG(err6, 'E', "NodeDescription was sent with Method other then Get !", "sma");
      MSGSND(err6);

      MSG_EXIT_FUNC;
      return IB_MAD_STATUS_UNSUP_METHOD_ATTR;
    }

    status = nodeDescMad(respMadMsg);
    break;
  case IB_MAD_ATTR_NODE_INFO:
    MSGREG(inf4, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf4, CL_NTOH16(IB_MAD_ATTR_NODE_INFO));
    if (madMsg.header.method != IB_MAD_METHOD_GET)
    {
      MSGREG(err2, 'E', "NodeInfo was sent with Method other then Get !", "sma");
      MSGSND(err2);

      MSG_EXIT_FUNC;
      return IB_MAD_STATUS_UNSUP_METHOD_ATTR;
    }

    status = nodeInfoMad(respMadMsg, inPort);
    break;
  case IB_MAD_ATTR_PORT_INFO:
    MSGREG(inf6, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf6, CL_NTOH16(IB_MAD_ATTR_PORT_INFO));

    status = portInfoMad(respMadMsg, madMsg, inPort);
    break;
  case IB_MAD_ATTR_P_KEY_TABLE:
    MSGREG(inf7, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf7, CL_NTOH16(IB_MAD_ATTR_P_KEY_TABLE));

    status = pKeyMad(respMadMsg, madMsg, inPort);
    break;
  case IB_MAD_ATTR_SWITCH_INFO:
    MSGREG(inf8, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf8, CL_NTOH16(IB_MAD_ATTR_SWITCH_INFO));

    status = switchInfoMad(respMadMsg, madMsg);
    break;
  case IB_MAD_ATTR_LIN_FWD_TBL:
    MSGREG(inf9, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf9, CL_NTOH16(IB_MAD_ATTR_LIN_FWD_TBL));

    status = lftMad(respMadMsg, madMsg);
    break;
  case IB_MAD_ATTR_MCAST_FWD_TBL:
    MSGREG(inf10, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf10, CL_NTOH16(IB_MAD_ATTR_MCAST_FWD_TBL));

    status = mftMad(respMadMsg, madMsg);
    break;
  case IB_MAD_ATTR_SLVL_TABLE:
    MSGREG(inf11, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf11, CL_NTOH16(IB_MAD_ATTR_SLVL_TABLE));

    status = sl2VlMad(respMadMsg, madMsg, inPort);
    break;
  case IB_MAD_ATTR_VL_ARBITRATION:
    MSGREG(inf12, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf12, CL_NTOH16(IB_MAD_ATTR_VL_ARBITRATION));

    status = vlArbMad(respMadMsg, madMsg, inPort);
    break;
  case IB_MAD_ATTR_GUID_INFO:
    MSGREG(inf13, 'I', "Attribute being handled is $ !", "processMad");
    MSGSND(inf13, CL_NTOH16(IB_MAD_ATTR_VL_ARBITRATION));

    status = guidInfoMad(respMadMsg, madMsg, inPort);
    break;
  default:
    MSGREG(err1, 'E', "No handler for requested attribute:$", "processMad");
    MSGSND(err1, cl_ntoh16(attributeId));
    status = IB_MAD_STATUS_UNSUP_METHOD_ATTR;

    //TODO need to do it more cleanly
    MSG_EXIT_FUNC;
    return status;
    break;
  }

  //ib_mad_init_response( respMadMsg, respMadMsg, status);
  {
    ib_smp_t*   pRespSmp = (ib_smp_t*)(&respMadMsg.header);
    ib_smp_t*   pReqSmp = (ib_smp_t*)(&madMsg.header);

    respMadMsg.addr = madMsg.addr;
    respMadMsg.addr.slid = madMsg.addr.dlid;
    respMadMsg.addr.dlid = madMsg.addr.slid;
    if (respMadMsg.header.method == IB_MAD_METHOD_SET)
    {
      respMadMsg.header.method = IB_MAD_METHOD_GET_RESP;
    }
    else respMadMsg.header.method |= IB_MAD_METHOD_RESP_MASK;
    //only if direct then set D-bit
    if (respMadMsg.header.mgmt_class == 0x81)
    {
      respMadMsg.header.status = status | IB_SMP_DIRECTION;
    }
    pRespSmp->dr_slid = pReqSmp->dr_dlid;
    pRespSmp->dr_dlid = pReqSmp->dr_slid;
  }

  //send response
  ibms_dump_mad( respMadMsg, SND);
  pSimNode->getSim()->getDispatcher()->dispatchMad(pSimNode, inPort,
                                                   respMadMsg);

  MSG_EXIT_FUNC;
  return status;
}
