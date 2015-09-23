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
#include "ibbbm.h"
%}

%{

static ibbbm_t *p_ibbbm_global;

int
ibbbm_read_vpd_global(
  uint16_t lid,
  uint8_t vpd_device_selector,
  uint16_t bytes_num,
  uint16_t offset,
  ib_bbm_vpd_t *p_bbm_vpd_mad)
{
	ib_api_status_t status;

	status = ibbbm_read_vpd(p_ibbbm_global,lid,vpd_device_selector,bytes_num,offset,p_bbm_vpd_mad);
	;
	ibis_set_tcl_error("-E- Failed to read VPD");

	return(status);
}

int
ibbbm_write_vpd_global(
  uint16_t lid,
  uint8_t vpd_device_selector,
  uint16_t bytes_num,
  uint16_t offset,
  uint8_t *p_data)
{

	ib_api_status_t status;

	status = ibbbm_write_vpd(p_ibbbm_global,lid,vpd_device_selector,bytes_num,offset,p_data);
	;
	ibis_set_tcl_error("-E- Failed to write VPD");
	return(status);
}

int
ibbbm_read_vsd_vpd_global(
  uint16_t lid,
  ib_bbm_vsd_vpd_t *p_bbm_vsd_vpd_mad)
{
	ib_api_status_t status;
	ib_bbm_vpd_t *p_bbm_vpd_mad;

	p_bbm_vpd_mad = (ib_bbm_vpd_t *)p_bbm_vsd_vpd_mad;

	status = ibbbm_read_vpd(p_ibbbm_global,lid,IBBBM_VSD_VPD_DEV_SEL,IBBBM_VSD_VPD_SIZE,IBBBM_VSD_VPD_OFFSET,p_bbm_vpd_mad);
	;
	ibis_set_tcl_error("-E- Failed to read VSD VPD");
   return(status);
}

int
ibbbm_read_bsn_vpd_global(
  uint16_t lid,
  ib_bbm_bsn_vpd_t *p_bbm_bsn_vpd_mad)
{
	ib_api_status_t status;
	ib_bbm_vpd_t *p_bbm_vpd_mad;

	p_bbm_vpd_mad = (ib_bbm_vpd_t *)p_bbm_bsn_vpd_mad;

	status = ibbbm_read_vpd(p_ibbbm_global,lid,IBBBM_BSN_VPD_DEV_SEL,IBBBM_BSN_VPD_SIZE,IBBBM_BSN_VPD_OFFSET,p_bbm_vpd_mad);
	;
	ibis_set_tcl_error("-E- Failed to read BSN");

	return(status);
}

int
ibbbm_read_mod_vpd_global(
  uint16_t lid,
  ib_bbm_mod_vpd_t *p_bbm_mod_vpd_mad)
{
	ib_api_status_t status;
	ib_bbm_vpd_t *p_bbm_vpd_mad;

	p_bbm_vpd_mad = (ib_bbm_vpd_t *)p_bbm_mod_vpd_mad;

	status = ibbbm_read_vpd(p_ibbbm_global,lid,IBBBM_MOD_VPD_DEV_SEL,IBBBM_MOD_VPD_SIZE,IBBBM_MOD_VPD_OFFSET,p_bbm_vpd_mad);
	;
	ibis_set_tcl_error("-E- Failed to read Module VPD");

	return(status);
}

int
ibbbm_read_cha_vpd_global(
  uint16_t lid,
  ib_bbm_cha_vpd_t *p_bbm_cha_vpd_mad)
{
	ib_api_status_t status;
	ib_bbm_vpd_t *p_bbm_vpd_mad;

	p_bbm_vpd_mad = (ib_bbm_vpd_t *)p_bbm_cha_vpd_mad;

	status = ibbbm_read_vpd(p_ibbbm_global,lid,IBBBM_CHA_VPD_DEV_SEL,IBBBM_CHA_VPD_SIZE,IBBBM_CHA_VPD_OFFSET,p_bbm_vpd_mad);
	;
   ibis_set_tcl_error("-E- Failed to read Chassis VPD");
	return(status);
}


int
ibbbm_read_fw_ver_vpd_global(
  uint16_t lid,
  ib_bbm_fw_ver_vpd_t *p_bbm_fw_ver_vpd_mad)
{
	ib_api_status_t status;
	ib_bbm_vpd_t *p_bbm_vpd_mad;

	p_bbm_vpd_mad = (ib_bbm_vpd_t *)p_bbm_fw_ver_vpd_mad;

	status = ibbbm_read_vpd(p_ibbbm_global,lid,IBBBM_FW_VER_VPD_DEV_SEL,IBBBM_FW_VER_VPD_SIZE,IBBBM_FW_VER_VPD_OFFSET,p_bbm_vpd_mad);
	;

   ibis_set_tcl_error("-E- Failed to read FW Version.");

	return(status);
}

%}

//
// IBBBM MAD TYPE MAPS
//

