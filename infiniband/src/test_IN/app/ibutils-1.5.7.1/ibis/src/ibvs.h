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
 * Abstract:
 * 	Implementation of ibcr_t.
 *	This object represents the Subnet Performance Monitor object.
 *	This object is part of the IBIS family of objects.
 *
 * Environment:
 * 	Linux User Mode
 *
 * $Revision: 1.3 $
 */

#ifndef _IBVS_H_
#define _IBVS_H_

#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>
#include "ibis_api.h"
#include "ibis.h"
#include "ibvs_base.h"


/****s* IBIS: ibvs/ibvs_t
* NAME  ibvs_t
*
*
* DESCRIPTION
*       ibvs structure.
*
* SYNOPSIS
*/


typedef struct _ibvs
{
  ibvs_state_t       state;
  osm_bind_handle_t  h_bind;
  osm_bind_handle_t  h_smp_bind;
} ibvs_t;


/*
* FIELDS
*
*       state
*            The ibvs condition state.
*
*       h_bind
*            The handle to bind with the lower level.
*
*
* SEE ALSO
*
*********/


/****f* IBIS: ibvs/ibvs_construct
* NAME
*       ibvs_construct
*
* DESCRIPTION
*      Allocation of ibvs_t struct
*
* SYNOPSIS
*/

ibvs_t*
ibvs_construct(void);

/*
* PARAMETERS
*
*
* RETURN VALUE
*       Return a pointer to an ibvs struct. Null if fails to do so.
*
* NOTES
*       First step of the creation of ibvs_t
*
* SEE ALSO
*       ibvs_destroy ibvs_init
*********/

/****s* IBIS: ibvs/ibvs_destroy
* NAME
*       ibvs_destroy
*
* DESCRIPTION
*      release of ibvs_t struct
*
* SYNOPSIS
*/

void
ibvs_destroy(
  IN ibvs_t* const p_ibvs );

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct that is about to be released
*
* RETURN VALUE
*
* NOTES
*       Final step of the releasing of ibvs_t
*
* SEE ALSO
*       ibvs_construct
*********/

/****f* IBIS: ibvs/ibvs_init
* NAME
*       ibvs_init
*
* DESCRIPTION
*      Initialization of an ibvs_t struct
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_init(
  IN ibvs_t* const p_ibvs );

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct that is about to be initialized
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_construct
* *********/


/****f* IBIS: ibvs/ibvs_bind
* NAME
*       ibvs_bind
*
* DESCRIPTION
*      Binding the ibvs object to a lower level.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_bind(
  IN ibvs_t* const p_ibvs );

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct that is about to be binded
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_construct
*********/

/****f* IBIS: ibvs/ibvs_cpu_read
* NAME
*     ibvs_cpu_read
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_cpu_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t size,
  IN uint8_t cpu_traget_size,
  IN uint32_t address,
  OUT ib_vs_t *p_vs_mad);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       size
*               The size of the burst in DWORD.
*
*       cpu_target_size
*               The width of the cpu bus. (0-32bit 1-16bit 2-8bit).
*
*       address
*               The external port address in which to read from.
*
*       p_vs_mad
*               A pointer to a Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_cpu_write
*********/

/****f* IBIS: ibvs/ibvs_cpu_write
* NAME
*     ibvs_cpu_write
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_cpu_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t size,
  IN uint8_t cpu_traget_size,
  IN uint32_t *data,
  IN uint32_t address);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       size
*               The size of the burst in DWORD.
*
*       cpu_target_size
*               The width of the cpu bus. (0-32bit 1-16bit 2-8bit).
*
*       address
*               The external port address in which to write from.
*
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_cpu_read
*********/

/****f* IBIS: ibvs/ibvs_i2c_read
* NAME
*     ibvs_i2c_read
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_i2c_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t address,
  OUT ib_vs_t *p_vs_mad);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       port_num
*               The Destination i2c port (1/2).
*
*       size
*               The size of the burst in DWORD.
*
*       device_id
*               The device_id (address) on the i2c bus.
*
*       address
*               The external port address in which to read from.
*
*       p_vs_mad
*               A pointer to a Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_i2c_write
*********/

/****f* IBIS: ibvs/ibvs_i2c_write
* NAME
*     ibvs_i2c_write
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_i2c_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t *data,
  IN uint32_t address);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       port_num
*               The Destination i2c port (1/2).
*
*       size
*               The size of the burst in DWORD.
*
*       device_id
*               The device_id (address) on the i2c bus.
*
*       data
*               The array of data.
*
*       address
*               The external port address in which to write from.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_i2c_read
*********/


/****f* IBIS: ibvs/ibvs_multi_i2c_read
* NAME
*     ibvs_multi_i2c_read
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_i2c_read(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t address,
  OUT ib_vs_t *vs_mad_arr);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       port_num
*               The Destination i2c port (1/2).
*
*       size
*               The size of the burst in DWORD.
*
*       device_id
*               The device_id (address) on the i2c bus.
*
*       address
*               The external port address in which to read from.
*
*       vs_mad_arr
*               An array of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_i2c_write
*********/

/****f* IBIS: ibvs/ibvs_multi_i2c_write
* NAME
*     ibvs_i2c_write
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_i2c_write(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t port_num,
  IN uint8_t size,
  IN uint8_t device_id,
  IN uint32_t *data,
  IN uint32_t address,
  OUT ib_vs_t *vs_mad_arr);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       port_num
*               The Destination i2c port (1/2).
*
*       size
*               The size of the burst in DWORD.
*
*       device_id
*               The device_id (address) on the i2c bus.
*
*       data
*               The array of data.
*
*       address
*               The external port address in which to write from.
*
*       vs_mad_arr
*               An array of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_i2c_read
*********/

