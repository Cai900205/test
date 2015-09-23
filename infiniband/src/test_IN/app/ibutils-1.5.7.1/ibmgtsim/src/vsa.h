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

/****h* IBMS/Vendor specific Agent
* NAME
*	Vendor specific Agent Simulator
*
* DESCRIPTION
*	The top level object of the vendor specific agent simulator
*
* AUTHOR
*	Nimrod Gindi, Mellanox
*
*********/

#ifndef VENDOR_SPECIFIC_H
#define VENDOR_SPECIFIC_H


#include <complib/cl_packon.h>
typedef struct _ib_sim_cr_space
{
    ib_mad_t mad_header;
    ib_net64_t vendor_key;
    ib_net32_t data[56];
}	PACK_SUFFIX ib_sim_cr_space_t;
#include <complib/cl_packoff.h>

class IBMSVendorSpecific : IBMSMadProcessor {

  /* init functions of node structures */

  /* Mad Validation */
  int vsMadValidation(ibms_mad_msg_t &madMsg);

  /* ----------------------------
        Attributes Handling
     ----------------------------*/
  int vsAttribute50(ibms_mad_msg_t &respMadMsg,
                    ibms_mad_msg_t &reqMadMsg,
                    uint8_t        inPort);

  int vsAttribute51(ibms_mad_msg_t &respMadMsg,
                    ibms_mad_msg_t &reqMadMsg,
                    uint8_t        inPort);

 public:
   // init the CrSpace according to device type //
  void crSpaceInit(uint16_t devId);
  /* Top level of handling the Vendor Specific MAD. Might result with a call to the
     outstandingMads->push() with a result                     */
  int processMad(uint8_t inPort, ibms_mad_msg_t &madMsg);

  /* Constructor - should initial the specific class elements
     in the node. */
  IBMSVendorSpecific(IBMSNode *pSNode, list_uint16 mgtClasses);

  /* destructor - clean up from the node too */
  // ~IBMSVendorSpecific();

};

#endif