%typemap(tcl8,argout) ib_bbm_vpd_t *OUTPUT {
  if ($source) {
         static char buff[1624];
	 static int i;
	 sprintf(buff, "{b_key 0x%016" PRIx64 "} {bm_sequence 0x%x } {device_sel 0x%x } {bytes_num 0x%x } {offset 0x%x }",
                        cl_ntoh64($source->b_key),
                        cl_ntoh16($source->bm_sequence),
			($source->vpd_device_selector),
			cl_ntoh16($source->bytes_num),
			cl_ntoh16($source->offset));
         for (i=0;i<cl_ntoh16($source->bytes_num);i++) {
         	sprintf(buff,"%s {data%u 0x%x} ",buff,i,$source->data[i]);
         };

         Tcl_SetStringObj($target, buff, strlen(buff));
  } else {
         Tcl_SetStringObj($target, "", 0);
  }
}

%typemap(tcl8,ignore) ib_bbm_vpd_t *OUTPUT(ib_bbm_vpd_t temp) {
  $target = &temp;
}

%typemap(tcl8,argout) ib_bbm_mod_vpd_t *OUTPUT {
  if ($source) {
         static char buff[1624];
	 static int i;
	 sprintf(buff, "{b_key 0x%016" PRIx64 "} {bm_sequence 0x%x } {device_sel 0x%x } {bytes_num 0x%x } {offset 0x%x } {temp_sensor_count 0x%x }",
                        cl_ntoh64($source->b_key),
                        cl_ntoh16($source->bm_sequence),
			($source->vpd_device_selector),
			cl_ntoh16($source->bytes_num),
			cl_ntoh16($source->offset),
			$source->temp_sensor_count);

         for (i=0;i<IBBBM_MOD_VPD_TEMP_SIZE;i++) {
         	sprintf(buff,"%s {temp%u 0x%x} ",buff,i+1,cl_ntoh16($source->temp_sensor_record[i]));
         };

	 sprintf(buff,"%s {power_sup_count 0x%x} ",buff,$source->power_sup_count);

	 for (i=0;i<IBBBM_MOD_VPD_PWR_SIZE;i++) {
         	sprintf(buff,"%s {power%u 0x%x} ",buff,i+1,cl_ntoh32($source->power_sup_record[i]));
         };

         Tcl_SetStringObj($target, buff, strlen(buff));
  } else {
         Tcl_SetStringObj($target, "", 0);
  }
}

%typemap(tcl8,ignore) ib_bbm_mod_vpd_t *OUTPUT(ib_bbm_mod_vpd_t temp) {
  $target = &temp;
}

%typemap(tcl8,argout) ib_bbm_cha_vpd_t *OUTPUT {
  if ($source) {
         static char buff[1624];
	 static int i;
	 sprintf(buff, "{b_key 0x%016" PRIx64 "} {bm_sequence 0x%x } {device_sel 0x%x } {bytes_num 0x%x } {offset 0x%x } {temp_sensor_count 0x%x }",
                        cl_ntoh64($source->b_key),
                        cl_ntoh16($source->bm_sequence),
			($source->vpd_device_selector),
		        cl_ntoh16($source->bytes_num),
			cl_ntoh16($source->offset),
			$source->temp_sensor_count);

	 for (i=0;i<IBBBM_CHA_VPD_TEMP_SIZE;i++) {
         	sprintf(buff,"%s {temp%u 0x%x} ",buff,i+1,cl_ntoh16($source->temp_sensor_record[i]));
         };

	 sprintf(buff,"%s {power_sup_count 0x%x} ",buff,$source->power_sup_count);

	 for (i=0;i<IBBBM_CHA_VPD_PWR_SIZE;i++) {
         	sprintf(buff,"%s {power%u 0x%x} ",buff,i+1,cl_ntoh32($source->power_sup_record[i]));
         };

	 sprintf(buff,"%s {fan_count 0x%x} ",buff,$source->fan_count);

   	 for (i=0;i<IBBBM_CHA_VPD_FAN_SIZE;i++) {
         	sprintf(buff,"%s {fan%u 0x%x} ",buff,i+1,cl_ntoh16($source->fan_record[i]));
         };

         Tcl_SetStringObj($target, buff, strlen(buff));
  } else {
         Tcl_SetStringObj($target, "", 0);
  }
}

%typemap(tcl8,ignore) ib_bbm_cha_vpd_t *OUTPUT(ib_bbm_cha_vpd_t temp) {
  $target = &temp;
}

%typemap(tcl8,argout) ib_bbm_bsn_vpd_t *OUTPUT {
  if ($source) {
         static char buff[512];
	 static int j;
	 sprintf(buff, "{b_key 0x%016" PRIx64 "} {bm_sequence 0x%x} {device_sel 0x%x} {bytes_num 0x%x} {offset 0x%x}",
                        cl_ntoh64($source->b_key),
                        cl_ntoh16($source->bm_sequence),
			($source->vpd_device_selector),
		        cl_ntoh16($source->bytes_num),
			cl_ntoh16($source->offset));
	 for (j=0;j<IBBBM_BSN_VPD_SIZE;j++) {
		sprintf(buff,"%s {bsn%u 0x%x} ",buff,j,$source->bsn[j]);
	 };

         Tcl_SetStringObj($target, buff, strlen(buff));
  } else {
         Tcl_SetStringObj($target, "", 0);
  }
}

