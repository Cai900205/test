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

/****h* IBMS/Vendor Specific
 * NAME
 * Vendor Specific mad Agent Simulator object
 *
 * DESCRIPTION
 * The simulator routes mad messages to the target node. This node
 *  MadProcessor Vendor Specific class is provided to handle and respond to these mads.
 *
 * AUTHOR
 * Nimrod Gindi, Mellanox
 *
 *********/

#include "msgmgr.h"
#include "simmsg.h"
#include "sim.h"
#include "helper.h"
#include <iba/ib_types.h>
#include "vsa.h"

using namespace std;

IBMSVendorSpecific::IBMSVendorSpecific(
  IBMSNode *pSNode, list_uint16 mgtClasses) :
  IBMSMadProcessor(pSNode, mgtClasses)
{
  MSG_ENTER_FUNC;

  IBNode*     pNodeData;

  pNodeData = pSNode->getIBNode();
  MSGREG(inf0, 'V', "I'm in CrSpace const ", "IBMSVendorSpecific");
  MSGSND(inf0);

  crSpaceInit((*pNodeData).devId);

  MSG_EXIT_FUNC;
}


int IBMSVendorSpecific::vsMadValidation(
  ibms_mad_msg_t &madMsg)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;

  // we handle Get or Set we ignore GetResp
  if ((madMsg.header.method != 1) &&
      (madMsg.header.method != 2) &&
      (madMsg.header.method != 0x81))
  {
    MSGREG(err0, 'E', "unsupported Method $", "vsMadValidation");
    MSGSND(err0,madMsg.header.method);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  } else if (madMsg.header.method == 0x81) {
    MSGREG(err0, 'W', "ignoring GetResp mad", "vsMadValidation");
    MSGSND(err0);
    status = IB_MAD_STATUS_UNSUP_METHOD;

    MSG_EXIT_FUNC;
    return status;

  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSVendorSpecific::vsAttribute50(
  ibms_mad_msg_t &respMadMsg,
  ibms_mad_msg_t &reqMadMsg, uint8_t inPort)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  uint32_t            attrMod;
  uint32_t            data[56];
  uint32_t            dataLine = 0;
  uint32_t            startAddr = 0;
  uint32_t            length = 0;
  uint32_t            mask = 0;
  uint32_t            accessMode;
  uint32_t            numOfRecords;
  uint8_t             method;

  attrMod = CL_NTOH32(reqMadMsg.header.attr_mod);
  method  = reqMadMsg.header.method;
  accessMode = (attrMod >> 22) & 0x3;
  MSGREG(inf0, 'V', "Access Mode is $ method is $ attrMod is $", "vsAttribute50");
  MSGSND(inf0, accessMode, method,attrMod);

  switch (accessMode) {
  case 0:
    startAddr = ((attrMod & 0xffff) | ((attrMod & 0xff000000) >> 8));
    length = (attrMod >> 16) & 0x3f;
    if (length > 57)
    {
      MSGREG(err1, 'E', "data length bigger then 57 - $","vsAttribute50");
      MSGSND(err1,length);
    }
    if ((startAddr & 0x3) != 0)
    {
      MSGREG(err2, 'E', "start address is not aligned to Dwords - $","vsAttribute50");
      MSGSND(err2,startAddr);
    }
    MSGREG(inf1, 'V', "access parameters: address $ length $ ", "vsAttribute50");
    MSGSND(inf1,startAddr,length);
    // set CrSpace
    if (method == IB_MAD_METHOD_SET )
    {
      for (unsigned int i=0;i<(length);i+=1) {
        data[i] =     (reqMadMsg.payload[8 + i*4]   << 24 |
                       reqMadMsg.payload[8 + i*4+1] << 16 |
                       reqMadMsg.payload[8 + i*4+2] << 8 |
                       reqMadMsg.payload[8 + i*4+3]);
      }
      pSimNode->setCrSpace(startAddr,length,data);
    } else if (method == IB_MAD_METHOD_GET) {
      pSimNode->getCrSpace(startAddr,length,data);
      for (unsigned int j=0;j<length;j++) {
        respMadMsg.payload[8 + j*4] =   (uint8_t)((data[j] >> 24) & 0xff);
        respMadMsg.payload[8 + j*4+1] = (uint8_t)((data[j] >> 16) & 0xff);
        respMadMsg.payload[8 + j*4+2] = (uint8_t)((data[j] >> 8) & 0xff);
        respMadMsg.payload[8 + j*4+3] = (uint8_t)(data[j] & 0xff);
      }
    }
    else
    {
      MSGREG(err3, 'E', "method not supported method- $ accessMode - $","vsAttribute50");
      MSGSND(err3,method,accessMode);
      status = IB_MAD_STATUS_UNSUP_METHOD ; // specified method not supported
    }
    break;
  case 1:
    numOfRecords = (attrMod >> 16) & 0x3f;
    MSGREG(inf2, 'V', "scatter/gather list length $ ", "vsAttribute50");
    MSGSND(inf2,numOfRecords);
    if (numOfRecords > 28 )
    {
      MSGREG(err5, 'E', "number of records bigger than 28- $ accessMode - $","vsAttribute50");
      MSGSND(err5,numOfRecords,accessMode);
    }
    for ( uint32_t i=0;i < numOfRecords;i++)
    {
      startAddr = reqMadMsg.payload[(4 + i*8)]     << 24 |
        reqMadMsg.payload[(4 + i*8 + 1)] << 16 |
        reqMadMsg.payload[(4 + i*8 + 2)] << 8  |
        reqMadMsg.payload[(4 + i*8 + 3)];
      data[0]  =  (reqMadMsg.payload[(4 + i*8 + 4)] << 24 |
                   reqMadMsg.payload[(4 + i*8 + 5)] << 16 |
                   reqMadMsg.payload[(4 + i*8 + 6)] << 8 |
                   reqMadMsg.payload[(4 + i*8 + 7)]) ;
      if ((startAddr >> 16)!= 0)
      {
        MSGREG(err4, 'E', "upper bits of address are not 0- $ accessMode - $","vsAttribute50");
        MSGSND(err4,startAddr,accessMode);
      }
      MSGREG(inf4, 'V', "Entry number $: address = $ data = $ method = $ ", "vsAttribute50");
      MSGSND(inf4,i, startAddr, data[0], method);
      if (method == IB_MAD_METHOD_SET )
      {
        pSimNode->setCrSpace(startAddr,1,data);
      } else if (method == IB_MAD_METHOD_GET) {
        pSimNode->getCrSpace(startAddr,1,data);
        respMadMsg.payload[(4 + i*8 + 4)] = (uint8_t)((data[0] >> 24) & 0xff);
        respMadMsg.payload[(4 + i*8 + 5)] = (uint8_t)((data[0] >> 16) & 0xff);
        respMadMsg.payload[(4 + i*8 + 6)] = (uint8_t)((data[0] >> 8) & 0xff);
        respMadMsg.payload[(4 + i*8 + 7)] = (uint8_t)(data[0] & 0xff);
      }
      else
      {
        MSGREG(err3, 'E', "method not supported method- $ accessMode - $","vsAttribute50");
        MSGSND(err3,method,accessMode);
        status = IB_MAD_STATUS_UNSUP_METHOD ; // specified method not supported
      }

    }
    break;
  case 2:
    numOfRecords = (attrMod >> 16) & 0x3f;
    MSGREG(inf3, 'V', "enhanced scatter/gather list length $ ", "vsAttribute50");
    MSGSND(inf3,numOfRecords);
    if (numOfRecords > 19 )
    {
      MSGREG(err6, 'E', "number of records bigger than 19- $ accessMode - $","vsAttribute50");
      MSGSND(err6,numOfRecords,accessMode);
    }
    for ( uint32_t i=0;i < numOfRecords;i++)
    {
      startAddr = reqMadMsg.payload[(4 + i*12)]     << 24 |
        reqMadMsg.payload[(4 + i*12 + 1)] << 16 |
        reqMadMsg.payload[(4 + i*12 + 2)] << 8  |
        reqMadMsg.payload[(4 + i*12 + 3)];
      data[0]  = reqMadMsg.payload[(4 + i*12 + 4)] << 24 |
        reqMadMsg.payload[(4 + i*12 + 5)] << 16 |
        reqMadMsg.payload[(4 + i*12 + 6)] << 8 |
        reqMadMsg.payload[(4 + i*12 + 7)] ;
      mask      = reqMadMsg.payload[(4 + i*12 + 8)] << 24 |
        reqMadMsg.payload[(4 + i*12 + 9)] << 16 |
        reqMadMsg.payload[(4 + i*12 +10)] << 8  |
        reqMadMsg.payload[(4 + i*12 +11)];

      if ((startAddr >> 16)!= 0)
      {
        MSGREG(err4, 'E', "upper bits of address are not 0- $ accessMode - $","vsAttribute50");
        MSGSND(err4,startAddr,accessMode);
      }
      MSGREG(inf4, 'V', "Entry number %: address = $ data = $ mask = $ method = $", "vsAttribute50");
      MSGSND(inf4,i, startAddr, dataLine, mask, method);
      if (method == IB_MAD_METHOD_SET )
      {
        dataLine = dataLine & mask;
        pSimNode->getCrSpace(startAddr,1,data);
        data[0] = (data[0] * (~mask)) | dataLine;
        pSimNode->setCrSpace(startAddr,1,data);
      } else if (method == IB_MAD_METHOD_GET) {
        pSimNode->getCrSpace(startAddr,1,data);
        respMadMsg.payload[(4 + i*12 + 4)] = (uint8_t)((data[0] >> 24) & 0xff);
        respMadMsg.payload[(4 + i*12 + 5)] = (uint8_t)((data[0] >> 16) & 0xff);
        respMadMsg.payload[(4 + i*12 + 6)] = (uint8_t)((data[0] >> 8) & 0xff);
        respMadMsg.payload[(4 + i*12 + 7)] = (uint8_t)(data[0] & 0xff);
      }
      else
      {
        MSGREG(err3, 'E', "method not supported method- $ accessMode - $","vsAttribute50");
        MSGSND(err3,method,accessMode);
        status = IB_MAD_STATUS_UNSUP_METHOD ; // specified method not supported
      }
    }
    break;

  default:
    MSGREG(err3, 'E', "accessMod not supported - accessMode - $","vsAttribute50");
    MSGSND(err3,accessMode);
    status = 0x1c; //One or more fields in the attribute or attribute modifier contain an invalid value
    break;
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSVendorSpecific::vsAttribute51(
  ibms_mad_msg_t &respMadMsg,
  ibms_mad_msg_t &reqMadMsg,
  uint8_t inPort)
{
  ib_net16_t          status = 0;
  uint32_t        attrMod;
  uint32_t        startAddr;
  uint32_t        length;
  uint32_t        mask;
  uint32_t        data[56];
  uint32_t            dataLine = 0;
  uint32_t            accessMode;
  uint32_t            numOfRecords;
  uint8_t            method;

  MSG_ENTER_FUNC;
  attrMod = CL_NTOH32(reqMadMsg.header.attr_mod);
  method  = CL_NTOH32(reqMadMsg.header.method) & 0x000000ff;
  accessMode = (attrMod >> 26) & 0x3;
  MSGREG(inf0, 'V', "Access Mode is $ method is $ ", "vsAttribute51");
  MSGSND(inf0, accessMode);

  switch (accessMode) {
  case 0:
    startAddr = attrMod & 0xfffff;
    length = (attrMod >> 20) & 0x3f;
    if (length > 57)
    {
      MSGREG(err1, 'E', "data length bigger then 57 - $","vsAttribute51");
      MSGSND(err1,length);
    }
    if ((startAddr & 0x3) != 0)
    {
      MSGREG(err2, 'E', "start address is not aligned to Dwords - $","vsAttribute51");
      MSGSND(err2,startAddr);
    }
    MSGREG(inf1, 'V', "access parameters: address $ length $ ", "vsAttribute51");
    MSGSND(inf1,startAddr,length);
    if (method == IB_MAD_METHOD_SET )
    {
      for (unsigned int i=0;i<(length);i+=1) {
        data[i] =   reqMadMsg.payload[4 + i*4] << 24 |
          reqMadMsg.payload[(4 + i*4+1)] << 16 |
          reqMadMsg.payload[(4 + i*4+2)] << 8 |
          reqMadMsg.payload[(4 + i*4+3)] ;
      }
      pSimNode->setCrSpace(startAddr,length,data);
    } else if (method == IB_MAD_METHOD_GET) {
      pSimNode->getCrSpace(startAddr,length,data);
      for (unsigned int j=0;j<length;j++) {
        respMadMsg.payload[(4 + j*4)]   = (uint8_t)((data[j] >> 24) & 0xff);
        respMadMsg.payload[(4 + j*4+1)] = (uint8_t)((data[j] >> 16) & 0xff);
        respMadMsg.payload[(4 + j*4+2)] = (uint8_t)((data[j] >> 8) & 0xff);
        respMadMsg.payload[(4 + j*4+3)] = (uint8_t)(data[j] & 0xff);
      }
    }
    else
    {
      MSGREG(err3, 'E', "method not supported method- $ accessMode - $","vsAttribute51");
      MSGSND(err3,method,accessMode);
      status = IB_MAD_STATUS_UNSUP_METHOD ; // specified method not supported
    }
    break;
  case 1:
    numOfRecords = (attrMod >> 20) & 0x3f;
    MSGREG(inf2, 'V', "scatter/gather list length $ ", "vsAttribute50");
    MSGSND(inf2,numOfRecords);
    if (numOfRecords > 28 )
    {
      MSGREG(err5, 'E', "number of records bigger than 28- $ accessMode - $","vsAttribute50");
      MSGSND(err5,numOfRecords,accessMode);
    }
    for ( uint32_t i=0;i < numOfRecords;i++)
    {
      startAddr = reqMadMsg.payload[(4 + i*8)]     << 24 |
        reqMadMsg.payload[(4 + i*8 + 1)] << 16 |
        reqMadMsg.payload[(4 + i*8 + 2)] << 8  |
        reqMadMsg.payload[(4 + i*8 + 3)];
      data[0]  = reqMadMsg.payload[(4 + i*8 + 4)] << 24 |
        reqMadMsg.payload[(4 + i*8 + 5)] << 16 |
        reqMadMsg.payload[(4 + i*8 + 6)] << 8 |
        reqMadMsg.payload[(4 + i*8 + 7)] ;
      if ((startAddr >> 20)!= 0)
      {
        MSGREG(err4, 'E', "upper bits of address are not 0- $ accessMode - $","vsAttribute51");
        MSGSND(err4,startAddr,accessMode);
      }
      MSGREG(inf4, 'V', "Entry number $: address = $ data = $ method = $", "vsAttribute51");
      MSGSND(inf4,i, startAddr, dataLine ,method);
      if (method == IB_MAD_METHOD_SET )
      {
        pSimNode->setCrSpace(startAddr,1,data);
      } else if (method == IB_MAD_METHOD_GET) {
        pSimNode->getCrSpace(startAddr,1,data);
        respMadMsg.payload[(4 + i*8 + 4)] = (uint8_t)((data[0] >> 24) & 0xff);
        respMadMsg.payload[(4 + i*8 + 5)] = (uint8_t)((data[0] >> 16) & 0xff);
        respMadMsg.payload[(4 + i*8 + 6)] = (uint8_t)((data[0] >> 8) & 0xff);
        respMadMsg.payload[(4 + i*8 + 7)] = (uint8_t)(data[0] & 0xff);
      }
      else
      {
        MSGREG(err3, 'E', "method not supported method- $ accessMode - $","vsAttribute51");
        MSGSND(err3,method,accessMode);
        status = IB_MAD_STATUS_UNSUP_METHOD ; // specified method not supported
      }
    }
    break;
  case 2:
    numOfRecords = (attrMod >> 20) & 0x3f;
    MSGREG(inf3, 'V', "enhanced scatter/gather list length $ ", "vsAttribute51");
    MSGSND(inf3,numOfRecords);
    if (numOfRecords > 19 )
    {
      MSGREG(err6, 'E', "number of records bigger than 19- $ accessMode - $","vsAttribute51");
      MSGSND(err6,numOfRecords,accessMode);
    }
    for ( uint32_t i=0;i < numOfRecords;i++)
    {
      startAddr = reqMadMsg.payload[(4 + i*12)]     << 24 |
        reqMadMsg.payload[(4 + i*12 + 1)] << 16 |
        reqMadMsg.payload[(4 + i*12 + 2)] << 8  |
        reqMadMsg.payload[(4 + i*12 + 3)];
      dataLine  = reqMadMsg.payload[(4 + i*12 + 4)] << 24 |
        reqMadMsg.payload[(4 + i*12 + 5)] << 16 |
        reqMadMsg.payload[(4 + i*12 + 6)] << 8 |
        reqMadMsg.payload[(4 + i*12 + 7)] ;
      mask      = reqMadMsg.payload[(i*12 + 8)] << 24 |
        reqMadMsg.payload[(4 + i*12 + 9)] << 16 |
        reqMadMsg.payload[(4 + i*12 +10)] << 8  |
        reqMadMsg.payload[(4 + i*12 +11)];

      if ((startAddr >> 20)!= 0)
      {
        MSGREG(err4, 'E', "upper bits of address are not 0- $ accessMode - $","vsAttribute51");
        MSGSND(err4,startAddr,accessMode);
      }
      MSGREG(inf4, 'V', "Entry number %: address = $ data = $ mask = $ method = $", "vsAttribute51");
      MSGSND(inf4,i, startAddr, dataLine, mask, method);
      if (method == IB_MAD_METHOD_SET )
      {
        dataLine = dataLine & mask;
        pSimNode->getCrSpace(startAddr,1,data);
        data[0] = data[0] * (~mask) | dataLine;
        pSimNode->setCrSpace(startAddr,1,data);
      } else if (method == IB_MAD_METHOD_GET) {
        pSimNode->getCrSpace(startAddr,1,data);
        respMadMsg.payload[(4 + i*12 + 4)] = (uint8_t)((data[0] >> 24) & 0xff);
        respMadMsg.payload[(4 + i*12 + 5)] = (uint8_t)((data[0] >> 16) & 0xff);
        respMadMsg.payload[(4 + i*12 + 6)] = (uint8_t)((data[0] >> 8) & 0xff);
        respMadMsg.payload[(4 + i*12 + 7)] = (uint8_t)(data[0] & 0xff);
      }
      else
      {
        MSGREG(err3, 'E', "method not supported method- $ accessMode - $","vsAttribute51");
        MSGSND(err3,method,accessMode);
        status = IB_MAD_STATUS_UNSUP_METHOD ; // specified method not supported
      }
    }
    break;

  default:
    MSGREG(err3, 'E', "accessMod not supported - accessMode - $","vsAttribute51");
    MSGSND(err3,accessMode);
    status = 0x1c; //One or more fields in the attribute or attribute modifier contain an invalid value
    break;
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSVendorSpecific::processMad(
  uint8_t inPort, ibms_mad_msg_t &madMsg)
{
  MSG_ENTER_FUNC;

  ibms_mad_msg_t      respMadMsg;
  ib_net16_t          status = 0;
  uint16_t            attributeId = 0;
  uint8_t            mgmtClass = 0;

  //1. Verify rcv Vendor Specific MAD validity.
  status = vsMadValidation(madMsg);
  if (status != 0)
  {
    //TODO need to do it more cleanly
    MSG_EXIT_FUNC;
    return status;
  }

  //2. Switch according to the attribute to the mad handle
  //      and call appropriate function
  attributeId = (madMsg.header.attr_id >> 8);
  mgmtClass = madMsg.header.mgmt_class;
  MSGREG(inf0, 'I', "Process Mad got the following attribute: $ !", "processMad");
  MSGSND(inf0, attributeId);
  // copy header from request to the response
  {
    ib_mad_t*   pRespVs = (ib_mad_t*)(&respMadMsg.header);
    ib_mad_t*   pReqVs = (ib_mad_t*)(&madMsg.header);
    memcpy(pRespVs, pReqVs, sizeof(ib_mad_t));
    respMadMsg.header.method = IB_MAD_METHOD_GET_RESP;
  }

  switch (mgmtClass) {
  case 0x9:
    MSGREG(inf3, 'I', "Vendor Specific Class 0x9 !", "processMad");
    MSGSND(inf3);
    switch (attributeId) {
    case 0x50:
      MSGREG(inf1, 'I', "Access InfiniBridge, InfiniScale or InfiniScale2 (attr 50) !", "processMad");
      MSGSND(inf1);
      status = vsAttribute50(respMadMsg, madMsg, inPort);
      break;
    case 0x51:
      MSGREG(inf2, 'I', "Access InfiniHost* !", "processMad");
      MSGSND(inf2);
      status = vsAttribute51(respMadMsg, madMsg, inPort);
      break;
    default:
      MSGREG(err1, 'E', "No handler for requested attribute:$", "processMad");
      MSGSND(err1, cl_ntoh16(attributeId));
      status = IB_MAD_STATUS_UNSUP_METHOD_ATTR;
      //TODO need to do it more cleanly
      MSG_EXIT_FUNC;
      break;
    }
    break;
  case 0xa:
    MSGREG(inf5, 'I', "Vendor Specific Class 0xa !", "processMad");
    MSGSND(inf5);
    break;
  default:
    MSGREG(err2, 'E', "No handler for requested class:$", "processMad");
    MSGSND(err2, cl_ntoh16(mgmtClass));
    status = IB_MAD_STATUS_UNSUP_CLASS_VER;


  }

  //send response
  respMadMsg.addr.slid = madMsg.addr.dlid;
  respMadMsg.addr.dlid = madMsg.addr.slid;
  MSGREG(inf6, 'V', "sending response mad - lid - ", "processMad");
  MSGSND(inf6);
  respMadMsg.header.status = status;
  pSimNode->getSim()->getDispatcher()->dispatchMad(pSimNode, inPort, respMadMsg);

  MSG_EXIT_FUNC;
  return status;
}



void IBMSVendorSpecific::crSpaceInit(
  uint16_t devId) {
  /* configure CrSpace according to node type -
     1/2/3 - Tavor/Arbel/Sinai
     10/11 -  anafa/anafa2   */
  uint32_t portGuidLow;
  uint32_t portGuidHigh;
  MSG_ENTER_FUNC;
  MSGREG(inf1, 'I', "configuring device CrSpace - Type $", "crSpaceInit");
  MSGSND(inf1,devId);
  switch (devId) {
  case 23108 :  // tavor/arbel -2 ports
    pSimNode->crSpace[0x10120] = 0;
    pSimNode->crSpace[0x10124] = 0;
    pSimNode->crSpace[0x10128] = 0;
    pSimNode->crSpace[0x1012c] = 0;
    pSimNode->crSpace[0x10130] = 0;
    pSimNode->crSpace[0x10134] = 0;
    pSimNode->crSpace[0x10138] = 0;
    pSimNode->crSpace[0x10144] = 0;
    pSimNode->crSpace[0x10148] = 0;
    pSimNode->crSpace[0x1014c] = 0;
    pSimNode->crSpace[0x10150] = 0;
    pSimNode->crSpace[0x10154] = 0;
    pSimNode->crSpace[0x10164] = 0;
    pSimNode->crSpace[0x10920] = 0;
    pSimNode->crSpace[0x10924] = 0;
    pSimNode->crSpace[0x10928] = 0;
    pSimNode->crSpace[0x1092c] = 0;
    pSimNode->crSpace[0x10930] = 0;
    pSimNode->crSpace[0x10934] = 0;
    pSimNode->crSpace[0x10938] = 0;
    pSimNode->crSpace[0x10944] = 0;
    pSimNode->crSpace[0x10948] = 0;
    pSimNode->crSpace[0x1094c] = 0;
    pSimNode->crSpace[0x10950] = 0;
    pSimNode->crSpace[0x10954] = 0;
    pSimNode->crSpace[0x10964] = 0;
    pSimNode->crSpace[0xf0014] = 0x00a05a44; // RevId DevId
    portGuidHigh = pSimNode->nodeInfo.port_guid & 0xffffffff ;
    portGuidLow =  ((pSimNode->nodeInfo.port_guid >> 32) & 0xffffffff) + 1;
    pSimNode->crSpace[0x8232c] = portGuidHigh; // Guid High port 1
    pSimNode->crSpace[0x8232c] = portGuidLow; // Guid Low  port 1
    portGuidLow++;
    pSimNode->crSpace[0x82334] = portGuidHigh; // Guid High port 2
    pSimNode->crSpace[0x82338] = portGuidLow; // Guid Low  port 2
  case 25208 :
    pSimNode->crSpace[0x10120] = 0;
    pSimNode->crSpace[0x10124] = 0;
    pSimNode->crSpace[0x10128] = 0;
    pSimNode->crSpace[0x1012c] = 0;
    pSimNode->crSpace[0x10130] = 0;
    pSimNode->crSpace[0x10134] = 0;
    pSimNode->crSpace[0x10138] = 0;
    pSimNode->crSpace[0x10144] = 0;
    pSimNode->crSpace[0x10148] = 0;
    pSimNode->crSpace[0x1014c] = 0;
    pSimNode->crSpace[0x10150] = 0;
    pSimNode->crSpace[0x10154] = 0;
    pSimNode->crSpace[0x10164] = 0;
    pSimNode->crSpace[0x10920] = 0;
    pSimNode->crSpace[0x10924] = 0;
    pSimNode->crSpace[0x10928] = 0;
    pSimNode->crSpace[0x1092c] = 0;
    pSimNode->crSpace[0x10930] = 0;
    pSimNode->crSpace[0x10934] = 0;
    pSimNode->crSpace[0x10938] = 0;
    pSimNode->crSpace[0x10944] = 0;
    pSimNode->crSpace[0x10948] = 0;
    pSimNode->crSpace[0x1094c] = 0;
    pSimNode->crSpace[0x10950] = 0;
    pSimNode->crSpace[0x10954] = 0;
    pSimNode->crSpace[0x10964] = 0;
    pSimNode->crSpace[0xf0014] = 0x00a06278; // RevId DevId
    portGuidHigh = pSimNode->nodeInfo.port_guid & 0xffffffff ;
    portGuidLow =  ((pSimNode->nodeInfo.port_guid >> 32) & 0xffffffff) + 1;
    pSimNode->crSpace[0x8232c] = portGuidHigh; // Guid High port 1
    pSimNode->crSpace[0x8232c] = portGuidLow; // Guid Low  port 1
    portGuidLow++;
    pSimNode->crSpace[0x82334] = portGuidHigh; // Guid High port 2
    pSimNode->crSpace[0x82338] = portGuidLow; // Guid Low  port 2

  case 25204 : // sinai - 1 port
    pSimNode->crSpace[0x10120] = 0;
    pSimNode->crSpace[0x10124] = 0;
    pSimNode->crSpace[0x10128] = 0;
    pSimNode->crSpace[0x1012c] = 0;
    pSimNode->crSpace[0x10130] = 0;
    pSimNode->crSpace[0x10134] = 0;
    pSimNode->crSpace[0x10138] = 0;
    pSimNode->crSpace[0x10144] = 0;
    pSimNode->crSpace[0x10148] = 0;
    pSimNode->crSpace[0x1014c] = 0;
    pSimNode->crSpace[0x10150] = 0;
    pSimNode->crSpace[0x10154] = 0;
    pSimNode->crSpace[0x10164] = 0;
    pSimNode->crSpace[0xf0014] = 0x00a06274; // RevId DevId
    portGuidHigh = pSimNode->nodeInfo.port_guid & 0xffffffff ;
    portGuidLow =  ((pSimNode->nodeInfo.port_guid >> 32) & 0xffffffff) + 1;
    pSimNode->crSpace[0x8232c] = portGuidHigh; // Guid High port 1
    pSimNode->crSpace[0x8232c] = portGuidLow; // Guid Low  port 1
  case 47396 : // anafa2
    for (int i=0;i<24;i++) {
      pSimNode->crSpace[(0x102120 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102124 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102128 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x10212c + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102130 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102134 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102138 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102144 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102148 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x10214C + i*0x1000)] = 0;
      pSimNode->crSpace[(0x102164 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x1027A8 + i*0x1000)] = 0;
      pSimNode->crSpace[(0x1027AC + i*0x1000)] = 0;
    }
    pSimNode->crSpace[0x60014] = 0x00a0b924; // RevId DevId
  case 43132 : // anafa
    for (int i=0;i<8;i++) {
      pSimNode->crSpace[(0x8120 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8124 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8128 + i*0x400)] = 0;
      pSimNode->crSpace[(0x812C + i*0x400)] = 0;
      pSimNode->crSpace[(0x8130 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8134 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8138 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8144 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8148 + i*0x400)] = 0;
      pSimNode->crSpace[(0x814C + i*0x400)] = 0;
      pSimNode->crSpace[(0x8150 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8154 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8160 + i*0x400)] = 0;
      pSimNode->crSpace[(0x8164 + i*0x400)] = 0;
    }
    pSimNode->crSpace[0x3014] = 0x00a0a87c ;// RevId DevId
  }
  MSG_EXIT_FUNC;
}



