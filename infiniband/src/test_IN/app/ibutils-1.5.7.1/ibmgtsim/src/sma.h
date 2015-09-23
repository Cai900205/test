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

/****h* IBMS/Subnet Manager Agent
* NAME
*	IB Subnet Manager Agent Simulator
*
* DESCRIPTION
*	The top level object of the subnet manager agent simulator
*
* AUTHOR
*	Nimrod Gindi, Mellanox
*
*********/

#ifndef SMA_H
#define SMA_H

#include <pthread.h>
#include <vector>
#include <unistd.h>
#include <ibdm/Fabric.h>
#include "simmsg.h"
#include "server.h"
#include "dispatcher.h"
#include "node.h"
#include "ib_types_extend.h"

#define RCV 1
#define SND 2

/****f* IBMgtSma: ibms_dump_mad
* NAME
*	ibms_dump_mad
*
* DESCRIPTION
*	upper level function of printing all mad content in
*   receiving or sending of Mad
*
* SYNOPSIS
*/
void
ibms_dump_mad( const ibms_mad_msg_t &madMsg, const uint8_t dir);
/*
* PARAMETERS
*	madHeader
*		[in] The Mad it self
*
* RETURN VALUE
*	no returned value
*
* NOTES
*
* SEE ALSO
*
*********/

// Returns 0 if entry should be removed and 1 otherwise
typedef int (*cbFunc)(void*);

typedef struct reg_
{
  cbFunc f;
  void* data;
} reg_t;

class SMATimer
{
  // Mutex of the timer
  pthread_mutex_t timerMutex;
  // Timer thread function
  static void* timerRun(void* p);
  // Thread id
  int tid;
  // Thread
  pthread_t th;
  // Registered objects list
  vector<reg_t> L;
  // Sleep time
  int time;

 public:
  SMATimer(int time);
  void terminate();
  // Timer registration function
  void reg(reg_t r);
  // Removes registered object identified by POINTER
  void unreg(void* data);
};

#define T_FREQ 1

typedef struct portTiming_
{
  ib_port_info_t* pInfo;
  unsigned counter;
  int timerOn;
  pthread_mutex_t mut;
} portTiming;

class IBMSSma : IBMSMadProcessor {

  // m_key callback function
  static int cbMkey(void* data);
  // M_key timer
  static SMATimer mkeyTimer;

  vector<portTiming> vPT;

  /* init functions of node structures */
  void initSwitchInfo();
  void initNodeInfo();
  void initGuidInfo();
  void initPortInfo();
  void initPKeyTables();
  void initMftTable();
  void initCaSl2VlTable();
  void initSwitchSl2VlTable();
  void initVlArbitTable();

  /* Mad Validation */
  int madValidation(ibms_mad_msg_t &madMsg);

  /* ----------------------------
        Attributes Handling
     ----------------------------*/
  /* NodeInfo */
  int nodeInfoMad(ibms_mad_msg_t &respMadMsg, uint8_t inPort);
  /* NodeDesc */
  int nodeDescMad(ibms_mad_msg_t &respMadMsg);
  /* PortInfo */
  int setIBPortBaseLid(IBMSNode *pSimNode,
                       uint8_t   portNum,
                       uint16_t base_lid);
  int setPortInfoHca(ibms_mad_msg_t &respMadMsg,
                     ibms_mad_msg_t &reqMadMsg,
                     uint8_t        inPort,
                     ib_port_info_t portInfoElm,
                     int            portNum);
  int setPortInfoSwExtPort(ibms_mad_msg_t &respMadMsg,
                           ibms_mad_msg_t &reqMadMsg,
                           uint8_t        inPort,
                           ib_port_info_t portInfoElm,
                           int            portNum);
  int setPortInfoSwBasePort(ibms_mad_msg_t &respMadMsg,
                            ibms_mad_msg_t &reqMadMsg,
                            uint8_t        inPort,
                            ib_port_info_t portInfoElm,
                            int            portNum);
  int setPortInfoGeneral(ibms_mad_msg_t &respMadMsg,
                         ibms_mad_msg_t &reqMadMsg,
                         uint8_t        inPort,
                         ib_port_info_t portInfoElm,
                         int            portNum);
  int setPortInfo(ibms_mad_msg_t &respMadMsg,
                  ibms_mad_msg_t &reqMadMsg,
                  uint8_t        inPort,
                  ib_port_info_t portInfoElm,
                  int            portNum);
  int portInfoMad(ibms_mad_msg_t &respMadMsg,
                  ibms_mad_msg_t &reqMadMsg,
                  uint8_t        inPort);
  /* P_Key */
  int pKeyMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort);
  /* SwitchInfo */
  int switchInfoMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg);
  /* LFT */
  int lftMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg);
  /* MFT */
  int mftMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg);
  /* SL to VL */
  int sl2VlMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort);
  /* VL Arbitration */
  int vlArbMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort);
  /* GuidInfo */
  int guidInfoMad(ibms_mad_msg_t &respMadMsg, ibms_mad_msg_t &reqMadMsg, uint8_t inPort);

 public:
  /* Top level of handling the SMA MAD. Might result with a call to the
     outstandingMads->push() with a result                     */
  int processMad(uint8_t inPort, ibms_mad_msg_t &madMsg);

  /* Constructor - should initial the specific class elements
     in the node. */
  IBMSSma(IBMSNode *pSNode, list_uint16 mgtClasses);

};

#endif /* SMA_H */