%typemap(tcl8,ignore) ib_bbm_bsn_vpd_t *OUTPUT(ib_bbm_bsn_vpd_t temp) {
  $target = &temp;
}

%typemap(tcl8,argout) ib_bbm_vsd_vpd_t *OUTPUT {
  if ($source) {
         static char buff[512];
	 static int j;
	 sprintf(buff, "{b_key 0x%016" PRIx64 "} {bm_sequence 0x%x} {device_sel 0x%x} {bytes_num 0x%x} {offset 0x%x}",
                        cl_ntoh64($source->b_key),
                        cl_ntoh16($source->bm_sequence),
			($source->vpd_device_selector),
		        cl_ntoh16($source->bytes_num),
			cl_ntoh16($source->offset));
	 for (j=0;j<IBBBM_VSD_VPD_SIZE;j++) {
		sprintf(buff,"%s {vsd%u 0x%x} ",buff,j,$source->vsd[j]);
	 };

         Tcl_SetStringObj($target, buff, strlen(buff));
  } else {
         Tcl_SetStringObj($target, "", 0);
  }
}

%typemap(tcl8,ignore) ib_bbm_vsd_vpd_t *OUTPUT(ib_bbm_vsd_vpd_t temp) {
  $target = &temp;
}

%typemap(tcl8,argout) ib_bbm_fw_ver_vpd_t *OUTPUT {
  if ($source) {
         static char buff[512];
	 sprintf(buff, "{b_key 0x%016" PRIx64 "} {bm_sequence 0x%x} {device_sel 0x%x} {bytes_num 0x%x} {offset 0x%x} {maj_fw_ver 0x%x} {min_fw_ver 0x%x} {sub_min_fw_ver 0x%x}",
                        cl_ntoh64($source->b_key),
                        cl_ntoh16($source->bm_sequence),
			($source->vpd_device_selector),
		        cl_ntoh16($source->bytes_num),
			cl_ntoh16($source->offset),
			$source->maj_fw_ver,
			$source->min_fw_ver,
			$source->sub_min_fw_ver);

         Tcl_SetStringObj($target, buff, strlen(buff));
  } else {
         Tcl_SetStringObj($target, "", 0);
  }
}

%typemap(tcl8,ignore) ib_bbm_fw_ver_vpd_t *OUTPUT(ib_bbm_fw_ver_vpd_t temp) {
  $target = &temp;
}


%section "IBBBM Functions",pre
/* IBBBM UI functions */
%text %{
This section provide the details about the functions IBBBM exposes.
They all return 0 on succes.
%}

%apply ib_bbm_vpd_t *OUTPUT {ib_bbm_vpd_t *p_bbm_vpd_mad};
%apply ib_bbm_mod_vpd_t *OUTPUT {ib_bbm_mod_vpd_t *p_bbm_mod_vpd_mad};
%apply ib_bbm_cha_vpd_t *OUTPUT {ib_bbm_cha_vpd_t *p_bbm_cha_vpd_mad};
%apply ib_bbm_bsn_vpd_t *OUTPUT {ib_bbm_bsn_vpd_t *p_bbm_bsn_vpd_mad};
%apply ib_bbm_vsd_vpd_t *OUTPUT {ib_bbm_vsd_vpd_t *p_bbm_vsd_vpd_mad};
%apply ib_bbm_fw_ver_vpd_t *OUTPUT {ib_bbm_fw_ver_vpd_t *p_bbm_fw_ver_vpd_mad};

%name(bbmVpdRead) int ibbbm_read_vpd_global(uint16_t lid, uint8_t vpd_device_selector, uint16_t bytes_num, uint16_t offset,ib_bbm_vpd_t *p_bbm_vpd_mad);

%name(bbmVpdWrite) int ibbbm_write_vpd_global(uint16_t lid, uint8_t vpd_device_selector, uint16_t bytes_num, uint16_t offset, uint8_t *p_data);

%name(bbmVSDRead) int ibbbm_read_bsn_vpd_global(uint16_t lid, ib_bbm_bsn_vpd_t *p_bbm_bsn_vpd_mad);

%name(bbmBSNRead) int ibbbm_read_bsn_vpd_global(uint16_t lid, ib_bbm_bsn_vpd_t *p_bbm_bsn_vpd_mad);

%name(bbmModRead) int ibbbm_read_mod_vpd_global(uint16_t lid, ib_bbm_mod_vpd_t *p_bbm_mod_vpd_mad);

%name(bbmChaRead) int ibbbm_read_cha_vpd_global(uint16_t lid, ib_bbm_cha_vpd_t *p_bbm_cha_vpd_mad);

%name(bbmFWVerRead) int ibbbm_read_fw_ver_vpd_global(uint16_t lid, ib_bbm_fw_ver_vpd_t *p_bbm_fw_ver_vpd_mad);

