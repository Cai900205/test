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

/****h* IBMS/Performance Manager Agent
* NAME
*	IB Performance Manager Agent Simulator
*
* DESCRIPTION
*	The top level object of the performance manager agent simulator
*
* AUTHOR
*	Nimrod Gindi, Mellanox
*
*********/

#ifndef PMA_H
#define PMA_H

#include "sma.h"

class IBMSPma : IBMSMadProcessor {

  /* init functions of node structures */

  /* Mad Validation */
  int pmaMadValidation(ibms_mad_msg_t &madMsg);

  /* ----------------------------
        Attributes Handling
     ----------------------------*/
  int pmaPortCounters(ib_pm_counters_t &respMadMsg,
                      ib_pm_counters_t &reqMadMsg,
                      uint8_t         inPort);


 public:
  /* Top level of handling the PMA MAD. Might result with a call to the
     outstandingMads->push() with a result                     */
  int processMad(uint8_t inPort, ibms_mad_msg_t &madMsg);

  /* Constructor - should initial the specific class elements
     in the node. */
  IBMSPma(IBMSNode *pSNode, uint16_t mgtClass);

  /* destructor - clean up from the node too */
  // ~IBMSPma();

};

#endif /* PMA_H */