/****f* IBIS: ibvs/ibvs_gpio_read
* NAME
*     ibvs_gpio_read
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_gpio_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  OUT ib_vs_t *p_vs_mad);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       p_vs_mad
*               A pointer to a Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_gpio_write
*********/

/****f* IBIS: ibvs/ibvs_gpio_write
* NAME
*     ibvs_gpio_write
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_gpio_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint64_t gpio_mask,
  IN uint64_t gpio_data);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       gpio_mask
*               Masking of the GPIO pins data.
*
*       gpio_data
*               The written value for the GPIO pins.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_gpio_read
*********/

/****f* IBIS: ibvs/ibvs_multi_sw_reset
* NAME
*     ibvs_multi_sw_reset
*
* DESCRIPTION
*      Send a Vendor Specific MAD (Ext Port Access) and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_sw_reset(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[]);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid_list
*               List of the Destination lids of the MADs.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_sw_reset
*********/

/****f* IBIS: ibvs/ibvs_multi_flash_open
* NAME
*     ibvs_multi_flash_open
*
* DESCRIPTION
*      Send a Vendor Specific MAD and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_flash_open(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t last,
  IN uint8_t size,
  IN uint32_t *data,
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[]);
/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       last
*               The last MAD in the open stream.
*
*       size
*               The size of the ucode chunk.
*
*       address
*               The address in which to store ucode.
*
*       p_vs_mad_list
*               A pointer to a list of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_flash_close
*********/

/****f* IBIS: ibvs/ibvs_multi_flash_close
* NAME
*     ibvs_multi_flash_close
*
* DESCRIPTION
*      Send a Vendor Specific MAD and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_flash_close(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t force,
  OUT ib_vs_t vs_mad_arr[]);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       force
*               Force close without authentication.
*
*       p_vs_mad_list
*               A pointer to a list of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_flash_open
*********/

/****f* IBIS: ibvs/ibvs_multi_flash_set_bank
* NAME
*     ibvs_multi_flash_set_bank
*
* DESCRIPTION
*      Send a Vendor Specific MAD and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_flash_set_bank(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[]);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       address
*               The address in which to set bank.
*
*       p_vs_mad_list
*               A pointer to a list of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_flash_erase
*********/

/****f* IBIS: ibvs/ibvs_multi_flash_erase
* NAME
*     ibvs_multi_flash_erase
*
* DESCRIPTION
*      Send a Vendor Specific MAD and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_flash_erase(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[]);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       address
*               The address in which to erase.
*
*       p_vs_mad_list
*               A pointer to a list of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_flash_set_bank
*********/


/****f* IBIS: ibvs/ibvs_multi_flash_read
* NAME
*     ibvs_multi_flash_read
*
* DESCRIPTION
*      Send a Vendor Specific MAD and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_flash_read(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t size,
  IN uint32_t address,
  OUT ib_vs_t vs_mad_arr[]);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       size
*               The size of the burst in DWORD.
*
*       address
*               The address in which to read from.
*
*       p_vs_mad_list
*               A pointer to a list of Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_flash_write
*********/

/****f* IBIS: ibvs/ibvs_multi_flash_write
* NAME
*     ibvs_flash_write
*
* DESCRIPTION
*      Send a Vendor Specific MAD and wait for the reply.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_multi_flash_write(
  IN ibvs_t* const p_ibvs,
  IN uint8_t num,
  IN uint16_t lid_list[],
  IN uint8_t size,
  IN uint32_t *data,
  IN uint32_t address);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       num
*               number of MAD to be send.
*
*       lid_list
*               List of Destination lid of the MAD.
*
*       size
*               The size of the burst in DWORD.
*
*       data
*               The array of data.
*
*       address
*               The address in which to write from.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_multi_flash_read
*********/


/****f* IBIS: ibvs/ibvs_mirror_read
* NAME
*     ibvs_mirror_read
*
* DESCRIPTION
*      Read a Mirror Vendor Specific MAD.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_mirror_read(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  OUT ib_vs_t *p_vs_mad);

/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       p_vs_mad
*               A pointer to a Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_mirror_write
*********/

/****f* IBIS: ibvs/ibvs_mirror_write
* NAME
*     ibvs_mirror_write
*
* DESCRIPTION
*      Send a Mirror Vendor Specific MAD.
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_mirror_write(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint32_t rx_mirror,
  IN uint32_t tx_mirror);
/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       rx_mirror
*               From which port to mirror
*
*       tx_mirror
*               To which port to mirror
*
* RETURN VALUE
*       The status of the function.
*
* NOTES
*
* SEE ALSO
*       ibvs_mirror_read
*********/

/****f* IBIS: ibvs/ibvs_plft_map_get
* NAME
*     ibvs_plft_map_get
*
* DESCRIPTION
*      Get Private LFT Map
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_plft_map_get(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  IN uint8_t upper_ports,
  OUT ib_vs_t *p_vs_mad);
/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       upper_ports
*               In non zero will return the upper ports map
*
*       p_vs_mad
*               A pointer to a Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the get
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBIS: ibvs/ibvs_general_info_get
* NAME
*     ibvs_general_info_get
*
* DESCRIPTION
*      Get General Info
*
* SYNOPSIS
*/
ib_api_status_t
ibvs_general_info_get(
  IN ibvs_t* const p_ibvs,
  IN uint16_t lid,
  OUT ib_vs_t *p_vs_mad);
/*
* PARAMETERS
*       p_ibvs
*               A pointer to the ibvs_t struct.
*
*       lid
*               The Destination lid of the MAD.
*
*       p_vs_mad
*               A pointer to a Vendor Specific MAD that was received.
*
* RETURN VALUE
*       The status of the get
*
* NOTES
*
* SEE ALSO
*********/

#endif /* _IBVS_H_ */
