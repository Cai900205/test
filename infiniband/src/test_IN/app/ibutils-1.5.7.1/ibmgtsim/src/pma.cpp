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

/****h* IBMS/Pma
 * NAME
 * IB Performance Manager Agent Simulator object
 *
 * DESCRIPTION
 * The simulator routes mad messages to the target node. This node
 *  MadProcessor PMA class is provided to handle and respond to these mads.
 *
 * AUTHOR
 * Nimrod Gindi, Mellanox
 *
 *********/

#include "msgmgr.h"
#include "simmsg.h"
#include "sim.h"
#include "pma.h"
#include "sma.h"
#include "helper.h"
#include "dispatcher.h"
#include "node.h"

IBMSPma::IBMSPma(
  IBMSNode *pSNode, uint16_t mgtClass) :
  IBMSMadProcessor(pSNode, mgtClass)
{
  MSG_ENTER_FUNC;

  IBNode*     pNodeData;

  pNodeData = pSNode->getIBNode();

  MSG_EXIT_FUNC;
}

int IBMSPma::pmaMadValidation(
  ibms_mad_msg_t &madMsg)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;

  // we handle Get or Set
  if ((madMsg.header.method != 1) && (madMsg.header.method != 2))
  {
    MSGREG(err0, 'W', "We are not handling Method:$", "pmaMadValidation");
    MSGSND(err0, madMsg.header.method);
    status = IB_MAD_STATUS_INVALID_FIELD;

    MSG_EXIT_FUNC;
    return status;
  }

  //ibms_dump_mad( pmaMadMsg, RCV);

  MSG_EXIT_FUNC;
  return status;
}

int IBMSPma::pmaPortCounters(
  ib_pm_counters_t &respMadMsg,
  ib_pm_counters_t &reqMadMsg,
  uint8_t inPort)
{
  MSG_ENTER_FUNC;

  ib_net16_t          status = 0;
  uint8_t             portSelect;
  uint32_t            counterSelect;
  ib_pm_counters_t*   pPmCount;

  portSelect = reqMadMsg.port_select;
  counterSelect =  (uint32_t) CL_NTOH16(reqMadMsg.counter_select);

  switch (reqMadMsg.mad_header.method) {
  case IB_MAD_METHOD_GET:
    MSGREG(inf0, 'I', "Performance Get Counters !", "pmaPortCounters");
    MSGSND(inf0);

    pPmCount = pSimNode->getPhyPortPMCounter(portSelect, counterSelect);
    if (! pPmCount)
    {
      MSGREG(err3, 'E', "Failed to obtain Port Counters for Port:$", "pmaPortCounters");
      MSGSND(err3, portSelect);

      MSG_EXIT_FUNC;
      return IB_SA_MAD_STATUS_REQ_INVALID;
    }
    memcpy((void*) &respMadMsg, (void*) pPmCount, sizeof(ib_pm_counters_t));
    break;
  case IB_MAD_METHOD_SET:
    MSGREG(inf1, 'I', "Performance Set Counters !", "pmaPortCounters");
    MSGSND(inf1);

    status =
      pSimNode->setPhyPortPMCounter(portSelect, counterSelect, reqMadMsg);

    break;
  default:
    MSGREG(err2, 'E', "No such Method supported ", "pmaPortCounters");
    MSGSND(err2);

    MSG_EXIT_FUNC;
    return IB_MAD_STATUS_UNSUP_METHOD_ATTR;
  }

  MSG_EXIT_FUNC;
  return status;
}

int IBMSPma::processMad(
  uint8_t inPort,
  ibms_mad_msg_t &madMsg)
{
  MSG_ENTER_FUNC;
  ibms_mad_msg_t      respMadMsg;
  ib_pm_counters_t    pmaMadMsg;
  ib_pm_counters_t    respPmaMadMsg;
  ib_net16_t          status = 0;
  uint16_t            attributeId = 0;

  memset(&respPmaMadMsg, 0, sizeof(respPmaMadMsg));
  memcpy(&pmaMadMsg, &madMsg.header, sizeof(ib_pm_counters_t));

  //1. Verify rcv PMA MAD validity.
  status = pmaMadValidation(madMsg);
  if (status != 0)
  {
    //TODO need to do it more cleanly
    MSG_EXIT_FUNC;
    return status;
  }

  //2. Switch according to the attribute to the mad handle
  //      and call appropriate function
  MSGREG(inf0, 'I', "Process Mad got the following attribute: $ !", "processMad");
  MSGSND(inf0, cl_ntoh16(pmaMadMsg.mad_header.attr_id));
  attributeId = pmaMadMsg.mad_header.attr_id;

  switch (attributeId) {
  case IB_MAD_ATTR_PORT_CNTRS:
    MSGREG(inf1, 'I', "Attribute being handled is Port Counters !", "processMad");
    MSGSND(inf1);

    status = pmaPortCounters(respPmaMadMsg, pmaMadMsg, inPort);
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

  //send response
  //ibms_dump_mad( respPmaMadMsg, SND);
  // copy header from request to the response
  {
    ib_mad_t*   pRespGmp = (ib_mad_t*)(&respPmaMadMsg.mad_header);
    ib_mad_t*   pReqGmp = (ib_mad_t*)(&pmaMadMsg.mad_header);
    memcpy(pRespGmp, pReqGmp, sizeof(ib_mad_t));
  }
  memcpy(&respMadMsg.header, &respPmaMadMsg, sizeof(respPmaMadMsg));

  respMadMsg.addr = madMsg.addr;
  respMadMsg.addr.slid = madMsg.addr.dlid;
  respMadMsg.addr.dlid = madMsg.addr.slid;
  if (respMadMsg.header.method == IB_MAD_METHOD_SET)
  {
    respMadMsg.header.method = IB_MAD_METHOD_GET_RESP;
  }
  else respMadMsg.header.method |= IB_MAD_METHOD_RESP_MASK;

  pSimNode->getSim()->getDispatcher()->dispatchMad(pSimNode, inPort, respMadMsg);

  MSG_EXIT_FUNC;
  return status;
}
