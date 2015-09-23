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

/****h* IBMgtSim/Helper
 * NAME
 *	 Helper
 *
 * DESCRIPTION
 * 	Provide Some Helper Functions for Printing the content of the messages.
 *
 *
 * $Revision: 1.7 $
 *
 * AUTHOR
 *	Eitan Zahavi, Mellanox
 *
 *********/

#ifndef IBMGTSIM_HELPER_H
#define IBMGTSIM_HELPER_H

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif

#include "simmsg.h"

/****f* IBMgtSim: ClientIfc/ibms_dump_msg
* NAME
*	ibms_dump_msg
*
* DESCRIPTION
*	Dump the given message
*
* SYNOPSIS
*/
void
ibms_dump_msg(IN const ibms_client_msg_t *p_msg);
/*
* PARAMETERS
*	p_msg
*		[in] The message
*
* RETURN VALUE
*	NONE
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* IBMgtSim: ClientIfc/ibms_get_msg_str
* NAME
*	ibms_get_msg_str
*
* DESCRIPTION
*	return a string with the given message content
*
* SYNOPSIS
*/
std::string
ibms_get_msg_str(IN const ibms_client_msg_t *p_msg);
/*
* PARAMETERS
*	p_msg
*		[in] The message
*
* RETURN VALUE
*	NONE
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* IBMgtSim: ibms_get_mad_header_str
* NAME
*	ibms_get_mad_header_str
*
* DESCRIPTION
*	return a string with the given mad header content
*
* SYNOPSIS
*/
std::string
ibms_get_mad_header_str(ib_mad_t madHeader);
/*
* PARAMETERS
*	madHeader
*		[in] The mad header
*
* RETURN VALUE
*	string with the information to print to the display
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* IBMgtSim: ibms_get_portInfo_str
* NAME
*	ibms_get_portInfo_str
*
* DESCRIPTION
*	return a string with the given PortInfo content
*
* SYNOPSIS
*/
std::string
ibms_get_port_info_str(ib_port_info_t*     pPortInfo);
/*
* PARAMETERS
*	madHeader
*		[in] The mad header
*
* RETURN VALUE
*	string with the information to print to the display
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* IBMgtSim: ibms_get_node_info_str
* NAME
*	ibms_get_node_info_str
*
* DESCRIPTION
*	return a string with the given NodeInfo content
*
* SYNOPSIS
*/
std::string
ibms_get_node_info_str(ib_node_info_t*     pNodeInfo);
/*
* PARAMETERS
*	madHeader
*		[in] The mad header
*
* RETURN VALUE
*	string with the information to print to the display
*
* NOTES
*
* SEE ALSO
*
*********/

/****f* IBMgtSim: ClientIfc/ibms_get_resp_str
* NAME
*	ibms_get_resp_str
*
* DESCRIPTION
*	Get the string representing the message status
*
* SYNOPSIS
*/
char *
ibms_get_resp_str(IN const ibms_response_t *p_response);
/*
* PARAMETERS
*	p_msg
*		[in] The message
*
* RETURN VALUE
*	NONE
*
* NOTES
*
* SEE ALSO
*
*********/

#endif /* IBMGTSIM_HELPER_H */
