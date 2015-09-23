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
#include <inttypes.h>
#include "ibpm.h"
%}

%{

static ibpm_t *p_ibpm_global;

int
ibpm_num_of_multi_max(void)
{
	return (IBPM_MULTI_MAX);
}

/*
   this function returns the string corresponding to the
   port counters.
*/
char *
ibpm_get_port_counters_str(
  uint8_t num,
  ib_pm_port_counter_t *p_counters)
{
  char *p_res_str = 0;
  char buff[1024];
  static int i;

  buff[0] = '\0';
  for (i=0;i<num;i++) {
    /* format the string */
    if (p_counters[i].mad_header.method == VENDOR_GET_RESP) {
      sprintf(buff,"{{port_select %u } {counter_select %u } {symbol_error_counter %u } {link_error_recovery_counter %u } {link_down_counter %u } {port_rcv_errors %u } {port_rcv_remote_physical_errors %u } {port_rcv_switch_relay_errors %u } {port_xmit_discard %u } {port_xmit_constraint_errors %u } {port_rcv_constraint_errors %u } {local_link_integrity_errors %u } {excesive_buffer_errors %u } {vl15_dropped %u } {port_xmit_data %u } {port_rcv_data %u } {port_xmit_pkts %u } {port_rcv_pkts %u }} ",
              p_counters[i].port_select,
              cl_ntoh16(p_counters[i].counter_select),
              cl_ntoh16(p_counters[i].symbol_error_counter),
              p_counters[i].link_error_recovery_counter,
              p_counters[i].link_down_counter,
              cl_ntoh16(p_counters[i].port_rcv_errors),
              cl_ntoh16(p_counters[i].port_rcv_remote_physical_errors),
              cl_ntoh16(p_counters[i].port_rcv_switch_relay_errors),
              cl_ntoh16(p_counters[i].port_xmit_discard),
              p_counters[i].port_xmit_constraint_errors,
              p_counters[i].port_rcv_constraint_errors,
              (p_counters[i].lli_errors_exc_buf_errors & 0xf0) >> 4,
              (p_counters[i].lli_errors_exc_buf_errors & 0x0f),
              cl_ntoh16(p_counters[i].vl15_dropped),
              cl_ntoh32(p_counters[i].port_xmit_data),
              cl_ntoh32(p_counters[i].port_rcv_data),
              cl_ntoh32(p_counters[i].port_xmit_pkts),
              cl_ntoh32(p_counters[i].port_rcv_pkts));
    } else {
      sprintf(buff,"{TARGET_ERROR : Fail to obtain port counters} ");
    }

    if (p_res_str) {
      p_res_str = (char *)realloc(p_res_str,strlen(p_res_str)+strlen(buff) + 1);
    } else {
      p_res_str = (char *)malloc(strlen(buff) + 1);
      p_res_str[0] = '\0';
    }
    strcat(p_res_str, buff);
  }
  return(p_res_str);
}

/*
   this function returns the string corresponding to the
   extended port counters.
*/
char *
ibpm_get_port_counters_extended_str(
  uint8_t num,
  ib_pm_port_counter_extended_t *p_counters)
{
  char *p_res_str = 0;
  char buff[1024];
  static int i;

  buff[0] = '\0';
  for (i=0;i<num;i++) {
    /* format the string */
    if (p_counters[i].mad_header.method == VENDOR_GET_RESP) {
      sprintf(buff,"{{port_select %u} {counter_select %u} {port_xmit_data %"PRIu64"} {port_rcv_data %"PRIu64"} {port_xmit_pkts %"PRIu64"} {port_rcv_pkts %"PRIu64"} {port_ucast_xmit_pkts %"PRIu64"} {port_ucast_rcv_pkts %"PRIu64"} {port_mcast_xmit_pkts %"PRIu64"} {port_mcast_rcv_pkts %"PRIu64"} } ",
              p_counters[i].port_select,
              cl_ntoh16(p_counters[i].counter_select),
              cl_ntoh64(p_counters[i].port_xmit_data),
              cl_ntoh64(p_counters[i].port_rcv_data),
              cl_ntoh64(p_counters[i].port_xmit_pkts),
              cl_ntoh64(p_counters[i].port_rcv_pkts),
              cl_ntoh64(p_counters[i].port_ucast_xmit_pkts),
              cl_ntoh64(p_counters[i].port_ucast_rcv_pkts),
              cl_ntoh64(p_counters[i].port_mcast_xmit_pkts),
              cl_ntoh64(p_counters[i].port_mcast_rcv_pkts)
              );
    } else {
      sprintf(buff,"{TARGET_ERROR : Fail to obtain port counters} ");
    }

    if (p_res_str) {
      p_res_str = (char *)realloc(p_res_str,strlen(p_res_str)+strlen(buff) + 1);
    } else {
      p_res_str = (char *)malloc(strlen(buff) + 1);
      p_res_str[0] = '\0';
    }
    strcat(p_res_str, buff);
  }
  return(p_res_str);
}

int
ibpm_get_counters_global(
  uint16_t lid,
  uint8_t port_select,
  char **pp_new_counters_str)
{
  int status;
  ib_pm_port_counter_t single_counters_set;
  *pp_new_counters_str = NULL;
  status = (int) ibpm_get_counters(p_ibpm_global,lid,port_select,&single_counters_set);
  if (status) {
    ibis_set_tcl_error("ERROR : Fail to obtain port counters");
  } else {
    *pp_new_counters_str = ibpm_get_port_counters_str(1, &single_counters_set);
  }
  return(status);
}

int
ibpm_get_multi_counters_global(
   uint8_t num,
   uint16_t lid_list[],
   uint8_t port_select_list[],
   char **pp_new_counters_str)
{
  int status;
  ib_pm_port_counter_t *p_multi_counters_set;
  p_multi_counters_set = (ib_pm_port_counter_t *)malloc(sizeof(ib_pm_port_counter_t)*num);
  *pp_new_counters_str = NULL;

  status = (int) ibpm_get_multi_counters(
    p_ibpm_global,num,lid_list,port_select_list,p_multi_counters_set);
  if (status) {
    ibis_set_tcl_error("ERROR : Fail to obtain multiple port counters");
  } else {
    *pp_new_counters_str = ibpm_get_port_counters_str(num, p_multi_counters_set);
  }
  free(p_multi_counters_set);
  return(status);
}

int
ibpm_get_multi_counters_extended_global(
   uint8_t num,
   uint16_t lid_list[],
   uint8_t port_select_list[],
   char **pp_new_counters_str)
{
  int status;
  ib_pm_port_counter_extended_t *p_multi_counters_set;
  p_multi_counters_set =
    (ib_pm_port_counter_extended_t *)malloc(sizeof(ib_pm_port_counter_t)*num);
  *pp_new_counters_str = NULL;

  status = (int) ibpm_get_multi_counters_extended(
    p_ibpm_global,num,lid_list,port_select_list,p_multi_counters_set);
  if (status) {
    ibis_set_tcl_error("ERROR : Fail to obtain multiple port counters");
  } else {
    *pp_new_counters_str = ibpm_get_port_counters_extended_str(num, p_multi_counters_set);
  }
  free(p_multi_counters_set);
  return(status);
}

int
ibpm_clr_all_counters_global(
  uint16_t lid,
  uint8_t port_select)
{
  int status;
  status = (int) ibpm_clr_all_counters(p_ibpm_global,lid,port_select);
  if (status) {
    ibis_set_tcl_error("ERROR : Fail to clear port counters");
  }
  return(status);
}

int
ibpm_clr_all_multi_counters_global(
  uint8_t num,
  uint16_t lid_list[],
  uint8_t port_select_list[])
{
  int status;
  status = (int) ibpm_clr_all_multi_counters(p_ibpm_global,num,lid_list,port_select_list);

  if (status) {
    ibis_set_tcl_error("ERROR : Fail to clear multiple port counters");
  }
  return(status);
}

%}

