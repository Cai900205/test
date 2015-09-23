#!/bin/bash

#--
# Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#--

# fprogbar: File PROGress BAR
#   This script output a progress bar in the terminal width which monitors
#   file size relative to final expected size.
#   This may be used to provide a progress bar by applying to build output
#   log file (when final size of the log file is approx. known)
#   If a process ID is given, the fprogbar will terminate only when the
#   given process ID is gone from the active processes list.
#
# Note: It is recommended to provide final size which is a bit less than
#       expected size, since this size is the termination point

function usage()
{
  echo "usage: fprogbar <filename> <final size in bytes> [<process ID to sync with>]"
  exit 1
}

# Get filename and expected size
if [ $# -lt 2 ] ; then
  usage
fi
file=$1
shift
final_sz=$1
shift
if [ $# -eq 1 ]; then
  pid2track=$1
else
  pid2track=1
fi

# prepare progress bar of style [        ] to be [+++     ]
echo -n "[]"
let num_of_dots=`tput cols`-12
#for (( i= 0 ; i < $num_of_dots ;  )) ; do echo -n -e '\267' ; let i++ ;done
#echo -n -e \]'\r'\[
let bytes_per_dot=$final_sz/$num_of_dots
let cur_bars=0
let percent=0

# Update dots until final size is reached
while ((1)); do
  lead_proc_cnt=`ps -eo "%p" | awk '$1 == '$pid2track' {print}' | wc -l`
  current_sz=`ls -l $file | awk '{print $5}'`
  let req_bars=$current_sz/$bytes_per_dot
  if [ $req_bars -ge $num_of_dots ]; then
    let req_bars=$num_of_dots
  fi
  for ((  ; $cur_bars < $req_bars ; )); do
    let percent=($current_sz*100)/$final_sz
    printf "\b+] %3d%%\b\b\b\b\b" $percent
    let cur_bars++
  done

  # Check if reached expected file size
  if [ \( $cur_bars -eq $num_of_dots \) -o \( $percent -ge 100 \)  ]; then
    echo " 100%"
    exit 0
  fi

  if [ $lead_proc_cnt -eq 0 ]; then
    # probably some failure - but could be that expected file size is not updated
    echo " 100%"
    exit 2
  fi

 usleep 200000
done
