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

%typemap(tcl8,in) uint64_t *(uint64_t temp) {
  temp = strtoull(Tcl_GetStringFromObj($source,NULL), NULL,16);
  $target = &temp;
}

%typemap(tcl8,out) uint64_t *, uint64_t {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}
%{
#define new_uint64_t uint64_t
%}
%typemap(tcl8,out) new_uint64_t *, new_uint64_t {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
  free( $source );
}

%typemap(tcl8,in) uint32_t * (uint32_t temp){
  temp = strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0);
  $target = &temp;
}

%typemap(tcl8,out) uint32_t * {
  char buff[20];
  sprintf(buff, "%u", *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) uint16_t * (uint16_t temp) {
  temp = strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0);
  $target = &temp;
}

%typemap(tcl8,out) uint16_t * {
  char buff[20];
  sprintf(buff, "%u", *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) uint8_t * (uint8_t temp) {
  temp = strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0);
  $target = &temp;
}

%typemap(tcl8,out) uint8_t * {
  char buff[20];
  sprintf(buff, "%u", *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) ib_net64_t *(uint64_t temp) {
  temp = cl_hton64(strtoull(Tcl_GetStringFromObj($source,NULL), NULL, 16));
  $target = &temp;
}

%typemap(tcl8,out) ib_net64_t * {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, cl_ntoh64(*$source));
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) ib_net32_t *(ib_net32_t temp) {
  temp = cl_hton32(strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0));
  $target = &temp;
}

%typemap(tcl8,out) ib_net32_t * {
  char buff[20];
  sprintf(buff, "%u", cl_ntoh32(*$source));
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,in) ib_net16_t * (ib_net16_t temp) {
  temp = cl_hton16(strtoul(Tcl_GetStringFromObj($source,NULL), NULL, 0));
  $target = &temp;
}

%typemap(tcl8,out) ib_net16_t * {
  char buff[20];
  sprintf(buff, "%u", cl_hton16(*$source));
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,argout) uint64_t *OUTPUT {
  char buff[20];
  sprintf(buff, "0x%016" PRIx64, *$source);
  Tcl_SetStringObj($target,buff,strlen(buff));
}

%typemap(tcl8,ignore) uint64_t *OUTPUT(uint64_t temp) {
  $target = &temp;
}

%typemap(tcl8,out) boolean_t * {
  if (*$source) {
	 Tcl_SetStringObj($target,"TRUE", 4);
  } else {
	 Tcl_SetStringObj($target,"FALSE", 5);
  }
}

%typemap(tcl8,in) boolean_t *(boolean_t temp) {
  if (strcmp(Tcl_GetStringFromObj($source,NULL), "TRUE")) {
	 temp = FALSE;
  } else {
	 temp = TRUE;
  }
  $target = &temp;
}

%typemap (tcl8, ignore) char **p_out_str (char *p_c) {
  $target = &p_c;
}

%typemap (tcl8, argout) char **p_out_str {
  Tcl_SetStringObj($target,*$source,strlen(*$source));
  if (*$source) free(*$source);
}

/* handle char arrays as members of a struct */
%typemap (tcl8, memberin) char [ANY] {
  strncpy($target,$source,$dim0 - 1);
  $target[$dim0 - 1] = '\0';
}

%typemap(tcl8,out) ib_gid_t* {
  char buff[38];
  sprintf(buff, "0x%016" PRIx64 ":0x%016" PRIx64,
          cl_ntoh64($source->unicast.prefix),
          cl_ntoh64($source->unicast.interface_id)
          );
  Tcl_SetStringObj($target,buff,strlen(buff));
}

