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
#include "ibvs.h"
%}

%{

static ibvs_t *p_ibvs_global;

int
ibvs_num_of_multi_max(void)
{
	return (IBVS_MULTI_MAX);
}

/*
   this function returns the string corresponding to the
   read cpu data
*/
char *
ibvs_get_vs_str(
  boolean_t is_multi,
  boolean_t is_read,
  uint8_t num,
  uint8_t size,
  uint8_t first_data_idx,
  ib_vs_t *p_vs_mads
  )
{
  char *p_res_str = 0;
  char buff[1024];
  int i,j, extra;

  if (p_vs_mads) {
    for (i=0;i<num;i++) {
      boolean_t space_in_resp = TRUE;

      if (p_vs_mads[i].mad_header.method != VENDOR_GET_RESP) {
        sprintf(buff,"TARGET_ERROR : Failed to obtain VS mad response");
      } else if (ibis_get_mad_status((ib_mad_t*)&p_vs_mads[i]) != 0) {
        sprintf(buff,"TARGET_ERROR : Got remote error:0x%x",
                ibis_get_mad_status((ib_mad_t*)&p_vs_mads[i]));
      } else if (is_read) {
        sprintf(buff, "{vendor_key 0x%016" PRIx64 "} ",
                cl_ntoh64(p_vs_mads[i].vendor_key));
        for (j=0; j < size; j++) {
          sprintf(buff,"%s {data%u 0x%x} ",
                  buff, j, cl_ntoh32(p_vs_mads[i].data[j+first_data_idx]));
        }
      } else {
	  // Write response - no data
	  // Note: The trailing space here is important !
	  sprintf(buff, "ACK ");
	  space_in_resp = 0;
      }

      if (is_multi && space_in_resp)
	extra = 3;
      else
	extra = 0;

      if (p_res_str) {
        p_res_str =
          (char *)realloc(p_res_str,strlen(p_res_str)+strlen(buff) + 1+ extra);
      } else {
        p_res_str = (char *)malloc(strlen(buff) + 1+ extra);
        p_res_str[0] = '\0';
      }

      /* need an extra list wrap */
      if (is_multi && space_in_resp) {
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
ibvs_cpu_read_global(
  uint16_t lid,
  uint8_t size,
  uint8_t cpu_traget_size,
  uint32_t address,
  char **pp_new_cpu_str)
{
	ib_api_status_t status;
   ib_vs_t         vs_mads[1];

	status =
     ibvs_cpu_read(p_ibvs_global,lid,size,cpu_traget_size,address,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to obtain port counters");
   } else {
     *pp_new_cpu_str =
       ibvs_get_vs_str(FALSE, TRUE, 1, IBVS_DATA_MAX, VS_CPU_DATA_OFFSET, vs_mads);
   }

   return(status);
}

int
ibvs_cpu_write_global(
  uint16_t lid,
  uint8_t size,
  uint8_t cpu_traget_size,
  uint32_t data[],
  uint32_t address)
{

	ib_api_status_t status;

	status =
     ibvs_cpu_write(p_ibvs_global,lid,size,cpu_traget_size,data,address);
   if (status)
     ibis_set_tcl_error("ERROR : Fail to clear port counters");
   return(status);
}

int
ibvs_i2c_read_global(
  uint16_t lid,
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  char **pp_new_i2c_str)
{
	ib_api_status_t status;
   ib_vs_t         vs_mads[1];

	status =
     ibvs_i2c_read(
       p_ibvs_global,lid,port_num,size,device_id,address,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to obtain port counters");
   } else {
     *pp_new_i2c_str =
       ibvs_get_vs_str(FALSE, TRUE, 1, size / 4, VS_I2C_DATA_OFFSET, vs_mads);
   }

	return(status);
}

int
ibvs_multi_i2c_read_global(
  uint8_t num,
  uint16_t lid_list[],
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  char **pp_new_i2c_str)
{
    ib_api_status_t status;
    ib_vs_t         vs_mads[IBVS_MULTI_MAX];

    status =
	ibvs_multi_i2c_read(p_ibvs_global,num,lid_list,port_num,size,device_id,address,vs_mads);
    if (status) {
	ibis_set_tcl_error("ERROR : Failed reading multiple i2c");
    } else {
	*pp_new_i2c_str =
	    ibvs_get_vs_str(TRUE, TRUE, num, size / 4, VS_I2C_DATA_OFFSET, vs_mads);
    }

    return(status);
}

int
ibvs_multi_i2c_write_global(
  uint8_t num,
  uint16_t lid_list[],
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  uint32_t data[],
  char **pp_new_i2c_str)
{

    ib_api_status_t status;
    ib_vs_t         vs_mads[IBVS_MULTI_MAX];

    status =
	ibvs_multi_i2c_write(p_ibvs_global,num,lid_list,port_num,size,device_id,data,address,vs_mads);

    if (status) {
	ibis_set_tcl_error("ERROR : Failed writing multiple i2c");
    } else {
	*pp_new_i2c_str =
	    ibvs_get_vs_str(TRUE, FALSE, num, size / 4, VS_I2C_DATA_OFFSET, vs_mads);
    }

    return(status);
}

int
ibvs_i2c_write_global(
  uint16_t lid,
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  uint32_t data[])
{

    ib_api_status_t status;

    status = ibvs_i2c_write(p_ibvs_global,lid,port_num,size,device_id,data,address);
    if (status)
	ibis_set_tcl_error("ERROR : Fail to write i2c");
    return(status);
}

int
ibvs_gpio_read_global(
  IN uint16_t lid,
  OUT	char **pp_new_gpio_str)
{
	ib_api_status_t status;
   ib_vs_t         vs_mads[1];

	status = ibvs_gpio_read(p_ibvs_global,lid,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to read gpio");
   } else {
     *pp_new_gpio_str =
       ibvs_get_vs_str(TRUE, TRUE, 1, IBVS_DATA_MAX, VS_GPIO_DATA_OFFSET, vs_mads);
   }
	return(status);
}

int
ibvs_gpio_write_global(
  IN uint16_t lid,
  IN uint64_t gpio_mask,
  IN uint64_t gpio_data)
{
	ib_api_status_t status;

	status = ibvs_gpio_write(p_ibvs_global,lid,gpio_mask,gpio_data );
   if (status)
     ibis_set_tcl_error("ERROR : Fail to write gpio");
	return(status);
}

int
ibvs_multi_sw_reset_global(
  uint8_t num,
  uint16_t lid_list[])
{
	ib_api_status_t status;

	status = ibvs_multi_sw_reset(p_ibvs_global,num,lid_list);
   if (status)
     ibis_set_tcl_error("ERROR : Fail to reset");

	return(status);
}

int
ibvs_multi_flash_open_global(
  uint8_t num,
  uint16_t lid_list[],
  uint32_t last,
  uint8_t size,
  uint32_t address,
  uint32_t data[],
  char **pp_new_flash_str)
{
	ib_api_status_t status;
	ib_vs_t vs_mads[IBVS_MULTI_MAX];

	status =
     ibvs_multi_flash_open(
       p_ibvs_global,num,lid_list,last,size,data,address,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to open flash");
   } else {
     *pp_new_flash_str =
       ibvs_get_vs_str(TRUE, TRUE, num, 4, VS_FLASH_DATA_OFFSET, vs_mads);
   }

	return(status);
}

int
ibvs_multi_flash_close_global(
  uint8_t num,
  uint16_t lid_list[],
  uint32_t force,
  char **pp_new_flash_str)
{
	ib_api_status_t status;
 	ib_vs_t vs_mads[IBVS_MULTI_MAX];

	status =
     ibvs_multi_flash_close(
       p_ibvs_global,num,lid_list,force,vs_mads);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to close flash");
   } else {
     *pp_new_flash_str =
       ibvs_get_vs_str(TRUE, TRUE, num, 4, VS_FLASH_DATA_OFFSET, vs_mads);
   }

	return(status);
}

int
ibvs_multi_flash_set_bank_global(
  uint8_t num,
  uint16_t lid_list[],
  uint32_t address,
  char **pp_new_flash_str)
{
	ib_api_status_t status;
 	ib_vs_t vs_mads[IBVS_MULTI_MAX];

	status =
     ibvs_multi_flash_set_bank(
       p_ibvs_global, num, lid_list, address, vs_mads);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to set flash bank");
   } else {
     *pp_new_flash_str =
       ibvs_get_vs_str(TRUE, TRUE, num, 4, VS_FLASH_DATA_OFFSET, vs_mads);
   }

	return(status);
}

int
ibvs_multi_flash_erase_global(
  uint8_t num,
  uint16_t lid_list[],
  uint32_t address,
  char **pp_new_flash_str)
{
	ib_api_status_t status;
 	ib_vs_t vs_mads[IBVS_MULTI_MAX];

	status =
     ibvs_multi_flash_erase(
       p_ibvs_global, num, lid_list, address, vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to erase flash sector");
   } else {
     *pp_new_flash_str =
       ibvs_get_vs_str(TRUE, TRUE, num, 4, VS_FLASH_DATA_OFFSET, vs_mads);
   }

	return(status);
}

int
ibvs_multi_flash_read_global(
  uint8_t num,
  uint16_t lid_list[],
  uint8_t size,
  uint32_t address,
  char **pp_new_flash_str)
{
	ib_api_status_t status;
 	ib_vs_t vs_mads[IBVS_MULTI_MAX];

	status =
     ibvs_multi_flash_read(
       p_ibvs_global, num, lid_list, size, address, vs_mads);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to read flash");
   } else {
     *pp_new_flash_str =
       ibvs_get_vs_str(TRUE, TRUE, num, size / 4, VS_FLASH_DATA_OFFSET, vs_mads);
   }

	return(status);
}

int
ibvs_multi_flash_write_global(
  uint8_t num,
  uint16_t lid_list[],
  uint8_t size,
  uint32_t address,
  uint32_t data[])
{
	ib_api_status_t status;

	status =
     ibvs_multi_flash_write(
       p_ibvs_global, num, lid_list, size, data, address);

   if (status) {
     ibis_set_tcl_error("ERROR : Fail to write flash");
   }
	return(status);
}

int
ibvs_mirror_read_global(
  IN uint16_t lid,
  OUT	char **pp_new_mirror_str)
{
   ib_api_status_t status;
   ib_vs_t         vs_mads[1];

   status = ibvs_mirror_read(p_ibvs_global,lid,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to read mirror");
   } else {
     *pp_new_mirror_str =
       ibvs_get_vs_str(FALSE, TRUE, 1, IBVS_DATA_MAX, VS_MIRROR_DATA_OFFSET, vs_mads);
   }
	return(status);
}

int
ibvs_mirror_write_global(
  IN uint16_t lid,
  IN uint32_t rx_mirror,
  IN uint32_t tx_mirror)
{
   ib_api_status_t status;

   status = ibvs_mirror_write(p_ibvs_global,lid,rx_mirror,tx_mirror );
   if (status)
     ibis_set_tcl_error("ERROR : Fail to write mirror");
	return(status);
}

int
ibvs_plft_map_get_global(
  IN uint16_t lid,
  IN uint8_t upper_ports,
  OUT	char **pp_new_plft_map_str)
{
   ib_api_status_t status;
   ib_vs_t         vs_mads[1];
	char *          buff;
	ib_vs_plft_map_t *p_map = (ib_vs_plft_map_t *)&vs_mads[0];

   status = ibvs_plft_map_get(p_ibvs_global,lid,upper_ports,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to get private LFT map");
		*pp_new_plft_map_str = NULL;
   } else {
		buff = (char *)malloc(1024);
		*pp_new_plft_map_str = buff;
		sprintf(buff,
				  "{ib_port0 0x%x} "
				  "{shared_plft_port0 0x%x} "
				  "{size0 0x%x} "
				  "{ib_port1 0x%x} "
				  "{shared_plft_port1 0x%x} "
				  "{size1 0x%x} "
				  "{ib_port2 0x%x} "
				  "{shared_plft_port2 0x%x} "
				  "{size2 0x%x} "
				  "{ib_port3 0x%x} "
				  "{shared_plft_port3 0x%x} "
				  "{size3 0x%x} "
				  "{ib_port4 0x%x} "
				  "{shared_plft_port4 0x%x} "
				  "{size4 0x%x} "
				  "{ib_port5 0x%x} "
				  "{shared_plft_port5 0x%x} "
				  "{size5 0x%x} "
				  "{ib_port6 0x%x} "
				  "{shared_plft_port6 0x%x} "
				  "{size6 0x%x} "
				  "{ib_port7 0x%x} "
				  "{shared_plft_port7 0x%x} "
				  "{reserved7 0x%x} "
				  "{size7 0x%x} "
				  "{ib_port8 0x%x} "
				  "{shared_plft_port8 0x%x} "
				  "{size8 0x%x} "
				  "{ib_port9 0x%x} "
				  "{shared_plft_port9 0x%x} "
				  "{reserved9 0x%x} "
				  "{size9 0x%x} "
				  "{ib_port10 0x%x} "
				  "{shared_plft_port10 0x%x} "
				  "{size10 0x%x} "
				  "{ib_port11 0x%x} "
				  "{shared_plft_port11 0x%x} "
				  "{size11 0x%x} "
				  "{ib_port12 0x%x} "
				  "{shared_plft_port12 0x%x} "
				  "{size12 0x%x}",
				  p_map->ib_port0,
				  p_map->shared_plft_port0,
				  p_map->size0,
				  p_map->ib_port1,
				  p_map->shared_plft_port1,
				  p_map->size1,
				  p_map->ib_port2,
				  p_map->shared_plft_port2,
				  p_map->size2,
				  p_map->ib_port3,
				  p_map->shared_plft_port3,
				  p_map->size3,
				  p_map->ib_port4,
				  p_map->shared_plft_port4,
				  p_map->size4,
				  p_map->ib_port5,
				  p_map->shared_plft_port5,
				  p_map->size5,
				  p_map->ib_port6,
				  p_map->shared_plft_port6,
				  p_map->size6,
				  p_map->ib_port7,
				  p_map->shared_plft_port7,
				  p_map->reserved7,
				  p_map->size7,
				  p_map->ib_port8,
				  p_map->shared_plft_port8,
				  p_map->size8,
				  p_map->ib_port9,
				  p_map->shared_plft_port9,
				  p_map->reserved9,
				  p_map->size9,
				  p_map->ib_port10,
				  p_map->shared_plft_port10,
				  p_map->size10,
				  p_map->ib_port11,
				  p_map->shared_plft_port11,
				  p_map->size11,
				  p_map->ib_port12,
				  p_map->shared_plft_port12,
				  p_map->size12
				  );
   }
	return(status);
}

int
ibvs_general_info_get_global(
  IN uint16_t lid,
  OUT	char **pp_gen_info_str)
{
   ib_api_status_t status;
   ib_vs_t         vs_mads[1];
	char *          buff;
	char            psid[17];
	ib_vs_gen_info_t *p_info = (ib_vs_gen_info_t *)&vs_mads[0];
   status = ibvs_general_info_get(p_ibvs_global,lid,vs_mads);
   if (status) {
     ibis_set_tcl_error("ERROR : Fail to get general info");
		*pp_gen_info_str = 0;
   } else {
		buff = (char *)malloc(1024);
		*pp_gen_info_str = buff;
		strncpy(psid, (char *)p_info->fw_psid, 16);
		psid[16] = '\0';
		sprintf(buff,
				  "{hw_rev 0x%x} "
				  "{hw_devid 0x%x} "
				  "{hw_uptime 0x%x} "
				  "{fw_major 0x%x} "
				  "{fw_minor 0x%x} "
				  "{fw_sub_minor 0x%x} "
				  "{fw_build_id 0x%x} "
				  "{fw_month 0x%x} "
				  "{fw_day 0x%x} "
				  "{fw_year 0x%x} "
				  "{fw_hour 0x%x} "
				  "{fw_psid \"%s\"} "
				  "{fw_ini_ver 0x%x} "
				  "{sw_major 0x%x} "
				  "{sw_minor 0x%x} "
				  "{sw_sub_minor 0x%x}",
				  cl_ntoh16(p_info->hw_rev),
				  cl_ntoh16(p_info->hw_devid),
				  cl_ntoh32(p_info->hw_uptime),
				  p_info->fw_major,
				  p_info->fw_minor,
				  p_info->fw_sub_minor,
				  cl_ntoh32(p_info->fw_build_id),
				  p_info->fw_month,
				  p_info->fw_day,
				  cl_ntoh16(p_info->fw_year),
				  cl_ntoh16(p_info->fw_hour),
				  psid,
				  cl_ntoh32(p_info->fw_ini_ver),
				  p_info->sw_major,
				  p_info->sw_minor,
				  p_info->sw_sub_minor);
   }
	return(status);
}

%}

%{
#define uint16_vs_arr_t uint16_t
%}
%typemap(tcl8,in) uint16_vs_arr_t *(uint16_t temp[IBVS_MULTI_MAX]) {
  char *str;
  char *str_tcl;
  int i;
  char *loc_buf;
  char *str_token = NULL;

  str_tcl = Tcl_GetStringFromObj($source,NULL);
  loc_buf = (char *)malloc((strlen(str_tcl)+1)*sizeof(char));
  strcpy(loc_buf,str_tcl);

  str = strtok_r(loc_buf," ", &str_token);
  for (i=0;i<IBVS_MULTI_MAX;i++) {
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
#define uint32_vs_data_arr_t uint32_t
%}
%typemap(tcl8,in) uint32_vs_data_arr_t *(uint32_t temp[IBVS_DATA_MAX]) {
    char *str;
    char *str_tcl;
    int i;
    char *loc_buf;
    char *str_token = NULL;

    str_tcl = Tcl_GetStringFromObj($source,NULL);
    loc_buf = (char *)malloc((strlen(str_tcl)+1)*sizeof(char));
    strcpy(loc_buf,str_tcl);
    str = strtok_r(loc_buf," ", &str_token);
    for (i=0;i<IBVS_DATA_MAX;i++) {
	if (str == NULL) {
	    break;
	}
	temp[i] = (uint32_t)strtoll(str, (char **)NULL, 0);
	str = strtok_r(NULL," ",&str_token);
    }
    $target = temp;
    free(loc_buf);
}

//
// IBVS MAD TYPE MAPS
//

%section "IB Vendor Specific Functions",pre
/* IBVS UI functions */
%text %{
This section provide the details about the functions IBVS exposes.
They all return 0 on succes.
%}

%name(vsMultiMaxGet) ibvs_num_of_multi_max();

%typemap (tcl8, ignore) char **p_out_str (char *p_c) {
  $target = &p_c;
}

%typemap (tcl8, argout) char **p_out_str {
  Tcl_SetStringObj($target,*$source,strlen(*$source));
  if (*$source) free(*$source);
}

%apply char **p_out_str {char **pp_new_cpu_str};

%name(vsCpuRead) int ibvs_cpu_read_global(
  uint16_t lid,
  uint8_t size,
  uint8_t cpu_traget_size,
  uint32_t address,
  char **pp_new_cpu_str);

%name(vsCpuWrite) int ibvs_cpu_write_global(
  uint16_t lid,
  uint8_t size,
  uint8_t cpu_traget_size,
  uint32_vs_data_arr_t data[],
  uint32_t address);

%apply char **p_out_str {char **pp_new_i2c_str};

%name(vsI2cRead) int ibvs_i2c_read_global(
  uint16_t lid,
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  char **pp_new_i2c_str);

%name(vsI2cWrite) int ibvs_i2c_write_global(
  uint16_t lid,
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  uint32_vs_data_arr_t data[]);

%name(vsI2cReadMulti) int ibvs_multi_i2c_read_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  char **pp_new_i2c_str);

%name(vsI2cWriteMulti) int ibvs_multi_i2c_write_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint8_t port_num,
  uint8_t device_id,
  uint8_t size,
  uint32_t address,
  uint32_vs_data_arr_t data[],
  char **pp_new_i2c_str);

