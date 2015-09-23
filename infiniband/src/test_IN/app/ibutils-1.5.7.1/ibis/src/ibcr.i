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
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>
#include "ibcr.h"
%}

%{

static ibcr_t *p_ibcr_global;

/*
   this function returns the string corresponding to the
   read cpu data
*/
char *
ibcr_get_cr_str(
  boolean_t is_multi,
  uint8_t num,
  ib_cr_space_t *p_cr_mads
  )
{
  char *p_res_str = 0;
  char buff[512];
  static int i, extra;

  if (p_cr_mads) {
    for (i=0;i<num;i++) {
      if (p_cr_mads[i].mad_header.method != VENDOR_GET_RESP) {
        if (is_multi) {
          sprintf(buff,"TARGET_ERROR : Fail to obtain CR mad response");
        } else {
          sprintf(buff,"ERROR : Fail to obtain CR mad response");
        }
      } else if (ibis_get_mad_status((ib_mad_t*)&p_cr_mads[i]) != 0) {
        if (is_multi) {
          sprintf(buff,"TARGET_ERROR : Got remote error:0x%x",
                  ibis_get_mad_status((ib_mad_t*)&p_cr_mads[i]));
        } else {
          sprintf(buff,"ERROR : Got remote error:0x%x",
                  ibis_get_mad_status((ib_mad_t*)&p_cr_mads[i]));
        }
      } else {
        sprintf(buff, "{vendor_key 0x%016" PRIx64 "} {data 0x%x}",
                cl_ntoh64(p_cr_mads[i].vendor_key),
                cl_ntoh32(p_cr_mads[i].data[0]));
      }

      if (is_multi) extra = 3; else extra = 0;

      if (p_res_str) {
        p_res_str =
          (char *)realloc(p_res_str,strlen(p_res_str)+strlen(buff) + 1+ extra);
      } else {
        p_res_str = (char *)malloc(strlen(buff) + 1+ extra);
        p_res_str[0] = '\0';
      }

      /* need an extra list wrap */
      if (is_multi) {
        strcat(p_res_str,"{");
        strcat(p_res_str, buff);
        strcat(p_res_str,"} ");
      } else {
        strcat(p_res_str, buff);
      }
    }
  }
  return(p_res_str);
}

int
ibcr_destroy_global(void)
{
	ibcr_destroy(p_ibcr_global);
	return (0);
}

int
ibcr_num_of_multi_max(void)
{
	return (IBCR_MULTI_MAX);
}

int
ibcr_read_global(
  uint16_t lid,
  uint32_t address,
  char **pp_new_cr_str)
{
	ib_api_status_t status;
   ib_cr_space_t   cr_space_mads_arr[1];

	status = ibcr_read(p_ibcr_global,lid,address,cr_space_mads_arr);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to read CR space");
   } else {
     *pp_new_cr_str = ibcr_get_cr_str(FALSE, 1, cr_space_mads_arr);
   }

	return(status);
}

int
ibcr_write_global(
  uint16_t lid,
  uint32_t data,
  uint32_t address)
{

	ib_api_status_t status;

	status = ibcr_write(p_ibcr_global,lid,data,address);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to write CR space");
   }
	return(status);
}

int
ibcr_multi_read_global(
  uint8_t num,
  uint16_t lid_list[],
  uint32_t address,
  char **pp_new_cr_str)
{
	ib_api_status_t status;
   ib_cr_space_t   cr_space_mads_arr[IBCR_MULTI_MAX];

	status =
     ibcr_multi_read(p_ibcr_global,num,lid_list,address,cr_space_mads_arr);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to read all targets CR space");
   } else {
     *pp_new_cr_str = ibcr_get_cr_str(TRUE, num, cr_space_mads_arr);
   }
	return(status);
}

int
ibcr_multi_write_global(
  uint8_t num,
  uint16_t lid_list[],
  uint32_t data,
  uint32_t address)
{

	ib_api_status_t status;

	status = ibcr_multi_write(p_ibcr_global,num,lid_list,data,address);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to write all targets CR space");
   }
	return(status);
}

%}

//
// STANDARD IB TYPE MAPS:
//

%{
#define uint16_cr_arr_t uint16_t
%}
%typemap(tcl8,in) uint16_cr_arr_t *(uint16_t temp[IBCR_MULTI_MAX]) {
    char *str;
    char *str_tcl;
    int i;
    char *loc_buf;
    char *str_token = NULL;

    str_tcl = Tcl_GetStringFromObj($source,NULL);
    loc_buf = (char *)malloc((strlen(str_tcl)+1)*sizeof(char));
    strcpy(loc_buf,str_tcl);


    str = strtok_r(loc_buf," ", &str_token);
    for (i=0;i<IBCR_MULTI_MAX;i++) {
	if (str == NULL) {
	    break;
	}
	temp[i] = strtoul(str, NULL, 0);
	str = strtok_r(NULL," ",&str_token);
    }
    $target = temp;
    free(loc_buf);
}

//
// IBCR MAD TYPE MAPS
//


%typemap(memberin) ib_cr_space_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+sizeof(ib_cr_space_t)*$dim0);
	}
}

%section "IBCR Functions",pre
/* IBCR UI functions */
%text %{
This section provide the details about the functions IBCR exposes.
They all return 0 on succes.
%}


%name(crDestroy) int ibcr_destroy_global();

%name(crMultiMaxGet) ibcr_num_of_multi_max();


%apply char **p_out_str {char **pp_new_cr_str};

%name(crRead) int ibcr_read_global(
  uint16_t lid,
  uint32_t address,
  char **pp_new_cr_str);

%name(crWrite) int ibcr_write_global(
  uint16_t lid,
  uint32_t data,
  uint32_t address);

%name(crReadMulti) int ibcr_multi_read_global(
  uint8_t num,
  uint16_cr_arr_t lid_list[],
  uint32_t address,
  char **pp_new_cr_str);

%name(crWriteMulti) int ibcr_multi_write_global(
  uint8_t num,
  uint16_cr_arr_t lid_list[],
  uint32_t data,
  uint32_t address );

//
// INIT CODE
//
%init %{

 {

	}
%}