//
// STANDARD IB TYPE MAPS:
//

%{
#define  uint16_pm_arr_t  uint16_t
%}
%typemap(tcl8,in) uint16_pm_arr_t *(uint16_t temp[IBPM_MULTI_MAX]) {
    char *str;
    char *str_tcl;
    int i;
    char *loc_buf;
    char *str_token = NULL;

    str_tcl = Tcl_GetStringFromObj($source,NULL);
    loc_buf = (char *)malloc((strlen(str_tcl)+1)*sizeof(char));
    strcpy(loc_buf,str_tcl);

    str = strtok_r(loc_buf," ", &str_token);
    for (i=0;i<IBPM_MULTI_MAX;i++) {
	if (str == NULL) {
	    break;
	}
	temp[i] = atoi(str);
	str = strtok_r(NULL," ",&str_token);
    }
    $target = temp;
    free(loc_buf);
}

%{
#define  uint8_pm_arr_t  uint8_t
%}
%typemap(tcl8,in) uint8_pm_arr_t *(uint8_t temp[IBPM_MULTI_MAX]) {
  char *str;
  char *str_tcl;
  int i;
  char *loc_buf;
  char *str_token = NULL;

  str_tcl = Tcl_GetStringFromObj($source,NULL);
  loc_buf = (char *)malloc((strlen(str_tcl)+1)*sizeof(char));
  strcpy(loc_buf,str_tcl);

  str = strtok_r(loc_buf," ", &str_token);
  for (i=0;i<IBPM_MULTI_MAX;i++) {
    if (str == NULL) {
      break;
    }
    temp[i] = atoi(str);
    str = strtok_r(NULL," ",&str_token);
  }

  $target = temp;
  free(loc_buf);

}

//
// IBPM MAD TYPE MAPS
//

%section "IBPM Functions",pre
/* IBPM UI functions */
%text %{
This section provide the details about the functions IBPM exposes.
They all return 0 on succes.
%}

%name(pmMultiMaxGet) ibpm_num_of_multi_max();

%typemap (tcl8, ignore) char **p_out_str (char *p_c) {
  $target = &p_c;
}

%typemap (tcl8, argout) char **p_out_str {
  Tcl_SetStringObj($target,*$source,strlen(*$source));
  if (*$source) free(*$source);
}

%apply char **p_out_str {char **p_counters_str};

%name(pmGetPortCounters)
     int ibpm_get_counters_global(uint16_t lid,uint8_t port_select, char **p_counters_str);

%name(pmGetPortCountersMulti)
     int ibpm_get_multi_counters_global(uint8_t num, uint16_pm_arr_t lid_list[],uint8_pm_arr_t port_select_list[], char **p_counters_str);

%name(pmGetExtPortCountersMulti)
     int ibpm_get_multi_counters_extended_global(uint8_t num, uint16_pm_arr_t lid_list[],uint8_pm_arr_t port_select_list[], char **p_counters_str);

%name(pmClrAllCounters) int ibpm_clr_all_counters_global(uint16_t lid,uint8_t port_select);

%name(pmClrAllCountersMulti) int ibpm_clr_all_multi_counters_global( uint8_t num,uint16_pm_arr_t lid_list[],uint8_pm_arr_t port_select_list[]);