/* ---------------- handling array of uint8_t ---------------------- */
%{
#define uint8_array_t uint8_t
%}
%typemap(memberin) uint8_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(in) uint8_array_t[ANY] (uint8_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  long int entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtol(p_ch, NULL, 0);
    if (entry > 0xff)
    {
      printf("Error: wrong format or out of range value for expected uint8_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = entry;
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) uint8_array_t[ANY] {
  int i;
  char buff[8];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%02x ", *($source+i));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

/* ---------------- handling array of uint16_t ---------------------- */
%{
#define uint16_array_t uint16_t
%}
%typemap(memberin) uint16_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(in) uint16_array_t[ANY] (uint16_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  long int entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtol(p_ch, NULL, 0);
    if (entry > 0xffff)
    {
      printf("Error: wrong format or out of range value for expected uint16_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = entry;
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) uint16_array_t[ANY] {
  int i;
  char buff[8];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%04x ", *($source+i));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

/* ---------------- handling array of uint32_t ---------------------- */
%{
#define uint32_array_t uint32_t
%}
%typemap(memberin) uint32_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(in) uint32_array_t[ANY] (uint32_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  long int entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtol(p_ch, NULL, 0);
    if (entry > 0xffffffff)
    {
      printf("Error: wrong format or out of range value for expected uint32_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = entry;
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) uint32_array_t[ANY] {
  int i;
  char buff[12];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%08x ", *($source+i));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

/* ---------------- handling array of uint64_t ---------------------- */
%{
#define uint64_array_t uint64_t
%}
%typemap(memberin) uint64_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
		$target[i] = *($source+i);
	}
}

%typemap(in) uint64_array_t[ANY] (uint64_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  uint64_t entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtol(p_ch, NULL, 0);
    if (entry > 0xffffffffffffffffULL )
    {
      printf("Error: wrong format or out of range value for expected uint64_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = entry;
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) uint64_array_t[ANY] {
  int i;
  char buff[20];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%016 " PRIx64 " ", *($source+i));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

/* ---------------- handling array of ib_net16_t ---------------------- */
%{
#define ib_net16_array_t ib_net16_t
%}

/* note - no need for cl_hton since the "in" typemap will do */
%typemap(memberin) ib_net16_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
     $target[i] = *($source+i);
	}
}

%typemap(in) ib_net16_array_t[ANY] (ib_net16_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  long int entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtol(p_ch, NULL, 0);
    if (entry > 65535)
    {
      printf("Error: wrong format or out of range value for expected ib_net16_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = cl_hton16(entry);
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) ib_net16_array_t[ANY] {
  int i;
  char buff[8];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%04x ", cl_ntoh16(*($source+i)));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

/* ---------------- handling array of ib_net32_t ---------------------- */
%{
#define ib_net32_array_t ib_net32_t
%}

/* note - no need for cl_hton since the "in" typemap will do */
%typemap(memberin) ib_net32_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
     $target[i] = *($source+i);
	}
}

%typemap(in) ib_net32_array_t[ANY] (ib_net32_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  uint32_t entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtol(p_ch, NULL, 0);
    if (entry > 0xffffffff)
    {
      printf("Error: wrong format or out of range value for expected ib_net32_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = cl_hton32(entry);
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) ib_net32_array_t[ANY] {
  int i;
  char buff[12];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%08x ", cl_ntoh32(*($source+i)));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

/* ---------------- handling array of ib_net64_t ---------------------- */
%{
#define ib_net64_array_t ib_net64_t
%}

/* note - no need for cl_hton since the "in" typemap will do */
%typemap(memberin) ib_net64_array_t[ANY] {
	int i;
	for (i=0; i <$dim0 ; i++) {
     $target[i] = *($source+i);
	}
}

%typemap(in) ib_net64_array_t[ANY] (ib_net64_t entrys[$dim0]) {
  char *buff;
  char *p_ch;
  char *last;
  uint64_t entry;

  int i = 0;
  buff = (char *)malloc((strlen(Tcl_GetStringFromObj($source,NULL))+1)*sizeof(char));
  strcpy(buff, Tcl_GetStringFromObj($source,NULL));
  p_ch = strtok_r(buff, " \t",&last);
  while (p_ch && (i < $dim0))
  {
    entry = strtoll(p_ch, NULL, 0);
    if (entry > 0xffffffffffffffffULL )
    {
      printf("Error: wrong format or out of range value for expected ib_net64_t entry: %s\n", p_ch);
      return TCL_ERROR;
    }
    entrys[i++] = cl_hton64(entry);
    p_ch = strtok_r(NULL, " \t", &last);
  }
  for (; i < $dim0; i++) entrys[i] = 0;

  free(buff);
  $target = entrys;
}

%typemap(tcl8, out) ib_net64_array_t[ANY] {
  int i;
  char buff[20];
  for (i=0; i <$dim0 ; i++) {
    sprintf(buff, "0x%016" PRIx64 " ", cl_ntoh64(*($source+i)));
    Tcl_AppendResult(interp, buff, NULL);
  }
}

%include typemaps.i