%apply char **p_out_str {char **pp_new_gpio_str};

%name(vsGpioRead) int ibvs_gpio_read_global(
  uint16_t lid,
  char **pp_new_gpio_str);

%name(vsGpioWrite) int ibvs_gpio_write_global(
  uint16_t lid,
  uint64_t gpio_mask,
  uint64_t gpio_data);

%name(vsSWReset) int ibvs_multi_sw_reset_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[]);

%apply char **p_out_str {char **pp_new_flash_str};

%name(vsFlashStartMulti) int ibvs_multi_flash_open_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint32_t last,
  uint8_t size,
  uint32_t address,
  uint32_vs_data_arr_t data[],
  char **pp_new_flash_str);

%name(vsFlashStopMulti) int ibvs_multi_flash_close_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint32_t force,
  char **pp_new_flash_str);

%name(vsFlashSetBankMulti) int ibvs_multi_flash_set_bank_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint32_t address,
  char **pp_new_flash_str);

%name(vsFlashEraseSectorMulti) int ibvs_multi_flash_erase_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint32_t address,
  char **pp_new_flash_str);

%name(vsFlashReadSectorMulti) int ibvs_multi_flash_read_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint8_t size,
  uint32_t address,
  char **pp_new_flash_str);

%name(vsFlashWriteSectorMulti) int ibvs_multi_flash_write_global(
  uint8_t num,
  uint16_vs_arr_t lid_list[],
  uint8_t size,
  uint32_t address,
  uint32_vs_data_arr_t data[]);

%name(vsMirrorRead) int ibvs_mirror_read_global(
  uint16_t lid,
  char **pp_new_gpio_str);

%name(vsMirrorWrite) int ibvs_mirror_write_global(
  uint16_t lid,
  uint32_t rx_mirror,
  uint32_t tx_mirror);

%apply char **p_out_str {char **pp_gen_info_str};

%name(vsGetGeneralInfo) int ibvs_general_info_get_global(
	uint16_t lid,
	char **pp_gen_info_str);

%apply char **p_out_str {char **pp_new_plft_map_str};

%name(vsGetPrivateLFTMap) int ibvs_plft_map_get_global(
	uint16_t lid,
	uint8_t upper_ports,
	char **pp_new_plft_map_str);
